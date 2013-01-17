// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_vector.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/transform_operations.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebTransformationMatrix.h"

using WebKit::WebTransformationMatrix;

namespace cc {
namespace {

TEST(TransformOperationTest, TransformTypesAreUnique) {
  ScopedVector<TransformOperations> transforms;

  TransformOperations* to_add = new TransformOperations();
  to_add->AppendTranslate(1, 0, 0);
  transforms.push_back(to_add);

  to_add = new TransformOperations();
  to_add->AppendRotate(0, 0, 1, 2);
  transforms.push_back(to_add);

  to_add = new TransformOperations();
  to_add->AppendScale(2, 2, 2);
  transforms.push_back(to_add);

  to_add = new TransformOperations();
  to_add->AppendSkew(1, 0);
  transforms.push_back(to_add);

  to_add = new TransformOperations();
  to_add->AppendPerspective(800);
  transforms.push_back(to_add);

  for (size_t i = 0; i < transforms.size(); ++i) {
    for (size_t j = 0; j < transforms.size(); ++j) {
      bool matches_type = transforms[i]->MatchesTypes(*transforms[j]);
      EXPECT_TRUE((i == j && matches_type) || !matches_type);
    }
  }
}

TEST(TransformOperationTest, MatchTypesSameLength) {
  TransformOperations translates;
  translates.AppendTranslate(1, 0, 0);
  translates.AppendTranslate(1, 0, 0);
  translates.AppendTranslate(1, 0, 0);

  TransformOperations skews;
  skews.AppendSkew(0, 2);
  skews.AppendSkew(0, 2);
  skews.AppendSkew(0, 2);

  TransformOperations translates2;
  translates2.AppendTranslate(0, 2, 0);
  translates2.AppendTranslate(0, 2, 0);
  translates2.AppendTranslate(0, 2, 0);

  TransformOperations translates3 = translates2;

  EXPECT_FALSE(translates.MatchesTypes(skews));
  EXPECT_TRUE(translates.MatchesTypes(translates2));
  EXPECT_TRUE(translates.MatchesTypes(translates3));
}

TEST(TransformOperationTest, MatchTypesDifferentLength) {
  TransformOperations translates;
  translates.AppendTranslate(1, 0, 0);
  translates.AppendTranslate(1, 0, 0);
  translates.AppendTranslate(1, 0, 0);

  TransformOperations skews;
  skews.AppendSkew(2, 0);
  skews.AppendSkew(2, 0);

  TransformOperations translates2;
  translates2.AppendTranslate(0, 2, 0);
  translates2.AppendTranslate(0, 2, 0);

  EXPECT_FALSE(translates.MatchesTypes(skews));
  EXPECT_FALSE(translates.MatchesTypes(translates2));
}

void GetIdentityOperations(ScopedVector<TransformOperations>* operations) {
  TransformOperations* to_add = new TransformOperations();
  operations->push_back(to_add);

  to_add = new TransformOperations();
  to_add->AppendTranslate(0, 0, 0);
  operations->push_back(to_add);

  to_add = new TransformOperations();
  to_add->AppendTranslate(0, 0, 0);
  to_add->AppendTranslate(0, 0, 0);
  operations->push_back(to_add);

  to_add = new TransformOperations();
  to_add->AppendScale(1, 1, 1);
  operations->push_back(to_add);

  to_add = new TransformOperations();
  to_add->AppendScale(1, 1, 1);
  to_add->AppendScale(1, 1, 1);
  operations->push_back(to_add);

  to_add = new TransformOperations();
  to_add->AppendSkew(0, 0);
  operations->push_back(to_add);

  to_add = new TransformOperations();
  to_add->AppendSkew(0, 0);
  to_add->AppendSkew(0, 0);
  operations->push_back(to_add);

  to_add = new TransformOperations();
  to_add->AppendRotate(0, 0, 1, 0);
  operations->push_back(to_add);

  to_add = new TransformOperations();
  to_add->AppendRotate(0, 0, 1, 0);
  to_add->AppendRotate(0, 0, 1, 0);
  operations->push_back(to_add);

  to_add = new TransformOperations();
  to_add->AppendMatrix(WebTransformationMatrix());
  operations->push_back(to_add);

  to_add = new TransformOperations();
  to_add->AppendMatrix(WebTransformationMatrix());
  to_add->AppendMatrix(WebTransformationMatrix());
  operations->push_back(to_add);
}

TEST(TransformOperationTest, IdentityAlwaysMatches) {
  ScopedVector<TransformOperations> operations;
  GetIdentityOperations(&operations);

  for (size_t i = 0; i < operations.size(); ++i) {
    for (size_t j = 0; j < operations.size(); ++j)
      EXPECT_TRUE(operations[i]->MatchesTypes(*operations[j]));
  }
}

TEST(TransformOperationTest, ApplyTranslate) {
  double x = 1;
  double y = 2;
  double z = 3;
  TransformOperations operations;
  operations.AppendTranslate(x, y, z);
  WebTransformationMatrix expected;
  expected.translate3d(x, y, z);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, operations.Apply());
}

TEST(TransformOperationTest, ApplyRotate) {
  double x = 1;
  double y = 2;
  double z = 3;
  double degrees = 80;
  TransformOperations operations;
  operations.AppendRotate(x, y, z, degrees);
  WebTransformationMatrix expected;
  expected.rotate3d(x, y, z, degrees);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, operations.Apply());
}

