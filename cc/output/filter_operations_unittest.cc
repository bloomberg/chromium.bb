// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/filter_operations.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/point.h"

namespace cc {
namespace {

TEST(FilterOperationsTest, GetOutsetsBlur) {
  FilterOperations ops;
  ops.Append(FilterOperation::CreateBlurFilter(20));
  int top, right, bottom, left;
  top = right = bottom = left = 0;
  ops.GetOutsets(&top, &right, &bottom, &left);
  EXPECT_EQ(57, top);
  EXPECT_EQ(57, right);
  EXPECT_EQ(57, bottom);
  EXPECT_EQ(57, left);
}

TEST(FilterOperationsTest, GetOutsetsDropShadow) {
  FilterOperations ops;
  ops.Append(FilterOperation::CreateDropShadowFilter(gfx::Point(3, 8), 20, 0));
  int top, right, bottom, left;
  top = right = bottom = left = 0;
  ops.GetOutsets(&top, &right, &bottom, &left);
  EXPECT_EQ(49, top);
  EXPECT_EQ(60, right);
  EXPECT_EQ(65, bottom);
  EXPECT_EQ(54, left);
}

#define SAVE_RESTORE_AMOUNT(filter_name, filter_type, a)                  \
  {                                                                       \
    FilterOperation op = FilterOperation::Create##filter_name##Filter(a); \
    EXPECT_EQ(FilterOperation::filter_type, op.type());                   \
    EXPECT_EQ(a, op.amount());                                            \
                                                                          \
    FilterOperation op2 = FilterOperation::CreateEmptyFilter();           \
    op2.set_type(FilterOperation::filter_type);                           \
                                                                          \
    EXPECT_NE(a, op2.amount());                                           \
                                                                          \
    op2.set_amount(a);                                                    \
                                                                          \
    EXPECT_EQ(FilterOperation::filter_type, op2.type());                  \
    EXPECT_EQ(a, op2.amount());                                           \
  }

#define SAVE_RESTORE_OFFSET_AMOUNT_COLOR(filter_name, filter_type, a, b, c) \
  {                                                                         \
    FilterOperation op =                                                    \
        FilterOperation::Create##filter_name##Filter(a, b, c);              \
    EXPECT_EQ(FilterOperation::filter_type, op.type());                     \
    EXPECT_EQ(a, op.drop_shadow_offset());                                  \
    EXPECT_EQ(b, op.amount());                                              \
    EXPECT_EQ(c, op.drop_shadow_color());                                   \
                                                                            \
    FilterOperation op2 = FilterOperation::CreateEmptyFilter();             \
    op2.set_type(FilterOperation::filter_type);                             \
                                                                            \
    EXPECT_NE(a, op2.drop_shadow_offset());                                 \
    EXPECT_NE(b, op2.amount());                                             \
    EXPECT_NE(c, op2.drop_shadow_color());                                  \
                                                                            \
    op2.set_drop_shadow_offset(a);                                          \
    op2.set_amount(b);                                                      \
    op2.set_drop_shadow_color(c);                                           \
                                                                            \
    EXPECT_EQ(FilterOperation::filter_type, op2.type());                    \
    EXPECT_EQ(a, op2.drop_shadow_offset());                                 \
    EXPECT_EQ(b, op2.amount());                                             \
    EXPECT_EQ(c, op2.drop_shadow_color());                                  \
  }

#define SAVE_RESTORE_MATRIX(filter_name, filter_type, a)                  \
  {                                                                       \
    FilterOperation op = FilterOperation::Create##filter_name##Filter(a); \
    EXPECT_EQ(FilterOperation::filter_type, op.type());                   \
    for (size_t i = 0; i < 20; ++i)                                       \
      EXPECT_EQ(a[i], op.matrix()[i]);                                    \
                                                                          \
    FilterOperation op2 = FilterOperation::CreateEmptyFilter();           \
    op2.set_type(FilterOperation::filter_type);                           \
                                                                          \
    for (size_t i = 0; i < 20; ++i)                                       \
      EXPECT_NE(a[i], op2.matrix()[i]);                                   \
                                                                          \
    op2.set_matrix(a);                                                    \
                                                                          \
    EXPECT_EQ(FilterOperation::filter_type, op2.type());                  \
    for (size_t i = 0; i < 20; ++i)                                       \
      EXPECT_EQ(a[i], op.matrix()[i]);                                    \
  }

#define SAVE_RESTORE_AMOUNT_INSET(filter_name, filter_type, a, b)            \
  {                                                                          \
    FilterOperation op = FilterOperation::Create##filter_name##Filter(a, b); \
    EXPECT_EQ(FilterOperation::filter_type, op.type());                      \
    EXPECT_EQ(a, op.amount());                                               \
    EXPECT_EQ(b, op.zoom_inset());                                           \
                                                                             \
    FilterOperation op2 = FilterOperation::CreateEmptyFilter();              \
    op2.set_type(FilterOperation::filter_type);                              \
                                                                             \
    EXPECT_NE(a, op2.amount());                                              \
    EXPECT_NE(b, op2.zoom_inset());                                          \
                                                                             \
    op2.set_amount(a);                                                       \
    op2.set_zoom_inset(b);                                                   \
                                                                             \
    EXPECT_EQ(FilterOperation::filter_type, op2.type());                     \
    EXPECT_EQ(a, op2.amount());                                              \
    EXPECT_EQ(b, op2.zoom_inset());                                          \
  }

TEST(FilterOperationsTest, SaveAndRestore) {
  SAVE_RESTORE_AMOUNT(Grayscale, GRAYSCALE, 0.6f);
  SAVE_RESTORE_AMOUNT(Sepia, SEPIA, 0.6f);
  SAVE_RESTORE_AMOUNT(Saturate, SATURATE, 0.6f);
  SAVE_RESTORE_AMOUNT(HueRotate, HUE_ROTATE, 0.6f);
  SAVE_RESTORE_AMOUNT(Invert, INVERT, 0.6f);
  SAVE_RESTORE_AMOUNT(Brightness, BRIGHTNESS, 0.6f);
  SAVE_RESTORE_AMOUNT(Contrast, CONTRAST, 0.6f);
  SAVE_RESTORE_AMOUNT(Opacity, OPACITY, 0.6f);
  SAVE_RESTORE_AMOUNT(Blur, BLUR, 0.6f);
  SAVE_RESTORE_AMOUNT(SaturatingBrightness, SATURATING_BRIGHTNESS, 0.6f);
  SAVE_RESTORE_OFFSET_AMOUNT_COLOR(
      DropShadow, DROP_SHADOW, gfx::Point(3, 4), 0.4f, 0xffffff00);

  SkScalar matrix[20] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                         17, 18, 19, 20};
  SAVE_RESTORE_MATRIX(ColorMatrix, COLOR_MATRIX, matrix);

  SAVE_RESTORE_AMOUNT_INSET(Zoom, ZOOM, 0.5f, 32);
}

}  // namespace
}  // namespace cc
