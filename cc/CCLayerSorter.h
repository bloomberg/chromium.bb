// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCLayerSorter_h
#define CCLayerSorter_h

#include "CCLayerImpl.h"
#include "FloatPoint3D.h"
#include "FloatQuad.h"
#include "FloatRect.h"
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace WebKit {
class WebTransformationMatrix;
}

namespace cc {

class CCLayerSorter {
    WTF_MAKE_NONCOPYABLE(CCLayerSorter);
public:
    CCLayerSorter() : m_zRange(0) { }

    typedef Vector<CCLayerImpl*> LayerList;

    void sort(LayerList::iterator first, LayerList::iterator last);

    // Holds various useful properties derived from a layer's 3D outline.
    struct LayerShape {
        LayerShape() { }
        LayerShape(float width, float height, const WebKit::WebTransformationMatrix& drawTransform);

        float layerZFromProjectedPoint(const FloatPoint&) const;

        FloatPoint3D layerNormal;
        FloatPoint3D transformOrigin;
        FloatQuad projectedQuad;
        FloatRect projectedBounds;
    };

    enum ABCompareResult {
        ABeforeB,
        BBeforeA,
        None
    };

    static ABCompareResult checkOverlap(LayerShape*, LayerShape*, float zThreshold, float& weight);

private:
    struct GraphEdge;

    struct GraphNode {
        explicit GraphNode(CCLayerImpl* cclayer) : layer(cclayer), incomingEdgeWeight(0) { }

        CCLayerImpl* layer;
        LayerShape shape;
        Vector<GraphEdge*> incoming;
        Vector<GraphEdge*> outgoing;
        float incomingEdgeWeight;
    };

    struct GraphEdge {
        GraphEdge(GraphNode* fromNode, GraphNode* toNode, float weight) : from(fromNode), to(toNode), weight(weight) { };

        GraphNode* from;
        GraphNode* to;
        float weight;
    };

    typedef Vector<GraphNode> NodeList;
    typedef Vector<GraphEdge> EdgeList;
    NodeList m_nodes;
    EdgeList m_edges;
    float m_zRange;

    typedef HashMap<GraphEdge*, GraphEdge*> EdgeMap;
    EdgeMap m_activeEdges;

    void createGraphNodes(LayerList::iterator first, LayerList::iterator last);
    void createGraphEdges();
    void removeEdgeFromList(GraphEdge*, Vector<GraphEdge*>&);
};

}
#endif
