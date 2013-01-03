// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layer_sorter.h"

#include <algorithm>
#include <deque>
#include <limits>
#include <vector>

#include "base/logging.h"
#include "cc/math_util.h"
#include "cc/render_surface_impl.h"
#include "ui/gfx/transform.h"

namespace cc {

inline static float perpProduct(const gfx::Vector2dF& u, const gfx::Vector2dF& v)
{
    return u.x() * v.y() - u.y() * v.x();
}

// Tests if two edges defined by their endpoints (a,b) and (c,d) intersect. Returns true and the
// point of intersection if they do and false otherwise.
static bool edgeEdgeTest(const gfx::PointF& a, const gfx::PointF& b, const gfx::PointF& c, const gfx::PointF& d, gfx::PointF& r)
{
    gfx::Vector2dF u = b - a;
    gfx::Vector2dF v = d - c;
    gfx::Vector2dF w = a - c;

    float denom = perpProduct(u, v);

    // If denom == 0 then the edges are parallel. While they could be overlapping
    // we don't bother to check here as the we'll find their intersections from the
    // corner to quad tests.
    if (!denom)
        return false;

    float s = perpProduct(v, w) / denom;
    if (s < 0 || s > 1)
        return false;

    float t = perpProduct(u, w) / denom;
    if (t < 0 || t > 1)
        return false;

    u.Scale(s);
    r = a + u;
    return true;
}

GraphNode::GraphNode(LayerImpl* layerImpl)
    : layer(layerImpl)
    , incomingEdgeWeight(0)
{
}

GraphNode::~GraphNode()
{
}

LayerSorter::LayerSorter()
    : m_zRange(0)
{
}

LayerSorter::~LayerSorter()
{
}

// Checks whether layer "a" draws on top of layer "b". The weight value returned is an indication of
// the maximum z-depth difference between the layers or zero if the layers are found to be intesecting
// (some features are in front and some are behind).
LayerSorter::ABCompareResult LayerSorter::checkOverlap(LayerShape* a, LayerShape* b, float zThreshold, float& weight)
{
    weight = 0;

    // Early out if the projected bounds don't overlap.
    if (!a->projectedBounds.Intersects(b->projectedBounds))
        return None;

    gfx::PointF aPoints[4] = {a->projectedQuad.p1(), a->projectedQuad.p2(), a->projectedQuad.p3(), a->projectedQuad.p4() };
    gfx::PointF bPoints[4] = {b->projectedQuad.p1(), b->projectedQuad.p2(), b->projectedQuad.p3(), b->projectedQuad.p4() };

    // Make a list of points that inside both layer quad projections.
    std::vector<gfx::PointF> overlapPoints;

    // Check all four corners of one layer against the other layer's quad.
    for (int i = 0; i < 4; ++i) {
        if (a->projectedQuad.Contains(bPoints[i]))
            overlapPoints.push_back(bPoints[i]);
        if (b->projectedQuad.Contains(aPoints[i]))
            overlapPoints.push_back(aPoints[i]);
    }

    // Check all the edges of one layer for intersection with the other layer's edges.
    gfx::PointF r;
    for (int ea = 0; ea < 4; ++ea)
        for (int eb = 0; eb < 4; ++eb)
            if (edgeEdgeTest(aPoints[ea], aPoints[(ea + 1) % 4],
                             bPoints[eb], bPoints[(eb + 1) % 4],
                             r))
                overlapPoints.push_back(r);

    if (overlapPoints.empty())
        return None;

    // Check the corresponding layer depth value for all overlap points to determine
    // which layer is in front.
    float maxPositive = 0;
    float maxNegative = 0;
    for (unsigned o = 0; o < overlapPoints.size(); o++) {
        float za = a->layerZFromProjectedPoint(overlapPoints[o]);
        float zb = b->layerZFromProjectedPoint(overlapPoints[o]);

        float diff = za - zb;
        if (diff > maxPositive)
            maxPositive = diff;
        if (diff < maxNegative)
            maxNegative = diff;
    }

    float maxDiff = (fabsf(maxPositive) > fabsf(maxNegative) ? maxPositive : maxNegative);

    // If the results are inconsistent (and the z difference substantial to rule out
    // numerical errors) then the layers are intersecting. We will still return an
    // order based on the maximum depth difference but with an edge weight of zero
    // these layers will get priority if a graph cycle is present and needs to be broken.
    if (maxPositive > zThreshold && maxNegative < -zThreshold)
        weight = 0;
    else
        weight = fabsf(maxDiff);

    // Maintain relative order if the layers have the same depth at all intersection points.
    if (maxDiff <= 0)
        return ABeforeB;

    return BBeforeA;
}

LayerShape::LayerShape()
{
}

LayerShape::LayerShape(float width, float height, const gfx::Transform& drawTransform)
{
    gfx::QuadF layerQuad(gfx::RectF(0, 0, width, height));

    // Compute the projection of the layer quad onto the z = 0 plane.

    gfx::PointF clippedQuad[8];
    int numVerticesInClippedQuad;
    MathUtil::mapClippedQuad(drawTransform, layerQuad, clippedQuad, numVerticesInClippedQuad);

    if (numVerticesInClippedQuad < 3) {
        projectedBounds = gfx::RectF();
        return;
    }

    projectedBounds = MathUtil::computeEnclosingRectOfVertices(clippedQuad, numVerticesInClippedQuad);

    // NOTE: it will require very significant refactoring and overhead to deal with
    // generalized polygons or multiple quads per layer here. For the sake of layer
    // sorting it is equally correct to take a subsection of the polygon that can be made
    // into a quad. This will only be incorrect in the case of intersecting layers, which
    // are not supported yet anyway.
    projectedQuad.set_p1(clippedQuad[0]);
    projectedQuad.set_p2(clippedQuad[1]);
    projectedQuad.set_p3(clippedQuad[2]);
    if (numVerticesInClippedQuad >= 4)
        projectedQuad.set_p4(clippedQuad[3]);
    else
        projectedQuad.set_p4(clippedQuad[2]); // this will be a degenerate quad that is actually a triangle.

    // Compute the normal of the layer's plane.
    bool clipped = false;
    gfx::Point3F c1 = MathUtil::mapPoint(drawTransform, gfx::Point3F(0, 0, 0), clipped);
    gfx::Point3F c2 = MathUtil::mapPoint(drawTransform, gfx::Point3F(0, 1, 0), clipped);
    gfx::Point3F c3 = MathUtil::mapPoint(drawTransform, gfx::Point3F(1, 0, 0), clipped);
    // FIXME: Deal with clipping.
    gfx::Vector3dF c12 = c2 - c1;
    gfx::Vector3dF c13 = c3 - c1;
    layerNormal = gfx::CrossProduct(c13, c12);

    transformOrigin = c1;
}

LayerShape::~LayerShape()
{
}

// Returns the Z coordinate of a point on the layer that projects
// to point p which lies on the z = 0 plane. It does it by computing the
// intersection of a line starting from p along the Z axis and the plane
// of the layer.
float LayerShape::layerZFromProjectedPoint(const gfx::PointF& p) const
{
    gfx::Vector3dF zAxis(0, 0, 1);
    gfx::Vector3dF w = gfx::Point3F(p) - transformOrigin;

    float d = gfx::DotProduct(layerNormal, zAxis);
    float n = -gfx::DotProduct(layerNormal, w);

    // Check if layer is parallel to the z = 0 axis which will make it
    // invisible and hence returning zero is fine.
    if (!d)
        return 0;

    // The intersection point would be given by:
    // p + (n / d) * u  but since we are only interested in the 
    // z coordinate and p's z coord is zero, all we need is the value of n/d.
    return n / d;
}

void LayerSorter::createGraphNodes(LayerList::iterator first, LayerList::iterator last)
{
    DVLOG(2) << "Creating graph nodes:";
    float minZ = FLT_MAX;
    float maxZ = -FLT_MAX;
    for (LayerList::const_iterator it = first; it < last; it++) {
        m_nodes.push_back(GraphNode(*it));
        GraphNode& node = m_nodes.at(m_nodes.size() - 1);
        RenderSurfaceImpl* renderSurface = node.layer->renderSurface();
        if (!node.layer->drawsContent() && !renderSurface)
            continue;

        DVLOG(2) << "Layer " << node.layer->id() << " (" << node.layer->bounds().width() << " x " << node.layer->bounds().height() << ")";

        gfx::Transform drawTransform;
        float layerWidth, layerHeight;
        if (renderSurface) {
            drawTransform = renderSurface->drawTransform();
            layerWidth = renderSurface->contentRect().width();
            layerHeight = renderSurface->contentRect().height();
        } else {
            drawTransform = node.layer->drawTransform();
            layerWidth = node.layer->contentBounds().width();
            layerHeight = node.layer->contentBounds().height();
        }

        node.shape = LayerShape(layerWidth, layerHeight, drawTransform);

        maxZ = std::max(maxZ, node.shape.transformOrigin.z());
        minZ = std::min(minZ, node.shape.transformOrigin.z());
    }

    m_zRange = fabsf(maxZ - minZ);
}

void LayerSorter::createGraphEdges()
{
    DVLOG(2) << "Edges:";
    // Fraction of the total zRange below which z differences
    // are not considered reliable.
    const float zThresholdFactor = 0.01f;
    float zThreshold = m_zRange * zThresholdFactor;

    for (unsigned na = 0; na < m_nodes.size(); na++) {
        GraphNode& nodeA = m_nodes[na];
        if (!nodeA.layer->drawsContent() && !nodeA.layer->renderSurface())
            continue;
        for (unsigned nb = na + 1; nb < m_nodes.size(); nb++) {
            GraphNode& nodeB = m_nodes[nb];
            if (!nodeB.layer->drawsContent() && !nodeB.layer->renderSurface())
                continue;
            float weight = 0;
            ABCompareResult overlapResult = checkOverlap(&nodeA.shape, &nodeB.shape, zThreshold, weight);
            GraphNode* startNode = 0;
            GraphNode* endNode = 0;
            if (overlapResult == ABeforeB) {
                startNode = &nodeA;
                endNode = &nodeB;
            } else if (overlapResult == BBeforeA) {
                startNode = &nodeB;
                endNode = &nodeA;
            }

            if (startNode) {
                DVLOG(2) << startNode->layer->id() << " -> " << endNode->layer->id();
                m_edges.push_back(GraphEdge(startNode, endNode, weight));
            }
        }
    }

    for (unsigned i = 0; i < m_edges.size(); i++) {
        GraphEdge& edge = m_edges[i];
        m_activeEdges[&edge] = &edge;
        edge.from->outgoing.push_back(&edge);
        edge.to->incoming.push_back(&edge);
        edge.to->incomingEdgeWeight += edge.weight;
    }
}

// Finds and removes an edge from the list by doing a swap with the
// last element of the list.
void LayerSorter::removeEdgeFromList(GraphEdge* edge, std::vector<GraphEdge*>& list)
{
    std::vector<GraphEdge*>::iterator iter = std::find(list.begin(), list.end(), edge);
    DCHECK(iter != list.end());
    list.erase(iter);
}

// Sorts the given list of layers such that they can be painted in a back-to-front
// order. Sorting produces correct results for non-intersecting layers that don't have
// cyclical order dependencies. Cycles and intersections are broken (somewhat) aribtrarily.
// Sorting of layers is done via a topological sort of a directed graph whose nodes are
// the layers themselves. An edge from node A to node B signifies that layer A needs to
// be drawn before layer B. If A and B have no dependency between each other, then we
// preserve the ordering of those layers as they were in the original list.
//
// The draw order between two layers is determined by projecting the two triangles making
// up each layer quad to the Z = 0 plane, finding points of intersection between the triangles
// and backprojecting those points to the plane of the layer to determine the corresponding Z
// coordinate. The layer with the lower Z coordinate (farther from the eye) needs to be rendered
// first.
//
// If the layer projections don't intersect, then no edges (dependencies) are created
// between them in the graph. HOWEVER, in this case we still need to preserve the ordering
// of the original list of layers, since that list should already have proper z-index
// ordering of layers.
//
void LayerSorter::sort(LayerList::iterator first, LayerList::iterator last)
{
    DVLOG(2) << "Sorting start ----";
    createGraphNodes(first, last);

    createGraphEdges();

    std::vector<GraphNode*> sortedList;
    std::deque<GraphNode*> noIncomingEdgeNodeList;

    // Find all the nodes that don't have incoming edges.
    for (NodeList::iterator la = m_nodes.begin(); la < m_nodes.end(); la++) {
        if (!la->incoming.size())
            noIncomingEdgeNodeList.push_back(&(*la));
    }

    DVLOG(2) << "Sorted list: ";
    while (m_activeEdges.size() || noIncomingEdgeNodeList.size()) {
        while (noIncomingEdgeNodeList.size()) {

            // It is necessary to preserve the existing ordering of layers, when there are
            // no explicit dependencies (because this existing ordering has correct
            // z-index/layout ordering). To preserve this ordering, we process Nodes in
            // the same order that they were added to the list.
            GraphNode* fromNode = noIncomingEdgeNodeList.front();
            noIncomingEdgeNodeList.pop_front();

            // Add it to the final list.
            sortedList.push_back(fromNode);

            DVLOG(2) << fromNode->layer->id() << ", ";

            // Remove all its outgoing edges from the graph.
            for (unsigned i = 0; i < fromNode->outgoing.size(); i++) {
                GraphEdge* outgoingEdge = fromNode->outgoing[i];

                m_activeEdges.erase(outgoingEdge);
                removeEdgeFromList(outgoingEdge, outgoingEdge->to->incoming);
                outgoingEdge->to->incomingEdgeWeight -= outgoingEdge->weight;

                if (!outgoingEdge->to->incoming.size())
                    noIncomingEdgeNodeList.push_back(outgoingEdge->to);
            }
            fromNode->outgoing.clear();
        }

        if (!m_activeEdges.size())
            break;

        // If there are still active edges but the list of nodes without incoming edges
        // is empty then we have run into a cycle. Break the cycle by finding the node
        // with the smallest overall incoming edge weight and use it. This will favor
        // nodes that have zero-weight incoming edges i.e. layers that are being
        // occluded by a layer that intersects them.
        float minIncomingEdgeWeight = FLT_MAX;
        GraphNode* nextNode = 0;
        for (unsigned i = 0; i < m_nodes.size(); i++) {
            if (m_nodes[i].incoming.size() && m_nodes[i].incomingEdgeWeight < minIncomingEdgeWeight) {
                minIncomingEdgeWeight = m_nodes[i].incomingEdgeWeight;
                nextNode = &m_nodes[i];
            }
        }
        DCHECK(nextNode);
        // Remove all its incoming edges.
        for (unsigned e = 0; e < nextNode->incoming.size(); e++) {
            GraphEdge* incomingEdge = nextNode->incoming[e];

            m_activeEdges.erase(incomingEdge);
            removeEdgeFromList(incomingEdge, incomingEdge->from->outgoing);
        }
        nextNode->incoming.clear();
        nextNode->incomingEdgeWeight = 0;
        noIncomingEdgeNodeList.push_back(nextNode);
        DVLOG(2) << "Breaking cycle by cleaning up incoming edges from " << nextNode->layer->id() << " (weight = " << minIncomingEdgeWeight << ")";
    }

    // Note: The original elements of the list are in no danger of having their ref count go to zero
    // here as they are all nodes of the layer hierarchy and are kept alive by their parent nodes.
    int count = 0;
    for (LayerList::iterator it = first; it < last; it++)
        *it = sortedList[count++]->layer;

    DVLOG(2) << "Sorting end ----";

    m_nodes.clear();
    m_edges.clear();
    m_activeEdges.clear();
}

}  // namespace cc