TEST(TransformOperationTest, ApplyScale) {
  double x = 1;
  double y = 2;
  double z = 3;
  TransformOperations operations;
  operations.AppendScale(x, y, z);
  WebTransformationMatrix expected;
  expected.scale3d(x, y, z);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, operations.Apply());
}

TEST(TransformOperationTest, ApplySkew) {
  double x = 1;
  double y = 2;
  TransformOperations operations;
  operations.AppendSkew(x, y);
  WebTransformationMatrix expected;
  expected.skewX(x);
  expected.skewY(y);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, operations.Apply());
}

TEST(TransformOperationTest, ApplyPerspective) {
  double depth = 800;
  TransformOperations operations;
  operations.AppendPerspective(depth);
  WebTransformationMatrix expected;
  expected.applyPerspective(depth);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected, operations.Apply());
}

TEST(TransformOperationTest, ApplyMatrix) {
  double dx = 1;
  double dy = 2;
  double dz = 3;
  WebTransformationMatrix expected_matrix;
  expected_matrix.translate3d(dx, dy, dz);
  TransformOperations matrix_transform;
  matrix_transform.AppendMatrix(expected_matrix);
  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_matrix, matrix_transform.Apply());
}

TEST(TransformOperationTest, ApplyOrder) {
  double sx = 2;
  double sy = 4;
  double sz = 8;

  double dx = 1;
  double dy = 2;
  double dz = 3;

  TransformOperations operations;
  operations.AppendScale(sx, sy, sz);
  operations.AppendTranslate(dx, dy, dz);

  WebTransformationMatrix expected_scale_matrix;
  expected_scale_matrix.scale3d(sx, sy, sz);

  WebTransformationMatrix expected_translate_matrix;
  expected_translate_matrix.translate3d(dx, dy, dz);

  WebTransformationMatrix expected_combined_matrix = expected_scale_matrix;
  expected_combined_matrix.multiply(expected_translate_matrix);

  EXPECT_TRANSFORMATION_MATRIX_EQ(expected_combined_matrix, operations.Apply());
}

TEST(TransformOperationTest, BlendOrder) {
  double sx1 = 2;
  double sy1 = 4;
  double sz1 = 8;

  double dx1 = 1;
  double dy1 = 2;
  double dz1 = 3;

  double sx2 = 4;
  double sy2 = 8;
  double sz2 = 16;

  double dx2 = 10;
  double dy2 = 20;
  double dz2 = 30;

  TransformOperations operations_from;
  operations_from.AppendScale(sx1, sy1, sz1);
  operations_from.AppendTranslate(dx1, dy1, dz1);

  TransformOperations operations_to;
  operations_to.AppendScale(sx2, sy2, sz2);
  operations_to.AppendTranslate(dx2, dy2, dz2);

  WebTransformationMatrix scale_from;
  scale_from.scale3d(sx1, sy1, sz1);
  WebTransformationMatrix translate_from;
  translate_from.translate3d(dx1, dy1, dz1);

  WebTransformationMatrix scale_to;
  scale_to.scale3d(sx2, sy2, sz2);
  WebTransformationMatrix translate_to;
  translate_to.translate3d(dx2, dy2, dz2);

  double progress = 0.25;

  WebTransformationMatrix blended_scale = scale_to;
  blended_scale.blend(scale_from, progress);

  WebTransformationMatrix blended_translate = translate_to;
  blended_translate.blend(translate_from, progress);

  WebTransformationMatrix expected = blended_scale;
  expected.multiply(blended_translate);

  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected, operations_to.Blend(operations_from, progress));
}

static void CheckProgress(double progress,
              const WebTransformationMatrix& from_matrix,
              const WebTransformationMatrix& to_matrix,
              const TransformOperations& from_transform,
              const TransformOperations& to_transform) {
  WebTransformationMatrix expected_matrix = to_matrix;
  expected_matrix.blend(from_matrix, progress);
  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected_matrix, to_transform.Blend(from_transform, progress));
}

