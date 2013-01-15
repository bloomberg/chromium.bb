// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_MATH_UTIL_H_
#define CC_MATH_UTIL_H_

#include "base/logging.h"
#include "cc/cc_export.h"
#include "ui/gfx/point_f.h"
#include "ui/gfx/point3_f.h"
#include "ui/gfx/transform.h"

namespace gfx {
class QuadF;
class Rect;
class RectF;
class Transform;
class Vector2dF;
}

namespace cc {

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

class CC_EXPORT MathUtil {
public:
    static const double PI_DOUBLE;
    static const float PI_FLOAT;
    static const double EPSILON;

    static double Deg2Rad(double deg)  { return deg * PI_DOUBLE / 180; }
    static double Rad2Deg(double rad)  { return rad * 180 / PI_DOUBLE; }

    static float Deg2Rad(float deg)  { return deg * PI_FLOAT / 180; }
    static float Rad2Deg(float rad)  { return rad * 180 / PI_FLOAT; }

    // Background: Existing transform code does not do the right thing in
    // mapRect / mapQuad / projectQuad when there is a perspective projection that causes
    // one of the transformed vertices to go to w < 0. In those cases, it is necessary to
    // perform clipping in homogeneous coordinates, after applying the transform, before
    // dividing-by-w to convert to cartesian coordinates.
    //
    // These functions return the axis-aligned rect that encloses the correctly clipped,
    // transformed polygon.
    static gfx::Rect mapClippedRect(const gfx::Transform&, const gfx::Rect&);
    static gfx::RectF mapClippedRect(const gfx::Transform&, const gfx::RectF&);
    static gfx::RectF projectClippedRect(const gfx::Transform&, const gfx::RectF&);

    // Returns an array of vertices that represent the clipped polygon. After returning, indexes from
    // 0 to numVerticesInClippedQuad are valid in the clippedQuad array. Note that
    // numVerticesInClippedQuad may be zero, which means the entire quad was clipped, and
    // none of the vertices in the array are valid.
    static void mapClippedQuad(const gfx::Transform&, const gfx::QuadF& srcQuad, gfx::PointF clippedQuad[8], int& numVerticesInClippedQuad);

    static gfx::RectF computeEnclosingRectOfVertices(gfx::PointF vertices[], int numVertices);
    static gfx::RectF computeEnclosingClippedRect(const HomogeneousCoordinate& h1, const HomogeneousCoordinate& h2, const HomogeneousCoordinate& h3, const HomogeneousCoordinate& h4);

    // NOTE: These functions do not do correct clipping against w = 0 plane, but they
    // correctly detect the clipped condition via the boolean clipped.
    static gfx::QuadF mapQuad(const gfx::Transform&, const gfx::QuadF&, bool& clipped);
    static gfx::PointF mapPoint(const gfx::Transform&, const gfx::PointF&, bool& clipped);
    static gfx::Point3F mapPoint(const gfx::Transform&, const gfx::Point3F&, bool& clipped);
    static gfx::QuadF projectQuad(const gfx::Transform&, const gfx::QuadF&, bool& clipped);
    static gfx::PointF projectPoint(const gfx::Transform&, const gfx::PointF&, bool& clipped);

    static gfx::Vector2dF computeTransform2dScaleComponents(const gfx::Transform&, float fallbackValue);

    // Returns the smallest angle between the given two vectors in degrees. Neither vector is
    // assumed to be normalized.
    static float smallestAngleBetweenVectors(gfx::Vector2dF, gfx::Vector2dF);

    // Projects the |source| vector onto |destination|. Neither vector is assumed to be normalized.
    static gfx::Vector2dF projectVector(gfx::Vector2dF source, gfx::Vector2dF destination);
};

} // namespace cc

#endif  // CC_MATH_UTIL_H_
