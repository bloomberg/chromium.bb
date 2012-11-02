// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCMathUtil_h
#define CCMathUtil_h

#include "base/logging.h"
#include "ui/gfx/point_f.h"
#include "ui/gfx/point3_f.h"

namespace WebKit {
class WebTransformationMatrix;
}

namespace gfx {
class QuadF;
class Rect;
class RectF;
}

namespace cc {

class FloatSize;

struct HomogeneousCoordinate {
    HomogeneousCoordinate(double newX, double newY, double newZ, double newW)
        : x(newX)
        , y(newY)
        , z(newZ)
        , w(newW)
    {
    }

    bool shouldBeClipped() const
    {
        return w <= 0;
    }

    gfx::PointF cartesianPoint2d() const
    {
        if (w == 1)
            return gfx::PointF(x, y);

        // For now, because this code is used privately only by MathUtil, it should never be called when w == 0, and we do not yet need to handle that case.
        DCHECK(w);
        double invW = 1.0 / w;
        return gfx::PointF(x * invW, y * invW);
    }

    gfx::Point3F cartesianPoint3d() const
    {
        if (w == 1)
            return gfx::Point3F(x, y, z);

        // For now, because this code is used privately only by MathUtil, it should never be called when w == 0, and we do not yet need to handle that case.
        DCHECK(w);
        double invW = 1.0 / w;
        return gfx::Point3F(x * invW, y * invW, z * invW);
    }

    double x;
    double y;
    double z;
    double w;
};

class MathUtil {
public:

    // Background: WebTransformationMatrix code in WebCore does not do the right thing in
    // mapRect / mapQuad / projectQuad when there is a perspective projection that causes
    // one of the transformed vertices to go to w < 0. In those cases, it is necessary to
    // perform clipping in homogeneous coordinates, after applying the transform, before
    // dividing-by-w to convert to cartesian coordinates.
    //
    // These functions return the axis-aligned rect that encloses the correctly clipped,
    // transformed polygon.
    static gfx::Rect mapClippedRect(const WebKit::WebTransformationMatrix&, const gfx::Rect&);
    static gfx::RectF mapClippedRect(const WebKit::WebTransformationMatrix&, const gfx::RectF&);
    static gfx::RectF projectClippedRect(const WebKit::WebTransformationMatrix&, const gfx::RectF&);

    // Returns an array of vertices that represent the clipped polygon. After returning, indexes from
    // 0 to numVerticesInClippedQuad are valid in the clippedQuad array. Note that
    // numVerticesInClippedQuad may be zero, which means the entire quad was clipped, and
    // none of the vertices in the array are valid.
    static void mapClippedQuad(const WebKit::WebTransformationMatrix&, const gfx::QuadF& srcQuad, gfx::PointF clippedQuad[8], int& numVerticesInClippedQuad);

    static gfx::RectF computeEnclosingRectOfVertices(gfx::PointF vertices[], int numVertices);
    static gfx::RectF computeEnclosingClippedRect(const HomogeneousCoordinate& h1, const HomogeneousCoordinate& h2, const HomogeneousCoordinate& h3, const HomogeneousCoordinate& h4);

    // NOTE: These functions do not do correct clipping against w = 0 plane, but they
    // correctly detect the clipped condition via the boolean clipped.
    static gfx::QuadF mapQuad(const WebKit::WebTransformationMatrix&, const gfx::QuadF&, bool& clipped);
    static gfx::PointF mapPoint(const WebKit::WebTransformationMatrix&, const gfx::PointF&, bool& clipped);
    static gfx::Point3F mapPoint(const WebKit::WebTransformationMatrix&, const gfx::Point3F&, bool& clipped);
    static gfx::QuadF projectQuad(const WebKit::WebTransformationMatrix&, const gfx::QuadF&, bool& clipped);
    static gfx::PointF projectPoint(const WebKit::WebTransformationMatrix&, const gfx::PointF&, bool& clipped);

    static void flattenTransformTo2d(WebKit::WebTransformationMatrix&);

    static gfx::Vector2dF computeTransform2dScaleComponents(const WebKit::WebTransformationMatrix&);

    // Returns the smallest angle between the given two vectors in degrees. Neither vector is
    // assumed to be normalized.
    static float smallestAngleBetweenVectors(const FloatSize&, const FloatSize&);

    // Projects the |source| vector onto |destination|. Neither vector is assumed to be normalized.
    static FloatSize projectVector(const FloatSize& source, const FloatSize& destination);
};

} // namespace cc

#endif // #define CCMathUtil_h