TEST(TransformOperationTest, BlendProgress) {
  double sx = 2;
  double sy = 4;
  double sz = 8;
  TransformOperations operations_from;
  operations_from.AppendScale(sx, sy, sz);

  WebTransformationMatrix matrix_from;
  matrix_from.scale3d(sx, sy, sz);

  sx = 4;
  sy = 8;
  sz = 16;
  TransformOperations operations_to;
  operations_to.AppendScale(sx, sy, sz);

  WebTransformationMatrix matrix_to;
  matrix_to.scale3d(sx, sy, sz);

  CheckProgress(-1, matrix_from, matrix_to, operations_from, operations_to);
  CheckProgress(0, matrix_from, matrix_to, operations_from, operations_to);
  CheckProgress(0.25, matrix_from, matrix_to, operations_from, operations_to);
  CheckProgress(0.5, matrix_from, matrix_to, operations_from, operations_to);
  CheckProgress(1, matrix_from, matrix_to, operations_from, operations_to);
  CheckProgress(2, matrix_from, matrix_to, operations_from, operations_to);
}

TEST(TransformOperationTest, BlendWhenTypesDoNotMatch) {
  double sx1 = 2;
  double sy1 = 4;
  double sz1 = 8;

  double dx1 = 1;
  double dy1 = 2;
  double dz1 = 3;

  double sx2 = 4;
  double sy2 = 8;
  double sz2 = 16;

  double dx2 = 10;
  double dy2 = 20;
  double dz2 = 30;

  TransformOperations operations_from;
  operations_from.AppendScale(sx1, sy1, sz1);
  operations_from.AppendTranslate(dx1, dy1, dz1);

  TransformOperations operations_to;
  operations_to.AppendTranslate(dx2, dy2, dz2);
  operations_to.AppendScale(sx2, sy2, sz2);

  WebTransformationMatrix from;
  from.scale3d(sx1, sy1, sz1);
  from.translate3d(dx1, dy1, dz1);

  WebTransformationMatrix to;
  to.translate3d(dx2, dy2, dz2);
  to.scale3d(sx2, sy2, sz2);

  double progress = 0.25;

  WebTransformationMatrix expected = to;
  expected.blend(from, progress);

  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected, operations_to.Blend(operations_from, progress));
}

TEST(TransformOperationTest, LargeRotationsWithSameAxis) {
  TransformOperations operations_from;
  operations_from.AppendRotate(0, 0, 1, 0);

  TransformOperations operations_to;
  operations_to.AppendRotate(0, 0, 2, 360);

  double progress = 0.5;

  WebTransformationMatrix expected;
  expected.rotate3d(0, 0, 1, 180);

  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected, operations_to.Blend(operations_from, progress));
}

TEST(TransformOperationTest, LargeRotationsWithSameAxisInDifferentDirection) {
  TransformOperations operations_from;
  operations_from.AppendRotate(0, 0, 1, 180);

  TransformOperations operations_to;
  operations_to.AppendRotate(0, 0, -1, 180);

  double progress = 0.5;

  WebTransformationMatrix expected;

  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected, operations_to.Blend(operations_from, progress));
}

TEST(TransformOperationTest, LargeRotationsWithDifferentAxes) {
  TransformOperations operations_from;
  operations_from.AppendRotate(0, 0, 1, 180);

  TransformOperations operations_to;
  operations_to.AppendRotate(0, 1, 0, 180);

  double progress = 0.5;
  WebTransformationMatrix matrix_from;
  matrix_from.rotate3d(0, 0, 1, 180);

  WebTransformationMatrix matrix_to;
  matrix_to.rotate3d(0, 1, 0, 180);

  WebTransformationMatrix expected = matrix_to;
  expected.blend(matrix_from, progress);

  EXPECT_TRANSFORMATION_MATRIX_EQ(
      expected, operations_to.Blend(operations_from, progress));
}

TEST(TransformOperationTest, BlendRotationFromIdentity) {
  ScopedVector<TransformOperations> identity_operations;
  GetIdentityOperations(&identity_operations);

  for (size_t i = 0; i < identity_operations.size(); ++i) {
    TransformOperations operations;
    operations.AppendRotate(0, 0, 1, 360);

    double progress = 0.5;

    WebTransformationMatrix expected;
    expected.rotate3d(0, 0, 1, 180);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        expected, operations.Blend(*identity_operations[i], progress));
  }
}

