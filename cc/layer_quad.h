// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CC_LAYER_QUAD_H_
#define CC_LAYER_QUAD_H_

#include "cc/cc_export.h"
#include "ui/gfx/point_f.h"

namespace gfx {
class QuadF;
}

static const float kAntiAliasingInflateDistance = 0.5f;

namespace cc {

class CC_EXPORT LayerQuad {
public:
    class Edge {
    public:
        Edge()
            : m_x(0)
            , m_y(0)
            , m_z(0)
        {
        }
        Edge(const gfx::PointF&, const gfx::PointF&);

        float x() const { return m_x; }
        float y() const { return m_y; }
        float z() const { return m_z; }

        void setX(float x) { m_x = x; }
        void setY(float y) { m_y = y; }
        void setZ(float z) { m_z = z; }
        void set(float x, float y, float z)
        {
            m_x = x;
            m_y = y;
            m_z = z;
        }

        void moveX(float dx) { m_x += dx; }
        void moveY(float dy) { m_y += dy; }
        void moveZ(float dz) { m_z += dz; }
        void move(float dx, float dy, float dz)
        {
            m_x += dx;
            m_y += dy;
            m_z += dz;
        }

        void scaleX(float sx) { m_x *= sx; }
        void scaleY(float sy) { m_y *= sy; }
        void scaleZ(float sz) { m_z *= sz; }
        void scale(float sx, float sy, float sz)
        {
            m_x *= sx;
            m_y *= sy;
            m_z *= sz;
        }
        void scale(float s) { scale(s, s, s); }

        gfx::PointF intersect(const Edge& e) const
        {
            return gfx::PointF(
                (y() * e.z() - e.y() * z()) / (x() * e.y() - e.x() * y()),
                (x() * e.z() - e.x() * z()) / (e.x() * y() - x() * e.y()));
        }

    private:
        float m_x;
        float m_y;
        float m_z;
    };

    LayerQuad(const Edge& left, const Edge& top, const Edge& right, const Edge& bottom);
    LayerQuad(const gfx::QuadF&);

    Edge left() const { return m_left; }
    Edge top() const { return m_top; }
    Edge right() const { return m_right; }
    Edge bottom() const { return m_bottom; }

    void inflateX(float dx) { m_left.moveZ(dx); m_right.moveZ(dx); }
    void inflateY(float dy) { m_top.moveZ(dy); m_bottom.moveZ(dy); }
    void inflate(float d) { inflateX(d); inflateY(d); }
    void inflateAntiAliasingDistance() { inflate(kAntiAliasingInflateDistance); }

    gfx::QuadF ToQuadF() const;

    void toFloatArray(float[12]) const;

private:
    Edge m_left;
    Edge m_top;
    Edge m_right;
    Edge m_bottom;
};

}

#endif  // CC_LAYER_QUAD_H_
