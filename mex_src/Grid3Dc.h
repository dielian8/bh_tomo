/*
 *  Grid3Dc.h
 *
 *  Created by Bernard Giroux on 08-04-24.
 *
 *  Modified by Benoit Larouche on 12-07-20
 *  	: now support parallel raytracing from many source points
 *  	  on the same 3D grid simultaneously, using OpenMP.
 *  	  Secondary nodes are placed on every edge and face of the grid cells.
 *
 *  	  The velocity model is sampled for each cell and is constant inside the
 *        cell.
 *
 *
 */

/*
 * Copyright (C) 2012 Bernard Giroux, Benoît Larouche.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __GRID3DC_H__
#define __GRID3DC_H__


#include <algorithm>
#include <cstring>
#include <iostream>
#include <fstream>
#include <queue>
#include <vector>
#include <ctime>

//#include <ext/pb_ds/priority_queue.hpp>

#include "Grid3D.h"
#include "Node3Dc.h"


template<typename T1, typename T2>
class Grid3Dc : public Grid3D<T1,T2> {
public:
    Grid3Dc(const T2 nx, const T2 ny, const T2 nz,
			const T1 ddx, const T1 ddy, const T1 ddz,
			const T1 minx, const T1 miny, const T1 minz,
			const T2 nnx, const T2 nny, const T2 nnz,
			const int nt=1);
    
    ~Grid3Dc() {}
    
    T1 getSlowness(const size_t n) const { return slowness[n]; }
    size_t getNumberOfNodes() const { return nodes.size(); }
    
    void setSlowness(const T1 s) {
        for ( size_t n=0; n<slowness.size(); ++n ) {
            slowness[n] = s;
        }
    }
    
    int setSlowness(const T1 *s, const size_t ns) {
        if ( slowness.size() != ns ) {
            std::cerr << "Error: slowness vectors of incompatible size.";
            return 1;
        }
        for ( size_t n=0; n<slowness.size(); ++n ) {
            slowness[n] = s[n];
        }
        return 0;
    }
    
    int setSlowness(const std::vector<T1>& s) {
        if ( slowness.size() != s.size() ) {
            std::cerr << "Error: slowness vectors of incompatible size.";
            return 1;
        }
        for ( size_t n=0; n<slowness.size(); ++n ) {
            slowness[n] = s[n];
        }
        return 0;
    }
    
    int raytrace(const std::vector<sxyz<T1> >& Tx,
                 const std::vector<T1>& t0,
                 const std::vector<sxyz<T1> >& Rx,
                 std::vector<T1>& traveltimes,
                 const int threadNo=0) const;
	
    int raytrace(const std::vector<sxyz<T1> >& Tx,
                 const std::vector<T1>& t0,
                 const std::vector<sxyz<T1> >& Rx,
                 std::vector<T1>& traveltimes,
				 std::vector<std::vector<sxyz<T1> > >& r_data,
                 const int threadNo=0) const;
	
    int raytrace(const std::vector<sxyz<T1> >& Tx,
                 const std::vector<T1>& t0,
                 const std::vector<sxyz<T1> >& Rx,
                 std::vector<T1>& traveltimes,
				 std::vector<std::vector<siv<T1> > >& l_data,
				 std::vector<std::vector<sxyz<T1> > >& r_data,
                 const int threadNo=0) const;
	
	int raytrace2(const std::vector<sxyz<T1> >& Tx,
				  const std::vector<T1>& t0,
				  const std::vector<sxyz<T1> >& Rx,
				  std::vector<T1>& traveltimes,
				  const int threadNo=0) const;
	
    
    void saveSlownessXYZ(const char filename[]) const {
        std::ofstream fout( filename );
        
        for ( size_t k=0, n=0; k<Grid3D<T1,T2>::nCellz; ++k ) {
            T1 z = Grid3D<T1,T2>::zmin + (0.5+k)*Grid3D<T1,T2>::dz;
            for ( size_t j=0; j<Grid3D<T1,T2>::nCelly; ++j ) {
            	T1 y = Grid3D<T1,T2>::ymin + (0.5+j)*Grid3D<T1,T2>::dy;
            	for ( size_t i=0; i<Grid3D<T1,T2>::nCellx; ++i, ++n ) {
            		T1 x = Grid3D<T1,T2>::xmin + (0.5+i)*Grid3D<T1,T2>::dx;
            		fout << x << "   " << y << "   " << z << "   "
					<< slowness[n] << '\n';
            	}
            }
        }
        fout.close();
    }
    
    size_t getCellNo(const sxyz<T1>& pt) const {
        size_t nx = static_cast<size_t>( small + (pt.x-Grid3D<T1,T2>::xmin)/Grid3D<T1,T2>::dx );
        size_t ny = static_cast<size_t>( small + (pt.y-Grid3D<T1,T2>::ymin)/Grid3D<T1,T2>::dy );
        size_t nz = static_cast<size_t>( small + (pt.z-Grid3D<T1,T2>::zmin)/Grid3D<T1,T2>::dz );
        return ny*Grid3D<T1,T2>::nCellx + nz*(Grid3D<T1,T2>::nCellx*Grid3D<T1,T2>::nCelly) + nx;
    }
	
    size_t getCellNo(const Node3Dc<T1,T2>& node) const {
        size_t nx = static_cast<size_t>( small + (node.getX()-Grid3D<T1,T2>::xmin)/Grid3D<T1,T2>::dx );
        size_t ny = static_cast<size_t>( small + (node.getY()-Grid3D<T1,T2>::ymin)/Grid3D<T1,T2>::dy );
        size_t nz = static_cast<size_t>( small + (node.getZ()-Grid3D<T1,T2>::zmin)/Grid3D<T1,T2>::dz );
        return ny*Grid3D<T1,T2>::nCellx + nz*(Grid3D<T1,T2>::nCellx*Grid3D<T1,T2>::nCelly) + nx;
    }
	
    void save(const char filename[]) const;
    void dsave(const char filename[]) const;
    void savefast(const char filename[]) const;
	void savePrimary(const char filename[], const int nt=0) const;
    
    size_t getSlownessSize() const {
        return slowness.size()*sizeof(T1);
    }
    size_t getNeighborsSize() const {
        size_t n_elem = 0;
        for ( size_t n=0; n<neighbors.size(); ++n ) {
            n_elem += neighbors[n].size();
        }
        return n_elem*sizeof(size_t);
    }
    size_t getNodesSize() const {
        size_t size = 0;
        for ( size_t n=0; n<nodes.size(); ++n ) {
            size += nodes[n].getSize();
        }
        return size;
    }
	
private:
	
    mutable std::vector<Node3Dc<T1,T2> > nodes;
    
    std::vector<T1> slowness;   // column-wise (z axis) slowness vector of the cells, NOT used by Grid3Dcinterp
    std::vector<std::vector<T2> > neighbors;  // nodes common to a cell
	
    void buildGridNodes();
    void buildGridNeighbors();
	
    void initQueue(const std::vector<sxyz<T1> >& Tx,
				   const std::vector<T1>& t0,
				   std::priority_queue<Node3Dc<T1,T2>*,
				   std::vector<Node3Dc<T1,T2>*>,
				   CompareNodePtr<T1> >& queue,
				   std::vector<Node3Dc<T1,T2> >& txNodes,
				   std::vector<bool>& inQueue,
				   std::vector<bool>& frozen,
				   const int threadNo) const;
    
    void propagate(std::priority_queue<Node3Dc<T1,T2>*, std::vector<Node3Dc<T1,T2>*>,
                   CompareNodePtr<T1> >& queue,
                   std::vector<bool>& inQueue,
                   std::vector<bool>& frozen,
                   int threadNo) const;
	
    void prepropagate(const Node3Dc<T1,T2>& node,
					  std::priority_queue<Node3Dc<T1,T2>*, std::vector<Node3Dc<T1,T2>*>,
					  CompareNodePtr<T1> >& queue,
					  std::vector<bool>& inQueue,
					  std::vector<bool>& frozen,
					  int threadNo) const;
	
	
	
	
    void initQueue2(const std::vector<sxyz<T1> >& Tx,
					const std::vector<T1>& t0,
					std::priority_queue<Node3Dc<T1,T2>*,
					std::deque<Node3Dc<T1,T2>*>,
					CompareNodePtr<T1> >& queue,
					std::vector<Node3Dc<T1,T2> >& txNodes,
					std::vector<bool>& inQueue,
					std::vector<bool>& frozen,
					const int threadNo) const;
    
    void propagate2(std::priority_queue<Node3Dc<T1,T2>*, std::deque<Node3Dc<T1,T2>*>,
					CompareNodePtr<T1> >& queue,
					std::vector<bool>& inQueue,
					std::vector<bool>& frozen,
					int threadNo) const;
	
    void prepropagate2(const Node3Dc<T1,T2>& node,
					   std::priority_queue<Node3Dc<T1,T2>*, std::deque<Node3Dc<T1,T2>*>,
					   CompareNodePtr<T1> >& queue,
					   std::vector<bool>& inQueue,
					   std::vector<bool>& frozen,
					   int threadNo) const;
	
	
	
	T1 computeDt(const Node3Dc<T1,T2>& source, const sxyz<T1>& node,
				 const size_t cellNo) const {
		return slowness[cellNo] * source.getDistance( node );
	}
    
	T1 computeDt(const Node3Dc<T1,T2>& source, const Node3Dc<T1,T2>& node,
				 const size_t cellNo) const {
		return slowness[cellNo] * source.getDistance( node );
	}
    
    int check_pts(const std::vector<sxyz<T1> >&) const;
    
    T1 getTraveltime(const sxyz<T1>& Rx,
					 const std::vector<Node3Dc<T1,T2> >& nodes,
					 const int threadNo) const;
	
    T1 getTraveltime(const sxyz<T1>& Rx,
					 const std::vector<Node3Dc<T1,T2> >& nodes,
					 T2&, T2& , const int threadNo) const;
	
    Grid3Dc() {}
    Grid3Dc(const Grid3Dc<T1,T2>& g) {}
    Grid3Dc<T1,T2>& operator=(const Grid3Dc<T1,T2>& g) { return *this; }
    
	};
	
	
	/* Constructor Format:
	 Grid3Dc<T1,T2>::Grid3Dc(nb cells in x, nb cells in y, nb cells in z,
	 x cells size, y cells size, z cells size,
	 x origin, y origin, z origin,
	 nb sec. cells in x, nb sec. cells in y, nb sec. cells in z,
	 index of the thread)
	 */
	template<typename T1, typename T2>
	Grid3Dc<T1,T2>::Grid3Dc(const T2 nx, const T2 ny, const T2 nz,
							const T1 ddx, const T1 ddy, const T1 ddz,
							const T1 minx, const T1 miny, const T1 minz,
							const T2 nnx, const T2 nny, const T2 nnz,
							const int nt) :
	Grid3D<T1,T2>(nx, ny, nz, ddx, ddy, ddz, minx, miny, minz, nnx, nny, nnz, nt),
	nodes(std::vector<Node3Dc<T1,T2> >(// secondary nodes on the edges
									   nx*nnx*((ny+1)*(nz+1)) +
									   ny*nny*((nx+1)*(nz+1)) +
									   nz*nnz*((nx+1)*(ny+1)) +
									   // secondary nodes on the faces
									   (nnx*nny)*(nx*ny*(nz+1))+
									   (nnx*nnz)*(nx*nz*(ny+1))+
									   (nny*nnz)*(ny*nz*(nx+1))+
									   // primary nodes
									   (nx+1) * (ny+1) * (nz+1),
									   Node3Dc<T1,T2>(nt) )),
	slowness(std::vector<T1>(nx*ny*nz)),
	neighbors(std::vector<std::vector<T2> >(nx*ny*nz))
	{
		buildGridNodes();
		buildGridNeighbors();
	}
	
	template<typename T1, typename T2>
	void Grid3Dc<T1,T2>::buildGridNodes() {
		
		// Create the grid, assign a number for each node and find the owners
		// Nodes and cells are first indexed in z, then y, and x.
		// Secondary nodes are placed on the faces and edges of every cells.
		// Ex: the node in "node[A]=(i,j,k)" is followed by the node in
		// "node[A+1]=(i+dx,j,k)"
		
		T1 dxs = Grid3D<T1,T2>::dx/(Grid3D<T1,T2>::nsnx+1); 	// distance between secondary nodes in x
		T1 dys = Grid3D<T1,T2>::dy/(Grid3D<T1,T2>::nsny+1);
		T1 dzs = Grid3D<T1,T2>::dz/(Grid3D<T1,T2>::nsnz+1);
		
		T2 cXmYmZm; 	// cell in the (x-,y-,z-) direction from the node
		T2 cXpYmZm; 	// cell in the (x+,y-,z-) direction from the node
		T2 cXmYpZm;
		T2 cXpYpZm;
		T2 cXmYmZp;
		T2 cXpYmZp;
		T2 cXmYpZp;
		T2 cXpYpZp;
		
		T2 n = 0;
		for ( T2 nk=0; nk<=Grid3D<T1,T2>::nCellz; ++nk ) {
			
			T1 z = Grid3D<T1,T2>::zmin + nk*Grid3D<T1,T2>::dz;
			
			for ( T2 nj=0; nj<=Grid3D<T1,T2>::nCelly; ++nj ) {
				
				T1 y = Grid3D<T1,T2>::ymin + nj*Grid3D<T1,T2>::dy;
				
				for (T2 ni=0; ni<=Grid3D<T1,T2>::nCellx; ++ni){
					
					T1 x = Grid3D<T1,T2>::xmin + ni*Grid3D<T1,T2>::dx;
					
					// Find the adjacent cells for each primary node
					
					if (ni < Grid3D<T1,T2>::nCellx && nj < Grid3D<T1,T2>::nCelly && nk < Grid3D<T1,T2>::nCellz){
						cXpYpZp = nj*Grid3D<T1,T2>::nCellx + nk*(Grid3D<T1,T2>::nCellx*Grid3D<T1,T2>::nCelly) + ni;
					}
					else {
						cXpYpZp = std::numeric_limits<T2>::max();
					}
					
					if (ni > 0 && nj < Grid3D<T1,T2>::nCelly && nk < Grid3D<T1,T2>::nCellz){
						cXmYpZp = nj*Grid3D<T1,T2>::nCellx + nk*(Grid3D<T1,T2>::nCellx*Grid3D<T1,T2>::nCelly) + ni - 1;
					}
					else {
						cXmYpZp = std::numeric_limits<T2>::max();
					}
					
					if (ni < Grid3D<T1,T2>::nCellx && nj > 0 && nk < Grid3D<T1,T2>::nCellz){
						cXpYmZp = (nj-1)*Grid3D<T1,T2>::nCellx + nk*(Grid3D<T1,T2>::nCellx*Grid3D<T1,T2>::nCelly) + ni;
					}
					else {
						cXpYmZp = std::numeric_limits<T2>::max();
					}
					
					if (ni > 0 && nj > 0 && nk < Grid3D<T1,T2>::nCellz){
						cXmYmZp = (nj-1)*Grid3D<T1,T2>::nCellx + nk*(Grid3D<T1,T2>::nCellx * Grid3D<T1,T2>::nCelly) + ni - 1;
					}
					else {
						cXmYmZp = std::numeric_limits<T2>::max();
					}
					
					if (ni < Grid3D<T1,T2>::nCellx && nj < Grid3D<T1,T2>::nCelly && nk > 0){
						cXpYpZm = nj*Grid3D<T1,T2>::nCellx + (nk-1)*(Grid3D<T1,T2>::nCellx*Grid3D<T1,T2>::nCelly) + ni;
					}
					else {
						cXpYpZm = std::numeric_limits<T2>::max();
					}
					
					if (ni > 0 && nj < Grid3D<T1,T2>::nCelly && nk > 0){
						cXmYpZm = nj*Grid3D<T1,T2>::nCellx + (nk-1)*(Grid3D<T1,T2>::nCellx*Grid3D<T1,T2>::nCelly) + ni - 1;
					}
					else {
						cXmYpZm = std::numeric_limits<T2>::max();
					}
					
					if (ni < Grid3D<T1,T2>::nCellx && nj > 0 && nk > 0){
						cXpYmZm = (nj-1)*Grid3D<T1,T2>::nCellx + (nk-1)*(Grid3D<T1,T2>::nCellx*Grid3D<T1,T2>::nCelly) + ni;
					}
					else {
						cXpYmZm = std::numeric_limits<T2>::max();
					}
					
					if (ni > 0 && nj > 0 && nk > 0){
						cXmYmZm = (nj-1)*Grid3D<T1,T2>::nCellx + (nk-1)*(Grid3D<T1,T2>::nCellx*Grid3D<T1,T2>::nCelly) + ni-1;
					}
					else {
						cXmYmZm = std::numeric_limits<T2>::max();
					}
					
					
					// Index the primary nodes owners
					
					if ( cXmYmZm != std::numeric_limits<T2>::max() ) {
						nodes[n].pushOwner( cXmYmZm );
					}
					if ( cXpYmZm != std::numeric_limits<T2>::max() ) {
						nodes[n].pushOwner( cXpYmZm );
					}
					if ( cXmYpZm != std::numeric_limits<T2>::max() ) {
						nodes[n].pushOwner( cXmYpZm );
					}
					if ( cXpYpZm != std::numeric_limits<T2>::max() ) {
						nodes[n].pushOwner( cXpYpZm );
					}
					if ( cXmYmZp != std::numeric_limits<T2>::max() ) {
						nodes[n].pushOwner( cXmYmZp );
					}
					if ( cXpYmZp != std::numeric_limits<T2>::max() ) {
						nodes[n].pushOwner( cXpYmZp );
					}
					if ( cXmYpZp != std::numeric_limits<T2>::max() ) {
						nodes[n].pushOwner( cXmYpZp );
					}
					if ( cXpYpZp != std::numeric_limits<T2>::max() ) {
						nodes[n].pushOwner( cXpYpZp );
					}
					
					nodes[n].setXYZindex( x, y, z, n );
					
					++n;
					
					// Secondary nodes on x edge
					if ( ni < Grid3D<T1,T2>::nCellx ) {
						for (T2 ns=0; ns< Grid3D<T1,T2>::nsnx; ++ns, ++n ) {
							
							T1 xsv = Grid3D<T1,T2>::xmin + ni* Grid3D<T1,T2>::dx + (ns+1)*dxs;
							
							if ( cXpYmZm != std::numeric_limits<T2>::max() )
							{
								nodes[n].pushOwner( cXpYmZm );
							}
							if ( cXpYpZm != std::numeric_limits<T2>::max() )
							{
								nodes[n].pushOwner( cXpYpZm );
							}
							if ( cXpYmZp != std::numeric_limits<T2>::max() )
							{
								nodes[n].pushOwner( cXpYmZp );
							}
							if ( cXpYpZp != std::numeric_limits<T2>::max() )
							{
								nodes[n].pushOwner( cXpYpZp );
							}
							nodes[n].setXYZindex( xsv, y, z, n );
						}
					}
					
					// Secondary nodes on y edge
					if ( nj < Grid3D<T1,T2>::nCelly ) {
						for (T2 ns=0; ns< Grid3D<T1,T2>::nsny; ++ns, ++n ) {
							
							T1 ysv = Grid3D<T1,T2>::ymin + nj* Grid3D<T1,T2>::dy + (ns+1)*dys;
							
							if ( cXmYpZm != std::numeric_limits<T2>::max() )
							{
								nodes[n].pushOwner( cXmYpZm );
							}
							if ( cXpYpZm != std::numeric_limits<T2>::max() )
							{
								nodes[n].pushOwner( cXpYpZm );
							}
							if ( cXmYpZp != std::numeric_limits<T2>::max() )
							{
								nodes[n].pushOwner( cXmYpZp );
							}
							if ( cXpYpZp != std::numeric_limits<T2>::max() )
							{
								nodes[n].pushOwner( cXpYpZp );
							}
							nodes[n].setXYZindex( x, ysv, z, n );
						}
					}
					
					// Secondary nodes on z edge
					if ( nk < Grid3D<T1,T2>::nCellz ) {
						for (T2 ns=0; ns< Grid3D<T1,T2>::nsnz; ++ns, ++n ) {
							
							T1 zsv = Grid3D<T1,T2>::zmin + nk* Grid3D<T1,T2>::dz + (ns+1)*dzs;
							
							if ( cXmYmZp != std::numeric_limits<T2>::max() )
							{
								nodes[n].pushOwner( cXmYmZp );
							}
							if ( cXpYmZp != std::numeric_limits<T2>::max() )
							{
								nodes[n].pushOwner( cXpYmZp );
							}
							if ( cXmYpZp != std::numeric_limits<T2>::max() )
							{
								nodes[n].pushOwner( cXmYpZp );
							}
							if ( cXpYpZp != std::numeric_limits<T2>::max() )
							{
								nodes[n].pushOwner( cXpYpZp );
							}
							nodes[n].setXYZindex( x, y, zsv, n );
						}
					}
					
					// Secondary nodes on the xy0 planes
					if ( ni < Grid3D<T1,T2>::nCellx && nj < Grid3D<T1,T2>::nCelly ) {
						for ( T2 sy=0; sy < Grid3D<T1,T2>::nsny; ++sy ) {
							for ( T2 sx=0; sx < Grid3D<T1,T2>::nsnx; ++sx, n++ ) {
								
								T1 ysv= Grid3D<T1,T2>::ymin+ nj* Grid3D<T1,T2>::dy+ (sy+1)*dys;
								T1 xsv= Grid3D<T1,T2>::xmin+ ni* Grid3D<T1,T2>::dx+ (sx+1)*dxs;
								
								if ( cXpYpZm != std::numeric_limits<T2>::max() )
								{
									nodes[n].pushOwner( cXpYpZm );
								}
								if ( cXpYpZp != std::numeric_limits<T2>::max() )
								{
									nodes[n].pushOwner( cXpYpZp );
								}
								nodes[n].setXYZindex( xsv, ysv, z, n );
							}
						}
					}
					
					// Secondary nodes on the x0z planes
					if ( ni < Grid3D<T1,T2>::nCellx && nk < Grid3D<T1,T2>::nCellz ) {
						for ( T2 sz=0; sz < Grid3D<T1,T2>::nsnz; ++sz ) {
							for ( T2 sx=0; sx < Grid3D<T1,T2>::nsnx; ++sx, n++ ) {
								
								T1 zsv= Grid3D<T1,T2>::zmin+ nk* Grid3D<T1,T2>::dz+ (sz+1)*dzs;
								T1 xsv= Grid3D<T1,T2>::xmin+ ni* Grid3D<T1,T2>::dx+ (sx+1)*dxs;
								
								if ( cXpYmZp != std::numeric_limits<T2>::max() )
								{
									nodes[n].pushOwner( cXpYmZp );
								}
								if ( cXpYpZp != std::numeric_limits<T2>::max() )
								{
									nodes[n].pushOwner( cXpYpZp );
								}
								nodes[n].setXYZindex( xsv, y, zsv, n );
							}
						}
					}
					
					// Secondary nodes on the 0yz planes
					if ( nj < Grid3D<T1,T2>::nCelly && nk < Grid3D<T1,T2>::nCellz ) {
						for ( T2 sz=0; sz < Grid3D<T1,T2>::nsnz; ++sz ) {
							for ( T2 sy=0; sy < Grid3D<T1,T2>::nsny; ++sy, n++ ) {
								
								T1 zsv= Grid3D<T1,T2>::zmin+ nk* Grid3D<T1,T2>::dz+ (sz+1)*dzs;
								T1 ysv= Grid3D<T1,T2>::ymin+ nj* Grid3D<T1,T2>::dy+ (sy+1)*dys;
								
								if ( cXmYpZp != std::numeric_limits<T2>::max() )
								{
									nodes[n].pushOwner( cXmYpZp );
								}
								if ( cXpYpZp != std::numeric_limits<T2>::max() )
								{
									nodes[n].pushOwner( cXpYpZp );
								}
								nodes[n].setXYZindex( x, ysv, zsv, n );
							}
						}
					}
				}
			}
		}
		// sanity check
		if ( n != nodes.size() ) {
			std::cerr << "Error building grid, wrong number of nodes\n";
			abort();
		}
	}
	
	template<typename T1, typename T2>
	void Grid3Dc<T1,T2>::buildGridNeighbors() {
		
		// Index the neighbors nodes of each cell
		for ( T2 n=0; n<nodes.size(); ++n ) {
			for ( size_t n2=0; n2<nodes[n].getOwners().size(); ++n2) {
				neighbors[ nodes[n].getOwners()[n2] ].push_back(n);
			}
		}
	}
	
	template<typename T1, typename T2>
	int Grid3Dc<T1,T2>::check_pts(const std::vector<sxyz<T1> >& pts) const {
		
		// Check if the points from a vector are in the grid
		for ( size_t n=0; n<pts.size(); ++n ) {
			if ( pts[n].x < Grid3D<T1,T2>::xmin || pts[n].x > Grid3D<T1,T2>::xmax ||
				pts[n].y < Grid3D<T1,T2>::ymin || pts[n].y > Grid3D<T1,T2>::ymax ||
				pts[n].z < Grid3D<T1,T2>::zmin || pts[n].z > Grid3D<T1,T2>::zmax ) {
				std::cerr << "Error: point no " << (n+1)
				<< " outside the grid.\n";
				return 1;
			}
		}
		return 0;
	}
	
	template<typename T1, typename T2>
	int Grid3Dc<T1,T2>::raytrace(const std::vector<sxyz<T1> >& Tx,
								 const std::vector<T1>& t0,
								 const std::vector<sxyz<T1> >& Rx,
								 std::vector<T1>& traveltimes,
								 const int threadNo) const {
		
		// Primary function
		
		// Checks if the points are in the grid
		if ( check_pts(Tx) == 1 ) return 1;
		if ( check_pts(Rx) == 1 ) return 1;
		
		for ( size_t n=0; n<nodes.size(); ++n ) {
			nodes[n].reinit( threadNo );
		}
		
		CompareNodePtr<T1> cmp(threadNo);
		std::priority_queue< Node3Dc<T1,T2>*, std::vector<Node3Dc<T1,T2>*>,
		CompareNodePtr<T1> > queue(cmp);
		// txNodes: Extra nodes if the sources points are not on an existing node
		std::vector<Node3Dc<T1,T2> > txNodes;
		// inQueue lists the nodes waiting in the queue
		std::vector<bool> inQueue( nodes.size(), false );
		// Tx sources nodes are "frozen" and their traveltime can't be modified
		std::vector<bool> frozen( nodes.size(), false );
		
		initQueue(Tx, t0, queue, txNodes, inQueue, frozen, threadNo);
		
		propagate(queue, inQueue, frozen, threadNo);
		
		if ( traveltimes.size() != Rx.size() ) {
			traveltimes.resize( Rx.size() );
		}
		
		for (size_t n=0; n<Rx.size(); ++n) {
			traveltimes[n] = getTraveltime(Rx[n], nodes, threadNo);
		}
		return 0;
	}
	
	template<typename T1, typename T2>
	int Grid3Dc<T1,T2>::raytrace(const std::vector<sxyz<T1> >& Tx,
								 const std::vector<T1>& t0,
								 const std::vector<sxyz<T1> >& Rx,
								 std::vector<T1>& traveltimes,
								 std::vector<std::vector<sxyz<T1> > >& r_data,
								 const int threadNo) const {
		
		// Primary function
		
		// Checks if the points are in the grid
		if ( check_pts(Tx) == 1 ) return 1;
		if ( check_pts(Rx) == 1 ) return 1;
		
		for ( size_t n=0; n<nodes.size(); ++n ) {
			nodes[n].reinit( threadNo );
		}
		
		CompareNodePtr<T1> cmp(threadNo);
		std::priority_queue< Node3Dc<T1,T2>*, std::vector<Node3Dc<T1,T2>*>,
		CompareNodePtr<T1> > queue(cmp);
		// txNodes: Extra nodes if the sources points are not on an existing node
		std::vector<Node3Dc<T1,T2> > txNodes;
		// inQueue lists the nodes waiting in the queue
		std::vector<bool> inQueue( nodes.size(), false );
		// Tx sources nodes are "frozen" and their traveltime can't be modified
		std::vector<bool> frozen( nodes.size(), false );
		
		initQueue(Tx, t0, queue, txNodes, inQueue, frozen, threadNo);
		
		propagate(queue, inQueue, frozen, threadNo);
		
		if ( traveltimes.size() != Rx.size() ) {
			traveltimes.resize( Rx.size() );
		}
		if ( r_data.size() != Rx.size() ) {
			r_data.resize( Rx.size() );
		}
		for ( size_t ni=0; ni<r_data.size(); ++ni ) {
			r_data[ni].resize( 0 );
		}
		T2 nodeParentRx;
		T2 cellParentRx;
		
		for (size_t n=0; n<Rx.size(); ++n) {
			traveltimes[n] = getTraveltime(Rx[n], nodes, nodeParentRx, cellParentRx,
										   threadNo);
			
			// Rx are in nodes (not txNodes)
			std::vector<Node3Dc<T1,T2> > *node_p;
			node_p = &nodes;
			
			std::vector<sxyz<T1> > r_tmp;
			T2 iChild, iParent = nodeParentRx;
			sxyz<T1> child;
			
			// store the son's coord
			child.x = Rx[n].x;
			child.y = Rx[n].y;
			child.z = Rx[n].z;
			while ( (*node_p)[iParent].getNodeParent(threadNo) != std::numeric_limits<T2>::max() ) {
				
				r_tmp.push_back( child );
				
				// we now go up in time - parent becomes the child of grand'pa
				iChild = iParent;
				child.x = (*node_p)[iChild].getX();
				child.y = (*node_p)[iChild].getY();
				child.z = (*node_p)[iChild].getZ();
				
				// grand'pa is now papa
				iParent = (*node_p)[iChild].getNodeParent(threadNo);
				if ( iParent >= nodes.size() ) {
					node_p = &txNodes;
					iParent -= nodes.size();
				}
				else {
					node_p = &nodes;
				}
			}
			
			// parent is now at Tx
			r_tmp.push_back( child );
			
			// finally, store Tx position
			child.x = (*node_p)[iParent].getX();
			child.y = (*node_p)[iParent].getY();
			child.z = (*node_p)[iParent].getZ();
			r_tmp.push_back( child );
			
			// the order should be from Tx to Rx, so we reorder...
			iParent = r_tmp.size();
			r_data[n].resize( r_tmp.size() );
			for ( size_t nn=0; nn<r_data[n].size(); ++nn ) {
				r_data[n][nn].x = r_tmp[ iParent-1-nn ].x;
				r_data[n][nn].y = r_tmp[ iParent-1-nn ].y;
				r_data[n][nn].z = r_tmp[ iParent-1-nn ].z;
			}
		}
		return 0;
	}
	
	
	template<typename T1, typename T2>
	int Grid3Dc<T1,T2>::raytrace(const std::vector<sxyz<T1> >& Tx,
								 const std::vector<T1>& t0,
								 const std::vector<sxyz<T1> >& Rx,
								 std::vector<T1>& traveltimes,
								 std::vector<std::vector<siv<T1> > >& l_data,
								 std::vector<std::vector<sxyz<T1> > >& r_data,
								 const int threadNo) const {
		
		// Primary function
		
		// Checks if the points are in the grid
		if ( check_pts(Tx) == 1 ) return 1;
		if ( check_pts(Rx) == 1 ) return 1;
		
		for ( size_t n=0; n<nodes.size(); ++n ) {
			nodes[n].reinit( threadNo );
		}
		
		CompareNodePtr<T1> cmp(threadNo);
		std::priority_queue< Node3Dc<T1,T2>*, std::vector<Node3Dc<T1,T2>*>,
		CompareNodePtr<T1> > queue(cmp);
		// txNodes: Extra nodes if the sources points are not on an existing node
		std::vector<Node3Dc<T1,T2> > txNodes;
		// inQueue lists the nodes waiting in the queue
		std::vector<bool> inQueue( nodes.size(), false );
		// Tx sources nodes are "frozen" and their traveltime can't be modified
		std::vector<bool> frozen( nodes.size(), false );
		
		initQueue(Tx, t0, queue, txNodes, inQueue, frozen, threadNo);
		
		propagate(queue, inQueue, frozen, threadNo);
		
		if ( traveltimes.size() != Rx.size() ) {
			traveltimes.resize( Rx.size() );
		}
		if ( l_data.size() != Rx.size() ) {
			l_data.resize( Rx.size() );
		}
		for ( size_t ni=0; ni<l_data.size(); ++ni ) {
			l_data[ni].resize( 0 );
		}
		if ( r_data.size() != Rx.size() ) {
			r_data.resize( Rx.size() );
		}
		for ( size_t ni=0; ni<r_data.size(); ++ni ) {
			r_data[ni].resize( 0 );
		}
		T2 nodeParentRx;
		T2 cellParentRx;
		
		for (size_t n=0; n<Rx.size(); ++n) {
			traveltimes[n] = getTraveltime(Rx[n], nodes, nodeParentRx, cellParentRx,
										   threadNo);
			
			// Rx are in nodes (not txNodes)
			std::vector<Node3Dc<T1,T2> > *node_p;
			node_p = &nodes;
			
			std::vector<sxyz<T1> > r_tmp;
			T2 iChild, iParent = nodeParentRx;
			sxyz<T1> child;
			siv<T1> cell;
			
			// store the son's coord
			child.x = Rx[n].x;
			child.y = Rx[n].y;
			child.z = Rx[n].z;
			cell.i = cellParentRx;
			while ( (*node_p)[iParent].getNodeParent(threadNo) != std::numeric_limits<T2>::max() ) {
				
				r_tmp.push_back( child );
				
				cell.v = (*node_p)[iParent].getDistance( child );
				bool found=false;
				for (size_t nc=0; nc<l_data[n].size(); ++nc) {
					if ( l_data[n][nc].i == cell.i ) {
						l_data[n][nc].v += cell.v;
						found = true;
					}
				}
				if ( found == false ) {
					l_data[n].push_back( cell );
				}
				
				// we now go up in time - parent becomes the child of grand'pa
				iChild = iParent;
				child.x = (*node_p)[iChild].getX();
				child.y = (*node_p)[iChild].getY();
				child.z = (*node_p)[iChild].getZ();
				cell.i = (*node_p)[iChild].getCellParent(threadNo);
				
				// grand'pa is now papa
				iParent = (*node_p)[iChild].getNodeParent(threadNo);
				if ( iParent >= nodes.size() ) {
					node_p = &txNodes;
					iParent -= nodes.size();
				}
				else {
					node_p = &nodes;
				}
			}
			
			// parent is now at Tx
			r_tmp.push_back( child );
			
			cell.v = (*node_p)[iParent].getDistance( child );
			bool found=false;
			for (size_t nc=0; nc<l_data[n].size(); ++nc) {
				if ( l_data[n][nc].i == cell.i ) {
					l_data[n][nc].v += cell.v;
					found = true;
				}
			}
			if ( found == false ) {
				l_data[n].push_back( cell );
			}
			
			// finally, store Tx position
			child.x = (*node_p)[iParent].getX();
			child.y = (*node_p)[iParent].getY();
			child.z = (*node_p)[iParent].getZ();
			r_tmp.push_back( child );
			
			//  must be sorted to build matrix L
			sort(l_data[n].begin(), l_data[n].end(), CompareSiv<T1>());
			
			// the order should be from Tx to Rx, so we reorder...
			iParent = r_tmp.size();
			r_data[n].resize( r_tmp.size() );
			for ( size_t nn=0; nn<r_data[n].size(); ++nn ) {
				r_data[n][nn].x = r_tmp[ iParent-1-nn ].x;
				r_data[n][nn].y = r_tmp[ iParent-1-nn ].y;
				r_data[n][nn].z = r_tmp[ iParent-1-nn ].z;
			}
		}
		return 0;
	}
	
	
	template<typename T1, typename T2>
	void Grid3Dc<T1,T2>::initQueue(const std::vector<sxyz<T1> >& Tx,
								   const std::vector<T1>& t0,
								   std::priority_queue<Node3Dc<T1,T2>*,
								   std::vector<Node3Dc<T1,T2>*>,
								   CompareNodePtr<T1> >& queue,
								   std::vector<Node3Dc<T1,T2> >& txNodes,
								   std::vector<bool>& inQueue,
								   std::vector<bool>& frozen,
								   const int threadNo) const {
		
		//Find the starting nodes of the transmitters Tx and start the queue list
		for ( size_t n=0; n<Tx.size(); ++n ) {
			bool found = false;
			for ( size_t nn=0; nn<nodes.size(); ++nn ) {
				if ( nodes[nn] == Tx[n] ) {
					found = true;
					nodes[nn].setTT( t0[n], threadNo );
					frozen[nn] = true;
					
					prepropagate(nodes[nn], queue, inQueue, frozen, threadNo); // See description in the function declaration
					
					//	queue.push( &(nodes[nn]) );   	//Don't use if prepropagate is used
					//	inQueue[nn] = true;				//Don't use if prepropagate is used
					
					break;
				}
			}
			if ( found==false ) {
				// If Tx[n] is not on a node, we create a new node and initialize the queue:
				txNodes.push_back( Node3Dc<T1,T2>(t0[n], Tx[n].x, Tx[n].y, Tx[n].z, Grid3D<T1,T2>::nThreads, threadNo));
				txNodes.back().pushOwner( getCellNo(Tx[n]) );
				txNodes.back().setGridIndex( nodes.size()+txNodes.size()-1 );
				frozen.push_back( true );
				
				prepropagate(txNodes.back(), queue, inQueue, frozen, threadNo); // See description in the function declaration
				
				//	queue.push( &(txNodes.back()) );	//Don't use if prepropagate is used
				//	inQueue.push_back( true );			//Don't use if prepropagate is used
				
			}
		}
	}
	
	template<typename T1, typename T2>
	void Grid3Dc<T1,T2>::propagate( std::priority_queue<Node3Dc<T1,T2>*,
								   std::vector<Node3Dc<T1,T2>*>,
								   CompareNodePtr<T1> >& queue,
								   std::vector<bool>& inQueue,
								   std::vector<bool>& frozen,
								   int threadNo) const {
		
		while ( !queue.empty() ) {
			const Node3Dc<T1,T2>* source = queue.top();
			queue.pop();
			inQueue[ source->getGridIndex() ] = false;
			for ( size_t no=0; no<source->getOwners().size(); ++no ) {
				size_t cellNo = source->getOwners()[no];
				for ( size_t k=0; k< neighbors[cellNo].size(); ++k ) {
					size_t neibNo = neighbors[cellNo][k];
					if ( neibNo == source->getGridIndex() || frozen[neibNo] ) {
						continue;
					}
					
					T1 ttsource= source->getTT( threadNo );
					if (ttsource < nodes[neibNo].getTT(threadNo)){
						// Compute dt
						T1 dt = computeDt(*source, nodes[neibNo], cellNo);
						
						if ( ttsource +dt < nodes[neibNo].getTT( threadNo ) ) {
							nodes[neibNo].setTT( ttsource +dt, threadNo );
							nodes[neibNo].setnodeParent( source->getGridIndex(),
														threadNo );
							nodes[neibNo].setCellParent( cellNo, threadNo );
							
							if ( !inQueue[neibNo] ) {
								queue.push( &(nodes[neibNo]) );
								inQueue[neibNo] = true;
							}
						}
					}
					
				}
			}
		}
	}
	
	template<typename T1, typename T2>
	void Grid3Dc<T1,T2>::prepropagate(const Node3Dc<T1,T2>& node,
									  std::priority_queue<Node3Dc<T1,T2>*,
									  std::vector<Node3Dc<T1,T2>*>,
									  CompareNodePtr<T1> >& queue,
									  std::vector<bool>& inQueue,
									  std::vector<bool>& frozen,
									  int threadNo) const {
		
		// This function can be used to "prepropagate" each Tx nodes one first time
		// during "initQueue", before running "propagate".
		// When a Tx source node seems to be lost in the queue and is not
		// propagated, corrupting the entire traveltime table,
		// this function force the propagation of every source points and can
		// solve the problem.
		
		for ( size_t no=0; no<node.getOwners().size(); ++no ) {
			size_t cellNo = node.getOwners()[no];
			for ( size_t k=0; k< neighbors[cellNo].size(); ++k ) {
				size_t neibNo = neighbors[cellNo][k];
				if ( neibNo == node.getGridIndex() || frozen[neibNo] ) {
					continue;
				}
				
				// compute dt
				T1 dt = computeDt(node, nodes[neibNo], cellNo);
				
				if ( node.getTT( threadNo )+dt < nodes[neibNo].getTT( threadNo ) ) {
					nodes[neibNo].setTT( node.getTT( threadNo )+dt, threadNo );
					nodes[neibNo].setnodeParent( node.getGridIndex(), threadNo );
					nodes[neibNo].setCellParent( cellNo, threadNo );
					
					if ( !inQueue[neibNo] ) {
						queue.push( &(nodes[neibNo]) );
						inQueue[neibNo] = true;
					}
				}
			}
		}
	}
	
	template<typename T1, typename T2>
	int Grid3Dc<T1,T2>::raytrace2(const std::vector<sxyz<T1> >& Tx,
								  const std::vector<T1>& t0,
								  const std::vector<sxyz<T1> >& Rx,
								  std::vector<T1>& traveltimes,
								  const int threadNo) const {
		
		// Primary function
		
		// Checks if the points are in the grid
		if ( check_pts(Tx) == 1 ) return 1;
		if ( check_pts(Rx) == 1 ) return 1;
		
		for ( size_t n=0; n<nodes.size(); ++n ) {
			nodes[n].reinit( threadNo );
		}
		
		CompareNodePtr<T1> cmp(threadNo);
		std::priority_queue< Node3Dc<T1,T2>*, std::deque<Node3Dc<T1,T2>*>,
		CompareNodePtr<T1> > queue(cmp);
		// txNodes: Extra nodes if the sources points are not on an existing node
		std::vector<Node3Dc<T1,T2> > txNodes;
		// inQueue lists the nodes waiting in the queue
		std::vector<bool> inQueue( nodes.size(), false );
		// Tx sources nodes are "frozen" and their traveltime can't be modified
		std::vector<bool> frozen( nodes.size(), false );
		
		initQueue2(Tx, t0, queue, txNodes, inQueue, frozen, threadNo);
		
		propagate2(queue, inQueue, frozen, threadNo);
		
		if ( traveltimes.size() != Rx.size() ) {
			traveltimes.resize( Rx.size() );
		}
		
		for (size_t n=0; n<Rx.size(); ++n) {
			traveltimes[n] = getTraveltime(Rx[n], nodes, threadNo);
		}
		return 0;
	}
	
	template<typename T1, typename T2>
	void Grid3Dc<T1,T2>::initQueue2(const std::vector<sxyz<T1> >& Tx,
									const std::vector<T1>& t0,
									std::priority_queue<Node3Dc<T1,T2>*,
									std::deque<Node3Dc<T1,T2>*>,
									CompareNodePtr<T1> >& queue,
									std::vector<Node3Dc<T1,T2> >& txNodes,
									std::vector<bool>& inQueue,
									std::vector<bool>& frozen,
									const int threadNo) const {
		
		//Find the starting nodes of the transmitters Tx and start the queue list
		for ( size_t n=0; n<Tx.size(); ++n ) {
			bool found = false;
			for ( size_t nn=0; nn<nodes.size(); ++nn ) {
				if ( nodes[nn] == Tx[n] ) {
					found = true;
					nodes[nn].setTT( t0[n], threadNo );
					frozen[nn] = true;
					
					prepropagate2(nodes[nn], queue, inQueue, frozen, threadNo); // See description in the function declaration
					
					//	queue.push( &(nodes[nn]) );   	//Don't use if prepropagate is used
					//	inQueue[nn] = true;				//Don't use if prepropagate is used
					
					break;
				}
			}
			if ( found==false ) {
				// If Tx[n] is not on a node, we create a new node and initialize the queue:
				txNodes.push_back( Node3Dc<T1,T2>(t0[n], Tx[n].x, Tx[n].y, Tx[n].z, Grid3D<T1,T2>::nThreads, threadNo));
				txNodes.back().pushOwner( getCellNo(Tx[n]) );
				txNodes.back().setGridIndex( nodes.size()+txNodes.size()-1 );
				frozen.push_back( true );
				
				prepropagate2(txNodes.back(), queue, inQueue, frozen, threadNo); // See description in the function declaration
				
				//	queue.push( &(txNodes.back()) );	//Don't use if prepropagate is used
				//	inQueue.push_back( true );			//Don't use if prepropagate is used
				
			}
		}
	}
	
	template<typename T1, typename T2>
	void Grid3Dc<T1,T2>::propagate2( std::priority_queue<Node3Dc<T1,T2>*,
									std::deque<Node3Dc<T1,T2>*>,
									CompareNodePtr<T1> >& queue,
									std::vector<bool>& inQueue,
									std::vector<bool>& frozen,
									int threadNo) const {
		
		while ( !queue.empty() ) {
			const Node3Dc<T1,T2>* source = queue.top();
			queue.pop();
			inQueue[ source->getGridIndex() ] = false;
			for ( size_t no=0; no<source->getOwners().size(); ++no ) {
				size_t cellNo = source->getOwners()[no];
				for ( size_t k=0; k< neighbors[cellNo].size(); ++k ) {
					size_t neibNo = neighbors[cellNo][k];
					if ( neibNo == source->getGridIndex() || frozen[neibNo] ) {
						continue;
					}
					
					T1 ttsource= source->getTT( threadNo );
					if (ttsource < nodes[neibNo].getTT(threadNo)){
						// Compute dt
						T1 dt = computeDt(*source, nodes[neibNo], cellNo);
						
						if ( ttsource +dt < nodes[neibNo].getTT( threadNo ) ) {
							nodes[neibNo].setTT( ttsource +dt, threadNo );
							nodes[neibNo].setnodeParent( source->getGridIndex(),
														threadNo );
							nodes[neibNo].setCellParent( cellNo, threadNo );
							
							if ( !inQueue[neibNo] ) {
								queue.push( &(nodes[neibNo]) );
								inQueue[neibNo] = true;
							}
						}
					}
					
				}
			}
		}
	}
	
	template<typename T1, typename T2>
	void Grid3Dc<T1,T2>::prepropagate2(const Node3Dc<T1,T2>& node,
									   std::priority_queue<Node3Dc<T1,T2>*,
									   std::deque<Node3Dc<T1,T2>*>,
									   CompareNodePtr<T1> >& queue,
									   std::vector<bool>& inQueue,
									   std::vector<bool>& frozen,
									   int threadNo) const {
		
		// This function can be used to "prepropagate" each Tx nodes one first time
		// during "initQueue", before running "propagate".
		// When a Tx source node seems to be lost in the queue and is not
		// propagated, corrupting the entire traveltime table,
		// this function force the propagation of every source points and can
		// solve the problem.
		
		for ( size_t no=0; no<node.getOwners().size(); ++no ) {
			size_t cellNo = node.getOwners()[no];
			for ( size_t k=0; k< neighbors[cellNo].size(); ++k ) {
				size_t neibNo = neighbors[cellNo][k];
				if ( neibNo == node.getGridIndex() || frozen[neibNo] ) {
					continue;
				}
				
				// compute dt
				T1 dt = computeDt(node, nodes[neibNo], cellNo);
				
				if ( node.getTT( threadNo )+dt < nodes[neibNo].getTT( threadNo ) ) {
					nodes[neibNo].setTT( node.getTT( threadNo )+dt, threadNo );
					nodes[neibNo].setnodeParent( node.getGridIndex(), threadNo );
					nodes[neibNo].setCellParent( cellNo, threadNo );
					
					if ( !inQueue[neibNo] ) {
						queue.push( &(nodes[neibNo]) );
						inQueue[neibNo] = true;
					}
				}
			}
		}
	}
	
	
	template<typename T1, typename T2>
	T1 Grid3Dc<T1,T2>::getTraveltime(const sxyz<T1>& Rx,
									 const std::vector<Node3Dc<T1,T2> >& nodes,
									 const int threadNo) const {
		
		// Calculate and return the traveltime for a Rx point.
		for ( size_t nn=0; nn<nodes.size(); ++nn ) {
			if ( nodes[nn] == Rx ) {
				return nodes[nn].getTT(threadNo);
			}
		}
		size_t cellNo = getCellNo( Rx );
		size_t neibNo = neighbors[cellNo][0];
		T1 dt = computeDt(nodes[neibNo], Rx, cellNo);
		
		T1 traveltime = nodes[neibNo].getTT(threadNo)+dt;
		for ( size_t k=1; k< neighbors[cellNo].size(); ++k ) {
			neibNo = neighbors[cellNo][k];
			dt = computeDt(nodes[neibNo], Rx, cellNo);
			if ( traveltime > nodes[neibNo].getTT(threadNo)+dt ) {
				traveltime =  nodes[neibNo].getTT(threadNo)+dt;
			}
		}
		return traveltime;
	}
	
	template<typename T1, typename T2>
	T1 Grid3Dc<T1,T2>::getTraveltime(const sxyz<T1>& Rx,
									 const std::vector<Node3Dc<T1,T2> >& nodes,
									 T2& nodeParentRx, T2& cellParentRx,
									 const int threadNo) const {
		
		// Calculate and return the traveltime for a Rx point.
		for ( size_t nn=0; nn<nodes.size(); ++nn ) {
			if ( nodes[nn] == Rx ) {
				nodeParentRx = nodes[nn].getNodeParent(threadNo);
				cellParentRx = nodes[nn].getCellParent(threadNo);
				return nodes[nn].getTT(threadNo);
			}
		}
		size_t cellNo = getCellNo( Rx );
		size_t neibNo = neighbors[cellNo][0];
		T1 dt = computeDt(nodes[neibNo], Rx, cellNo);
		
		T1 traveltime = nodes[neibNo].getTT(threadNo)+dt;
		nodeParentRx = neibNo;
		cellParentRx = cellNo;
		for ( size_t k=1; k< neighbors[cellNo].size(); ++k ) {
			neibNo = neighbors[cellNo][k];
			dt = computeDt(nodes[neibNo], Rx, cellNo);
			if ( traveltime > nodes[neibNo].getTT(threadNo)+dt ) {
				traveltime =  nodes[neibNo].getTT(threadNo)+dt;
				nodeParentRx = neibNo;
			}
		}
		return traveltime;
	}
	
	template<typename T1, typename T2>
	void Grid3Dc<T1,T2>::save(const char filename[]) const {
		std::ofstream fout( filename );
		
		fout << Grid3D<T1,T2>::dx << ' ' << Grid3D<T1,T2>::dy << ' ' << Grid3D<T1,T2>::dz << ' ' << Grid3D<T1,T2>::xmin << ' ' << Grid3D<T1,T2>::ymin << ' '
		<< Grid3D<T1,T2>::zmin << ' ' << Grid3D<T1,T2>::xmax << ' ' << Grid3D<T1,T2>::ymax << ' '<< Grid3D<T1,T2>::zmax << '\n';
		fout << Grid3D<T1,T2>::nCellx << ' ' << Grid3D<T1,T2>::nCelly << ' ' << Grid3D<T1,T2>::nCellz << ' '
		<< Grid3D<T1,T2>::nsnx << ' ' << Grid3D<T1,T2>::nsny << ' ' << Grid3D<T1,T2>::nsnz << ' ' << '\n';
		
		fout << nodes.size() << '\n';
		for ( size_t n=0; n < nodes.size(); ++n ) {
			fout << nodes[n].getsize() ;
			for (size_t nt=0; nt< nodes[n].getsize(); nt++){
				fout << " " << nodes[n].getTT(nt) << " "
				<< nodes[n].getNodeParent(nt) << ' '<< nodes[n].getCellParent(nt);
			}
			fout << ' ' << nodes[n].getX() << ' ' << nodes[n].getY()
			<< ' ' << nodes[n].getZ() << ' '
			<< ' ' << nodes[n].getGridIndex();
			for (size_t no=0; no < nodes[n].getOwners().size(); ++no ) {
				fout << ' ' << nodes[n].getOwners()[no];
			}
			fout << '\n';
		}
		/*
		 fout << slowness.size() << '\n';
		 for ( size_t n=0; n < slowness.size(); ++n ) {
		 fout << slowness[n] << '\n';
		 }
		 fout << neighbors.size() << '\n';
		 for ( size_t n=0; n < neighbors.size(); ++n ) {
		 fout << neighbors[n].size();
		 for ( size_t nn=0; nn < neighbors[n].size(); ++nn ) {
		 fout << ' ' << neighbors[n][nn];
		 }
		 fout << '\n';
		 }
		 */
		fout.close();
	}
	
	template<typename T1, typename T2>
	void Grid3Dc<T1,T2>::dsave(const char filename[]) const {
		//Similar to 'save', with text information
		std::ofstream fout( filename );
		
		fout << "dx "<< Grid3D<T1,T2>::dx << "\t dy " << Grid3D<T1,T2>::dy << "\t dz " << Grid3D<T1,T2>::dz
		<< "\t xmin " << Grid3D<T1,T2>::xmin << "\t ymin " << Grid3D<T1,T2>::ymin << "\t zmin "	<< Grid3D<T1,T2>::zmin
		<< "\t xmax " << Grid3D<T1,T2>::xmax<< "\t ymax " << Grid3D<T1,T2>::ymax << "\t zmax "<< Grid3D<T1,T2>::zmax
		<< '\n';
		fout << "nCx " << Grid3D<T1,T2>::nCellx << "\t nCy " << Grid3D<T1,T2>::nCelly << "\t nCz " << Grid3D<T1,T2>::nCellz
		<< "\t nsnx " << Grid3D<T1,T2>::nsnx << "\t nsny " << Grid3D<T1,T2>::nsny << "\t nsnz " << Grid3D<T1,T2>::nsnz
		<< '\n';
		
		fout << "nb. nodes " << nodes.size() << '\n';
		for ( size_t n=0; n < nodes.size(); ++n ) {
			fout << "node " << nodes[n].getGridIndex() << "\t TT \t ";
			for ( size_t nt=0; nt< nodes[n].getsize(); nt++ ) {
				fout << nodes[n].getTT(nt) << "\t";
			}
			fout << " X " << nodes[n].getX() << "\t Y " << nodes[n].getY()
			<< "\t Z " << nodes[n].getZ() << "\t Ray Parent \t";
			for ( size_t nt=0; nt< nodes[n].getsize(); nt++ ) {
				fout << nodes[n].getNodeParent(nt) << '\t';
			}
			fout<< "Cell Parent \t";
			for ( size_t nt=0; nt< nodes[n].getsize(); nt++ ) {
				fout << nodes[n].getCellParent(nt) << '\t';
			}
			fout << "Owners: ";
			for ( size_t no=0; no < nodes[n].getOwners().size(); ++no ) {
				fout << '\t' << nodes[n].getOwners()[no];
			}
			fout << '\n';
		}
		/*
		 fout << "slowness size " << slowness.size() << '\n';
		 for ( size_t n=0; n < slowness.size(); ++n ) {
		 fout << slowness[n] << '\n';
		 }
		 
		 fout << "neighbors size " << neighbors.size() << '\n';
		 for ( size_t n=0; n < neighbors.size(); ++n ) {
		 fout << "neighbors[" << n << "] size " << neighbors[n].size() << " :";
		 for ( size_t nn=0; nn < neighbors[n].size(); ++nn ) {
		 fout << '\t' << neighbors[n][nn];
		 }
		 fout << '\n';
		 }
		 */
		fout.close();
	}
	
	template<typename T1, typename T2>
	void Grid3Dc<T1,T2>::savefast(const char filename[]) const {
		
		std::ofstream fout( filename );
		
		for ( size_t n=0; n < nodes.size(); ++n ) {
			if ( floor((nodes[n].getX())/Grid3D<T1,T2>::dx)==(nodes[n].getX())/Grid3D<T1,T2>::dx &&
				floor((nodes[n].getZ())/Grid3D<T1,T2>::dz) == (nodes[n].getZ())/Grid3D<T1,T2>::dz &&
				floor((nodes[n].getY())/Grid3D<T1,T2>::dy) == (nodes[n].getY())/Grid3D<T1,T2>::dy )
			{
				//		fout <<  ((nodes[n].getX())/dx)+1 << '\t' << ((nodes[n].getY())/dy)+1
				//	    << '\t' << ((nodes[n].getZ())/dz)+1 ;
				for ( size_t nt=0; nt< nodes[n].getsize(); nt++ ) {
					fout.precision(9);
					fout //<< '\t'
					<< nodes[n].getTT(nt);
				}
				fout << '\n';
			}
		}
		
		fout.close();
	}
	
	
	template<typename T1, typename T2>
	void Grid3Dc<T1,T2>::savePrimary(const char filename[], const int nt) const {
		
		std::ofstream fout( filename );
		fout.precision(9);
		
		sxyz<T1> node;
		size_t n=0;
		for ( size_t nk=0; nk<=Grid3D<T1,T2>::nCellz; ++nk ) {
			
			node.z = Grid3D<T1,T2>::zmin + nk*Grid3D<T1,T2>::dz;
			
			for ( size_t nj=0; nj<=Grid3D<T1,T2>::nCelly; ++nj ) {
				
				node.y = Grid3D<T1,T2>::ymin + nj*Grid3D<T1,T2>::dy;
				
				for (size_t ni=0; ni<=Grid3D<T1,T2>::nCellx; ++ni ) {
					
					node.x = Grid3D<T1,T2>::xmin + ni*Grid3D<T1,T2>::dx;
					
					fout << nodes[n++].getTT(nt) << '\n';
					
					// Secondary nodes on x edge
					if ( ni < Grid3D<T1,T2>::nCellx ) {
						n += Grid3D<T1,T2>::nsnx;
					}
					
					// Secondary nodes on y edge
					if ( nj < Grid3D<T1,T2>::nCelly ) {
						n += Grid3D<T1,T2>::nsny;
					}
					
					// Secondary nodes on z edge
					if ( nk < Grid3D<T1,T2>::nCellz ) {
						n += Grid3D<T1,T2>::nsnz;
					}
					
					// Secondary nodes on the xy0 planes
					if ( ni < Grid3D<T1,T2>::nCellx && nj < Grid3D<T1,T2>::nCelly ) {
						n += Grid3D<T1,T2>::nsny*Grid3D<T1,T2>::nsnx;
					}
					
					// Secondary nodes on the x0z planes
					if ( ni < Grid3D<T1,T2>::nCellx && nk < Grid3D<T1,T2>::nCellz ) {
						n += Grid3D<T1,T2>::nsnz*Grid3D<T1,T2>::nsnx;
					}
					
					// Secondary nodes on the 0yz planes
					if ( nj < Grid3D<T1,T2>::nCelly && nk < Grid3D<T1,T2>::nCellz ) {
						n += Grid3D<T1,T2>::nsnz*Grid3D<T1,T2>::nsny;
					}
				}
			}
		}
		fout.close();
	}
	
	
#endif
