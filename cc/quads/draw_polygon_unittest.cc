// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// We would like to use M_PI on windows too.
#ifdef _WIN32
#define _USE_MATH_DEFINES
#endif

#include <stddef.h>

#include <limits>
#include <vector>

#include "cc/output/bsp_compare_result.h"
#include "cc/quads/draw_polygon.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/transform.h"

namespace cc {

#if !defined(OS_WIN)
void DrawPolygon::RecomputeNormalForTesting() {
  ConstructNormal();
}
#endif

namespace {

#define CREATE_NEW_DRAW_POLYGON(name, points_vector, normal, polygon_id) \
  DrawPolygon name(NULL, points_vector, normal, polygon_id)

#define CREATE_TEST_DRAW_POLYGON(name, points_vector, polygon_id)             \
  DrawPolygon name(NULL, points_vector, gfx::Vector3dF(1, 2, 3), polygon_id); \
  name.RecomputeNormalForTesting()

#define EXPECT_FLOAT_WITHIN_EPSILON_OF(a, b) \
  EXPECT_TRUE(std::abs(a - b) < std::numeric_limits<float>::epsilon());

#define EXPECT_POINT_EQ(point_a, point_b)    \
  EXPECT_FLOAT_EQ(point_a.x(), point_b.x()); \
  EXPECT_FLOAT_EQ(point_a.y(), point_b.y()); \
  EXPECT_FLOAT_EQ(point_a.z(), point_b.z());

#define EXPECT_NORMAL(poly, n_x, n_y, n_z)                \
  EXPECT_FLOAT_WITHIN_EPSILON_OF(poly.normal().x(), n_x); \
  EXPECT_FLOAT_WITHIN_EPSILON_OF(poly.normal().y(), n_y); \
  EXPECT_FLOAT_WITHIN_EPSILON_OF(poly.normal().z(), n_z);

static void ValidatePoints(const DrawPolygon& polygon,
                           const std::vector<gfx::Point3F>& points) {
  EXPECT_EQ(polygon.points().size(), points.size());
  for (size_t i = 0; i < points.size(); i++) {
    EXPECT_POINT_EQ(polygon.points()[i], points[i]);
  }
}

// A simple square in a plane.
TEST(DrawPolygonConstructionTest, NormalNormal) {
  gfx::Transform Identity;
  DrawPolygon polygon(NULL, gfx::RectF(10.0f, 10.0f), Identity, 1);
  EXPECT_NORMAL(polygon, 0.0f, 0.0f, 1.0f);
}

// More complicated shapes.
TEST(DrawPolygonConstructionTest, TestNormal) {
  std::vector<gfx::Point3F> vertices;
  vertices.push_back(gfx::Point3F(0.0f, 10.0f, 0.0f));
  vertices.push_back(gfx::Point3F(0.0f, 0.0f, 0.0f));
  vertices.push_back(gfx::Point3F(10.0f, 0.0f, 0.0f));
  vertices.push_back(gfx::Point3F(10.0f, 10.0f, 0.0f));

  CREATE_TEST_DRAW_POLYGON(polygon, vertices, 1);
  EXPECT_NORMAL(polygon, 0.0f, 0.0f, 1.0f);
}

TEST(DrawPolygonConstructionTest, InverseNormal) {
  std::vector<gfx::Point3F> vertices;
  vertices.push_back(gfx::Point3F(0.0f, 10.0f, 0.0f));
  vertices.push_back(gfx::Point3F(10.0f, 10.0f, 0.0f));
  vertices.push_back(gfx::Point3F(10.0f, 0.0f, 0.0f));
  vertices.push_back(gfx::Point3F(0.0f, 0.0f, 0.0f));

  CREATE_TEST_DRAW_POLYGON(polygon, vertices, 1);
  EXPECT_NORMAL(polygon, 0.0f, 0.0f, -1.0f);
}

TEST(DrawPolygonConstructionTest, ClippedNormal) {
  std::vector<gfx::Point3F> vertices;
  vertices.push_back(gfx::Point3F(0.1f, 10.0f, 0.0f));
  vertices.push_back(gfx::Point3F(0.0f, 9.9f, 0.0f));
  vertices.push_back(gfx::Point3F(0.0f, 10.0f, 0.0f));
  vertices.push_back(gfx::Point3F(0.0f, 0.0f, 0.0f));
  vertices.push_back(gfx::Point3F(10.0f, 0.0f, 0.0f));
  vertices.push_back(gfx::Point3F(10.0f, 10.0f, 0.0f));

  CREATE_TEST_DRAW_POLYGON(polygon, vertices, 1);
  EXPECT_NORMAL(polygon, 0.0f, 0.0f, 1.0f);
}

TEST(DrawPolygonConstructionTest, SlimTriangleNormal) {
  std::vector<gfx::Point3F> vertices;
  vertices.push_back(gfx::Point3F(0.0f, 0.0f, 0.0f));
  vertices.push_back(gfx::Point3F(5000.0f, 0.0f, 0.0f));
  vertices.push_back(gfx::Point3F(10000.0f, 1.0f, 0.0f));

  CREATE_TEST_DRAW_POLYGON(polygon, vertices, 2);
  EXPECT_NORMAL(polygon, 0.0f, 0.0f, 1.0f);
}

TEST(DrawPolygonConstructionTest, ManyVertexNormal) {
  std::vector<gfx::Point3F> vertices_c;
  std::vector<gfx::Point3F> vertices_d;
  for (int i = 0; i < 100; i++) {
    vertices_c.push_back(
        gfx::Point3F(cos(i * M_PI / 50), sin(i * M_PI / 50), 0.0f));
    vertices_d.push_back(gfx::Point3F(cos(i * M_PI / 50) + 99.0f,
                                      sin(i * M_PI / 50) + 99.0f, 100.0f));
  }
  CREATE_TEST_DRAW_POLYGON(polygon_c, vertices_c, 3);
  EXPECT_NORMAL(polygon_c, 0.0f, 0.0f, 1.0f);

  CREATE_TEST_DRAW_POLYGON(polygon_d, vertices_d, 4);
  EXPECT_NORMAL(polygon_c, 0.0f, 0.0f, 1.0f);
}

// A simple rect being transformed.
TEST(DrawPolygonConstructionTest, DizzyNormal) {
  gfx::RectF src(-0.1f, -10.0f, 0.2f, 20.0f);

  gfx::Transform transform_i(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
  DrawPolygon polygon_i(NULL, src, transform_i, 1);

  EXPECT_NORMAL(polygon_i, 0.0f, 0.0f, 1.0f);

  gfx::Transform tranform_a(0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
  DrawPolygon polygon_a(NULL, src, tranform_a, 2);
  EXPECT_NORMAL(polygon_a, 0.0f, 0.0f, -1.0f);

  gfx::Transform tranform_b(0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1);
  DrawPolygon polygon_b(NULL, src, tranform_b, 3);
  EXPECT_NORMAL(polygon_b, -1.0f, 0.0f, 0.0f);

  gfx::Transform tranform_c(1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1);
  DrawPolygon polygon_c(NULL, src, tranform_c, 4);
  EXPECT_NORMAL(polygon_c, 0.0f, -1.0f, 0.0f);

  gfx::Transform tranform_d(-1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
  DrawPolygon polygon_d(NULL, src, tranform_d, 5);
  EXPECT_NORMAL(polygon_d, 0.0f, 0.0f, -1.0f);
}

// Two quads are nearly touching but definitely ordered. Second one should
// compare in front.
TEST(DrawPolygonSplitTest, NearlyTouchingOrder) {
  std::vector<gfx::Point3F> vertices_a;
  vertices_a.push_back(gfx::Point3F(0.0f, 10.0f, 0.0f));
  vertices_a.push_back(gfx::Point3F(0.0f, 0.0f, 0.0f));
  vertices_a.push_back(gfx::Point3F(10.0f, 0.0f, 0.0f));
  vertices_a.push_back(gfx::Point3F(10.0f, 10.0f, 0.0f));
  std::vector<gfx::Point3F> vertices_b;
  vertices_b.push_back(gfx::Point3F(0.0f, 10.0f, -1.0f));
  vertices_b.push_back(gfx::Point3F(0.0f, 0.0f, -1.0f));
  vertices_b.push_back(gfx::Point3F(10.0f, 0.0f, -1.0f));
  vertices_b.push_back(gfx::Point3F(10.0f, 10.0f, -1.0f));
  gfx::Vector3dF normal(0.0f, 0.0f, 1.0f);

  CREATE_NEW_DRAW_POLYGON(polygon_a, vertices_a, normal, 0);
  CREATE_NEW_DRAW_POLYGON(polygon_b, vertices_b, normal, 1);

  EXPECT_EQ(BSP_BACK, DrawPolygon::SideCompare(polygon_b, polygon_a));
}

// Two quads are definitely not touching and so no split should occur.
TEST(DrawPolygonSplitTest, NotClearlyInFront) {
  std::vector<gfx::Point3F> vertices_a;
  vertices_a.push_back(gfx::Point3F(87.2f, 1185.0f, 0.9f));
  vertices_a.push_back(gfx::Point3F(288.3f, 1185.0f, -0.7f));
  vertices_a.push_back(gfx::Point3F(288.3f, 1196.0f, -0.7f));
  vertices_a.push_back(gfx::Point3F(87.2f, 1196.0f, 0.9f));
  gfx::Vector3dF normal_a = gfx::CrossProduct(vertices_a[1] - vertices_a[0],
                                              vertices_a[1] - vertices_a[2]);
  normal_a.Scale(1.0f / normal_a.Length());

  std::vector<gfx::Point3F> vertices_b;
  vertices_b.push_back(gfx::Point3F(62.1f, 1034.7f, 1.0f));
  vertices_b.push_back(gfx::Point3F(313.4f, 1035.3f, -1.0f));
  vertices_b.push_back(gfx::Point3F(313.4f, 1196.0f, -1.0f));
  vertices_b.push_back(gfx::Point3F(62.1f, 1196.0f, 1.0f));
  gfx::Vector3dF normal_b = gfx::CrossProduct(vertices_b[1] - vertices_b[0],
                                              vertices_b[1] - vertices_b[2]);
  normal_b.Scale(1.0f / normal_b.Length());

  CREATE_NEW_DRAW_POLYGON(polygon_a, vertices_a, normal_a, 0);
  CREATE_NEW_DRAW_POLYGON(polygon_b, vertices_b, normal_b, 1);

  EXPECT_EQ(BSP_FRONT, DrawPolygon::SideCompare(polygon_b, polygon_a));
}

// Two quads are definitely not touching and so no split should occur.
TEST(DrawPolygonSplitTest, NotTouchingNoSplit) {
  std::vector<gfx::Point3F> vertices_a;
  vertices_a.push_back(gfx::Point3F(0.0f, 10.0f, 0.0f));
  vertices_a.push_back(gfx::Point3F(0.0f, 0.0f, 0.0f));
  vertices_a.push_back(gfx::Point3F(10.0f, 0.0f, 0.0f));
  vertices_a.push_back(gfx::Point3F(10.0f, 10.0f, 0.0f));
  std::vector<gfx::Point3F> vertices_b;
  vertices_b.push_back(gfx::Point3F(5.0f, 10.0f, 5.0f));
  vertices_b.push_back(gfx::Point3F(5.0f, 0.0f, 15.0f));
  vertices_b.push_back(gfx::Point3F(5.0f, 0.0f, 15.0f));
  vertices_b.push_back(gfx::Point3F(5.0f, 10.0f, 5.0f));

  CREATE_NEW_DRAW_POLYGON(
      polygon_a, vertices_a, gfx::Vector3dF(0.0f, 0.0f, 1.0f), 0);
  CREATE_NEW_DRAW_POLYGON(
      polygon_b, vertices_b, gfx::Vector3dF(-1.0f, 0.0f, 0.0f), 1);

  EXPECT_EQ(BSP_FRONT, DrawPolygon::SideCompare(polygon_b, polygon_a));
}

// One quad is resting against another, but doesn't cross its plane so no split
// should occur.
TEST(DrawPolygonSplitTest, BarelyTouchingNoSplit) {
  std::vector<gfx::Point3F> vertices_a;
  vertices_a.push_back(gfx::Point3F(0.0f, 10.0f, 0.0f));
  vertices_a.push_back(gfx::Point3F(0.0f, 0.0f, 0.0f));
  vertices_a.push_back(gfx::Point3F(10.0f, 0.0f, 0.0f));
  vertices_a.push_back(gfx::Point3F(10.0f, 10.0f, 0.0f));
  std::vector<gfx::Point3F> vertices_b;
  vertices_b.push_back(gfx::Point3F(5.0f, 10.0f, 0.0f));
  vertices_b.push_back(gfx::Point3F(5.0f, 0.0f, -10.0f));
  vertices_b.push_back(gfx::Point3F(5.0f, 0.0f, -10.0f));
  vertices_b.push_back(gfx::Point3F(5.0f, 10.0f, 0.0f));

  CREATE_NEW_DRAW_POLYGON(
      polygon_a, vertices_a, gfx::Vector3dF(0.0f, 0.0f, 1.0f), 0);
  CREATE_NEW_DRAW_POLYGON(
      polygon_b, vertices_b, gfx::Vector3dF(-1.0f, 0.0f, 0.0f), 1);

  EXPECT_EQ(BSP_BACK, DrawPolygon::SideCompare(polygon_b, polygon_a));
}

// One quad intersects another and becomes two pieces.
TEST(DrawPolygonSplitTest, BasicSplit) {
  std::vector<gfx::Point3F> vertices_a;
  vertices_a.push_back(gfx::Point3F(0.0f, 10.0f, 0.0f));
  vertices_a.push_back(gfx::Point3F(0.0f, 0.0f, 0.0f));
  vertices_a.push_back(gfx::Point3F(10.0f, 0.0f, 0.0f));
  vertices_a.push_back(gfx::Point3F(10.0f, 10.0f, 0.0f));
  std::vector<gfx::Point3F> vertices_b;
  vertices_b.push_back(gfx::Point3F(5.0f, 10.0f, -5.0f));
  vertices_b.push_back(gfx::Point3F(5.0f, 0.0f, -5.0f));
  vertices_b.push_back(gfx::Point3F(5.0f, 0.0f, 5.0f));
  vertices_b.push_back(gfx::Point3F(5.0f, 10.0f, 5.0f));

  CREATE_NEW_DRAW_POLYGON(
      polygon_a, vertices_a, gfx::Vector3dF(0.0f, 0.0f, 1.0f), 0);
  CREATE_NEW_DRAW_POLYGON(
      polygon_b, vertices_b, gfx::Vector3dF(-1.0f, 0.0f, 0.0f), 1);

  EXPECT_EQ(BSP_SPLIT, DrawPolygon::SideCompare(polygon_b, polygon_a));

  scoped_ptr<DrawPolygon> front_polygon;
  scoped_ptr<DrawPolygon> back_polygon;
  polygon_b.Split(polygon_a, &front_polygon, &back_polygon);
  EXPECT_EQ(BSP_FRONT, DrawPolygon::SideCompare(*front_polygon, polygon_a));
  EXPECT_EQ(BSP_BACK, DrawPolygon::SideCompare(*back_polygon, polygon_a));

  std::vector<gfx::Point3F> test_points_a;
  test_points_a.push_back(gfx::Point3F(5.0f, 0.0f, 0.0f));
  test_points_a.push_back(gfx::Point3F(5.0f, 0.0f, 5.0f));
  test_points_a.push_back(gfx::Point3F(5.0f, 10.0f, 5.0f));
  test_points_a.push_back(gfx::Point3F(5.0f, 10.0f, 0.0f));
  std::vector<gfx::Point3F> test_points_b;
  test_points_b.push_back(gfx::Point3F(5.0f, 10.0f, 0.0f));
  test_points_b.push_back(gfx::Point3F(5.0f, 10.0f, -5.0f));
  test_points_b.push_back(gfx::Point3F(5.0f, 0.0f, -5.0f));
  test_points_b.push_back(gfx::Point3F(5.0f, 0.0f, 0.0f));
  ValidatePoints(*(front_polygon.get()), test_points_a);
  ValidatePoints(*(back_polygon.get()), test_points_b);

  EXPECT_EQ(4u, front_polygon->points().size());
  EXPECT_EQ(4u, back_polygon->points().size());
}

// In this test we cut the corner of a quad so that it creates a triangle and
// a pentagon as a result.
TEST(DrawPolygonSplitTest, AngledSplit) {
  std::vector<gfx::Point3F> vertices_a;
  vertices_a.push_back(gfx::Point3F(0.0f, 0.0f, 0.0f));
  vertices_a.push_back(gfx::Point3F(0.0f, 0.0f, 10.0f));
  vertices_a.push_back(gfx::Point3F(10.0f, 0.0f, 10.0f));
  vertices_a.push_back(gfx::Point3F(10.0f, 0.0f, 0.0f));
  std::vector<gfx::Point3F> vertices_b;
  vertices_b.push_back(gfx::Point3F(2.0f, 5.0f, 1.0f));
  vertices_b.push_back(gfx::Point3F(2.0f, -5.0f, 1.0f));
  vertices_b.push_back(gfx::Point3F(-1.0f, -5.0f, -2.0f));
  vertices_b.push_back(gfx::Point3F(-1.0f, 5.0f, -2.0f));

  CREATE_NEW_DRAW_POLYGON(
      polygon_a, vertices_a, gfx::Vector3dF(0.0f, 1.0f, 0.0f), 0);
  CREATE_NEW_DRAW_POLYGON(
      polygon_b, vertices_b, gfx::Vector3dF(0.707107f, 0.0f, -0.707107f), 1);

  EXPECT_EQ(BSP_SPLIT, DrawPolygon::SideCompare(polygon_a, polygon_b));

  scoped_ptr<DrawPolygon> front_polygon;
  scoped_ptr<DrawPolygon> back_polygon;
  polygon_a.Split(polygon_b, &front_polygon, &back_polygon);
  EXPECT_EQ(BSP_FRONT, DrawPolygon::SideCompare(*front_polygon, polygon_b));
  EXPECT_EQ(BSP_BACK, DrawPolygon::SideCompare(*back_polygon, polygon_b));

  EXPECT_EQ(3u, front_polygon->points().size());
  EXPECT_EQ(5u, back_polygon->points().size());

  std::vector<gfx::Point3F> test_points_a;
  test_points_a.push_back(gfx::Point3F(10.0f, 0.0f, 9.0f));
  test_points_a.push_back(gfx::Point3F(10.0f, 0.0f, 0.0f));
  test_points_a.push_back(gfx::Point3F(1.0f, 0.0f, 0.0f));
  std::vector<gfx::Point3F> test_points_b;
  test_points_b.push_back(gfx::Point3F(1.0f, 0.0f, 0.0f));
  test_points_b.push_back(gfx::Point3F(0.0f, 0.0f, 0.0f));
  test_points_b.push_back(gfx::Point3F(0.0f, 0.0f, 10.0f));
  test_points_b.push_back(gfx::Point3F(10.0f, 0.0f, 10.0f));
  test_points_b.push_back(gfx::Point3F(10.0f, 0.0f, 9.0f));

  ValidatePoints(*(front_polygon.get()), test_points_a);
  ValidatePoints(*(back_polygon.get()), test_points_b);
}

TEST(DrawPolygonTransformTest, TransformNormal) {
  std::vector<gfx::Point3F> vertices_a;
  vertices_a.push_back(gfx::Point3F(1.0f, 0.0f, 1.0f));
  vertices_a.push_back(gfx::Point3F(-1.0f, 0.0f, -1.0f));
  vertices_a.push_back(gfx::Point3F(0.0f, 1.0f, 0.0f));
  CREATE_NEW_DRAW_POLYGON(
      polygon_a, vertices_a, gfx::Vector3dF(0.707107f, 0.0f, -0.707107f), 0);
  // Check we believe your little white lie.
  EXPECT_NORMAL(polygon_a, 0.707107f, 0.0f, -0.707107f);

  polygon_a.RecomputeNormalForTesting();
  // Check that we recompute it more accurately.
  EXPECT_NORMAL(polygon_a, sqrt(2) / 2, 0.0f, -sqrt(2) / 2);

  gfx::Transform transform;
  transform.RotateAboutYAxis(45.0f);
  // This would transform the vertices as well, but we are transforming a
  // DrawPolygon with 0 vertices just to make sure our normal transformation
  // using the inverse tranpose matrix gives us the right result.
  polygon_a.TransformToScreenSpace(transform);

  // Note: We use EXPECT_FLOAT_WITHIN_EPSILON instead of EXPECT_FLOAT_EQUAL here
  // because some architectures (e.g., Arm64) employ a fused multiply-add
  // instruction which causes rounding asymmetry and reduces precision.
  // http://crbug.com/401117.
  EXPECT_NORMAL(polygon_a, 0.0f, 0.0f, -1.0f);
}

}  // namespace
}  // namespace cc