TEST(TransformOperationTest, BlendTranslationFromIdentity) {
  ScopedVector<TransformOperations> identity_operations;
  GetIdentityOperations(&identity_operations);

  for (size_t i = 0; i < identity_operations.size(); ++i) {
    TransformOperations operations;
    operations.AppendTranslate(2, 2, 2);

    double progress = 0.5;

    WebTransformationMatrix expected;
    expected.translate3d(1, 1, 1);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        expected, operations.Blend(*identity_operations[i], progress));
  }
}

TEST(TransformOperationTest, BlendScaleFromIdentity) {
  ScopedVector<TransformOperations> identity_operations;
  GetIdentityOperations(&identity_operations);

  for (size_t i = 0; i < identity_operations.size(); ++i) {
    TransformOperations operations;
    operations.AppendScale(3, 3, 3);

    double progress = 0.5;

    WebTransformationMatrix expected;
    expected.scale3d(2, 2, 2);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        expected, operations.Blend(*identity_operations[i], progress));
  }
}

TEST(TransformOperationTest, BlendSkewFromIdentity) {
  ScopedVector<TransformOperations> identity_operations;
  GetIdentityOperations(&identity_operations);

  for (size_t i = 0; i < identity_operations.size(); ++i) {
    TransformOperations operations;
    operations.AppendSkew(2, 2);

    double progress = 0.5;

    WebTransformationMatrix expected;
    expected.skewX(1);
    expected.skewY(1);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        expected, operations.Blend(*identity_operations[i], progress));
  }
}

TEST(TransformOperationTest, BlendPerspectiveFromIdentity) {
  ScopedVector<TransformOperations> identity_operations;
  GetIdentityOperations(&identity_operations);

  for (size_t i = 0; i < identity_operations.size(); ++i) {
    TransformOperations operations;
    operations.AppendPerspective(1000);

    double progress = 0.5;

    WebTransformationMatrix expected;
    expected.applyPerspective(500 + 0.5 * std::numeric_limits<double>::max());

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        expected, operations.Blend(*identity_operations[i], progress));
  }
}

TEST(TransformOperationTest, BlendRotationToIdentity) {
  ScopedVector<TransformOperations> identity_operations;
  GetIdentityOperations(&identity_operations);

  for (size_t i = 0; i < identity_operations.size(); ++i) {
    TransformOperations operations;
    operations.AppendRotate(0, 0, 1, 360);

    double progress = 0.5;

    WebTransformationMatrix expected;
    expected.rotate3d(0, 0, 1, 180);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        expected, identity_operations[i]->Blend(operations, progress));
  }
}

TEST(TransformOperationTest, BlendTranslationToIdentity) {
  ScopedVector<TransformOperations> identity_operations;
  GetIdentityOperations(&identity_operations);

  for (size_t i = 0; i < identity_operations.size(); ++i) {
    TransformOperations operations;
    operations.AppendTranslate(2, 2, 2);

    double progress = 0.5;

    WebTransformationMatrix expected;
    expected.translate3d(1, 1, 1);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        expected, identity_operations[i]->Blend(operations, progress));
  }
}

TEST(TransformOperationTest, BlendScaleToIdentity) {
  ScopedVector<TransformOperations> identity_operations;
  GetIdentityOperations(&identity_operations);

  for (size_t i = 0; i < identity_operations.size(); ++i) {
    TransformOperations operations;
    operations.AppendScale(3, 3, 3);

    double progress = 0.5;

    WebTransformationMatrix expected;
    expected.scale3d(2, 2, 2);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        expected, identity_operations[i]->Blend(operations, progress));
  }
}

TEST(TransformOperationTest, BlendSkewToIdentity) {
  ScopedVector<TransformOperations> identity_operations;
  GetIdentityOperations(&identity_operations);

  for (size_t i = 0; i < identity_operations.size(); ++i) {
    TransformOperations operations;
    operations.AppendSkew(2, 2);

    double progress = 0.5;

    WebTransformationMatrix expected;
    expected.skewX(1);
    expected.skewY(1);

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        expected, identity_operations[i]->Blend(operations, progress));
  }
}

TEST(TransformOperationTest, BlendPerspectiveToIdentity) {
  ScopedVector<TransformOperations> identity_operations;
  GetIdentityOperations(&identity_operations);

  for (size_t i = 0; i < identity_operations.size(); ++i) {
    TransformOperations operations;
    operations.AppendPerspective(1000);

    double progress = 0.5;

    WebTransformationMatrix expected;
    expected.applyPerspective(500 + 0.5 * std::numeric_limits<double>::max());

    EXPECT_TRANSFORMATION_MATRIX_EQ(
        expected, identity_operations[i]->Blend(operations, progress));
  }
}

}  // namespace
}  // namespace cc
