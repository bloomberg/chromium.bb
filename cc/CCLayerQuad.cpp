// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "CCLayerQuad.h"

namespace cc {

CCLayerQuad::Edge::Edge(const FloatPoint& p, const FloatPoint& q)
{
    ASSERT(p != q);

    FloatPoint tangent(p.y() - q.y(), q.x() - p.x());
    float cross2 = p.x() * q.y() - q.x() * p.y();

    set(tangent.x(), tangent.y(), cross2);
    scale(1.0f / tangent.length());
}

CCLayerQuad::CCLayerQuad(const FloatQuad& quad)
{
    // Create edges.
    m_left = Edge(quad.p4(), quad.p1());
    m_right = Edge(quad.p2(), quad.p3());
    m_top = Edge(quad.p1(), quad.p2());
    m_bottom = Edge(quad.p3(), quad.p4());

    float sign = quad.isCounterclockwise() ? -1 : 1;
    m_left.scale(sign);
    m_right.scale(sign);
    m_top.scale(sign);
    m_bottom.scale(sign);
}

FloatQuad CCLayerQuad::floatQuad() const
{
    return FloatQuad(m_left.intersect(m_top),
                     m_top.intersect(m_right),
                     m_right.intersect(m_bottom),
                     m_bottom.intersect(m_left));
}

void CCLayerQuad::toFloatArray(float flattened[12]) const
{
    flattened[0] = m_left.x();
    flattened[1] = m_left.y();
    flattened[2] = m_left.z();
    flattened[3] = m_top.x();
    flattened[4] = m_top.y();
    flattened[5] = m_top.z();
    flattened[6] = m_right.x();
    flattened[7] = m_right.y();
    flattened[8] = m_right.z();
    flattened[9] = m_bottom.x();
    flattened[10] = m_bottom.y();
    flattened[11] = m_bottom.z();
}

} // namespace cc

#endif // USE(ACCELERATED_COMPOSITING)
