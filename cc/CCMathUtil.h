// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCMathUtil_h
#define CCMathUtil_h

#include "FloatPoint.h"
#include "FloatPoint3D.h"

namespace WebKit {
class WebTransformationMatrix;
}

namespace cc {

class IntRect;
class FloatRect;
class FloatQuad;

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

    FloatPoint cartesianPoint2d() const
    {
        if (w == 1)
            return FloatPoint(x, y);

        // For now, because this code is used privately only by CCMathUtil, it should never be called when w == 0, and we do not yet need to handle that case.
        ASSERT(w);
        double invW = 1.0 / w;
        return FloatPoint(x * invW, y * invW);
    }

    FloatPoint3D cartesianPoint3d() const
    {
        if (w == 1)
            return FloatPoint3D(x, y, z);

        // For now, because this code is used privately only by CCMathUtil, it should never be called when w == 0, and we do not yet need to handle that case.
        ASSERT(w);
        double invW = 1.0 / w;
        return FloatPoint3D(x * invW, y * invW, z * invW);
    }

    double x;
    double y;
    double z;
    double w;
};

class CCMathUtil {
public:

    // Background: WebTransformationMatrix code in WebCore does not do the right thing in
    // mapRect / mapQuad / projectQuad when there is a perspective projection that causes
    // one of the transformed vertices to go to w < 0. In those cases, it is necessary to
    // perform clipping in homogeneous coordinates, after applying the transform, before
    // dividing-by-w to convert to cartesian coordinates.
    //
    // These functions return the axis-aligned rect that encloses the correctly clipped,
    // transformed polygon.
    static IntRect mapClippedRect(const WebKit::WebTransformationMatrix&, const IntRect&);
    static FloatRect mapClippedRect(const WebKit::WebTransformationMatrix&, const FloatRect&);
    static FloatRect projectClippedRect(const WebKit::WebTransformationMatrix&, const FloatRect&);

    // Returns an array of vertices that represent the clipped polygon. After returning, indexes from
    // 0 to numVerticesInClippedQuad are valid in the clippedQuad array. Note that
    // numVerticesInClippedQuad may be zero, which means the entire quad was clipped, and
    // none of the vertices in the array are valid.
    static void mapClippedQuad(const WebKit::WebTransformationMatrix&, const FloatQuad& srcQuad, FloatPoint clippedQuad[8], int& numVerticesInClippedQuad);

    static FloatRect computeEnclosingRectOfVertices(FloatPoint vertices[], int numVertices);
    static FloatRect computeEnclosingClippedRect(const HomogeneousCoordinate& h1, const HomogeneousCoordinate& h2, const HomogeneousCoordinate& h3, const HomogeneousCoordinate& h4);

    // NOTE: These functions do not do correct clipping against w = 0 plane, but they
    // correctly detect the clipped condition via the boolean clipped.
    static FloatQuad mapQuad(const WebKit::WebTransformationMatrix&, const FloatQuad&, bool& clipped);
    static FloatPoint mapPoint(const WebKit::WebTransformationMatrix&, const FloatPoint&, bool& clipped);
    static FloatPoint3D mapPoint(const WebKit::WebTransformationMatrix&, const FloatPoint3D&, bool& clipped);
    static FloatQuad projectQuad(const WebKit::WebTransformationMatrix&, const FloatQuad&, bool& clipped);
    static FloatPoint projectPoint(const WebKit::WebTransformationMatrix&, const FloatPoint&, bool& clipped);

    static void flattenTransformTo2d(WebKit::WebTransformationMatrix&);

    // Returns the smallest angle between the given two vectors in degrees. Neither vector is
    // assumed to be normalized.
    static float smallestAngleBetweenVectors(const FloatSize&, const FloatSize&);

    // Projects the |source| vector onto |destination|. Neither vector is assumed to be normalized.
    static FloatSize projectVector(const FloatSize& source, const FloatSize& destination);
};

} // namespace cc

#endif // #define CCMathUtil_h
