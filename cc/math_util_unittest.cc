// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/math_util.h"

#include <cmath>

#include "cc/test/geometry_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/rect_f.h"
#include "ui/gfx/transform.h"

namespace cc {
namespace {

TEST(MathUtilTest, verifyProjectionOfPerpendicularPlane)
{
    // In this case, the m33() element of the transform becomes zero, which could cause a
    // divide-by-zero when projecting points/quads.

    gfx::Transform transform;
    transform.MakeIdentity();
    transform.matrix().setDouble(2, 2, 0);

    gfx::RectF rect = gfx::RectF(0, 0, 1, 1);
    gfx::RectF projectedRect = MathUtil::projectClippedRect(transform, rect);

    EXPECT_EQ(0, projectedRect.x());
    EXPECT_EQ(0, projectedRect.y());
    EXPECT_TRUE(projectedRect.IsEmpty());
}

TEST(MathUtilTest, verifyEnclosingClippedRectUsesCorrectInitialBounds)
{
    HomogeneousCoordinate h1(-100, -100, 0, 1);
    HomogeneousCoordinate h2(-10, -10, 0, 1);
    HomogeneousCoordinate h3(10, 10, 0, -1);
    HomogeneousCoordinate h4(100, 100, 0, -1);

    // The bounds of the enclosing clipped rect should be -100 to -10 for both x and y.
    // However, if there is a bug where the initial xmin/xmax/ymin/ymax are initialized to
    // numeric_limits<float>::min() (which is zero, not -flt_max) then the enclosing
    // clipped rect will be computed incorrectly.
    gfx::RectF result = MathUtil::computeEnclosingClippedRect(h1, h2, h3, h4);

    EXPECT_FLOAT_RECT_EQ(gfx::RectF(gfx::PointF(-100, -100), gfx::SizeF(90, 90)), result);
}

TEST(MathUtilTest, verifyEnclosingRectOfVerticesUsesCorrectInitialBounds)
{
    gfx::PointF vertices[3];
    int numVertices = 3;

    vertices[0] = gfx::PointF(-10, -100);
    vertices[1] = gfx::PointF(-100, -10);
    vertices[2] = gfx::PointF(-30, -30);

    // The bounds of the enclosing rect should be -100 to -10 for both x and y. However,
    // if there is a bug where the initial xmin/xmax/ymin/ymax are initialized to
    // numeric_limits<float>::min() (which is zero, not -flt_max) then the enclosing
    // clipped rect will be computed incorrectly.
    gfx::RectF result = MathUtil::computeEnclosingRectOfVertices(vertices, numVertices);

    EXPECT_FLOAT_RECT_EQ(gfx::RectF(gfx::PointF(-100, -100), gfx::SizeF(90, 90)), result);
}

TEST(MathUtilTest, smallestAngleBetweenVectors)
{
    gfx::Vector2dF x(1, 0);
    gfx::Vector2dF y(0, 1);
    gfx::Vector2dF testVector(0.5, 0.5);

    // Orthogonal vectors are at an angle of 90 degress.
    EXPECT_EQ(90, MathUtil::smallestAngleBetweenVectors(x, y));

    // A vector makes a zero angle with itself.
    EXPECT_EQ(0, MathUtil::smallestAngleBetweenVectors(x, x));
    EXPECT_EQ(0, MathUtil::smallestAngleBetweenVectors(y, y));
    EXPECT_EQ(0, MathUtil::smallestAngleBetweenVectors(testVector, testVector));

    // Parallel but reversed vectors are at 180 degrees.
    EXPECT_FLOAT_EQ(180, MathUtil::smallestAngleBetweenVectors(x, -x));
    EXPECT_FLOAT_EQ(180, MathUtil::smallestAngleBetweenVectors(y, -y));
    EXPECT_FLOAT_EQ(180, MathUtil::smallestAngleBetweenVectors(testVector, -testVector));

    // The test vector is at a known angle.
    EXPECT_FLOAT_EQ(45, std::floor(MathUtil::smallestAngleBetweenVectors(testVector, x)));
    EXPECT_FLOAT_EQ(45, std::floor(MathUtil::smallestAngleBetweenVectors(testVector, y)));
}

TEST(MathUtilTest, vectorProjection)
{
    gfx::Vector2dF x(1, 0);
    gfx::Vector2dF y(0, 1);
    gfx::Vector2dF testVector(0.3f, 0.7f);

    // Orthogonal vectors project to a zero vector.
    EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), MathUtil::projectVector(x, y));
    EXPECT_VECTOR_EQ(gfx::Vector2dF(0, 0), MathUtil::projectVector(y, x));

    // Projecting a vector onto the orthonormal basis gives the corresponding component of the
    // vector.
    EXPECT_VECTOR_EQ(gfx::Vector2dF(testVector.x(), 0), MathUtil::projectVector(testVector, x));
    EXPECT_VECTOR_EQ(gfx::Vector2dF(0, testVector.y()), MathUtil::projectVector(testVector, y));

    // Finally check than an arbitrary vector projected to another one gives a vector parallel to
    // the second vector.
    gfx::Vector2dF targetVector(0.5, 0.2f);
    gfx::Vector2dF projectedVector = MathUtil::projectVector(testVector, targetVector);
    EXPECT_EQ(projectedVector.x() / targetVector.x(),
              projectedVector.y() / targetVector.y());
}

}  // namespace
}  // namespace cc
