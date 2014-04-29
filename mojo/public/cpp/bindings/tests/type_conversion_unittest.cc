// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/allocation_scope.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/interfaces/bindings/tests/test_structs.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace {

struct RedmondRect {
  int32_t left;
  int32_t top;
  int32_t right;
  int32_t bottom;
};

struct RedmondNamedRegion {
  std::string name;
  std::vector<RedmondRect> rects;
};

bool AreEqualRectArrays(const Array<test_structs::Rect>& rects1,
                        const Array<test_structs::Rect>& rects2) {
  if (rects1.size() != rects2.size())
    return false;

  for (size_t i = 0; i < rects1.size(); ++i) {
    if (rects1[i].x() != rects2[i].x() ||
        rects1[i].y() != rects2[i].y() ||
        rects1[i].width() != rects2[i].width() ||
        rects1[i].height() != rects2[i].height()) {
      return false;
    }
  }

  return true;
}

}  // namespace

template <>
class TypeConverter<test_structs::Rect, RedmondRect> {
 public:
  static test_structs::Rect ConvertFrom(const RedmondRect& input, Buffer* buf) {
    test_structs::Rect::Builder rect(buf);
    rect.set_x(input.left);
    rect.set_y(input.top);
    rect.set_width(input.right - input.left);
    rect.set_height(input.bottom - input.top);
    return rect.Finish();
  }
  static RedmondRect ConvertTo(const test_structs::Rect& input) {
    RedmondRect rect;
    rect.left = input.x();
    rect.top = input.y();
    rect.right = input.x() + input.width();
    rect.bottom = input.y() + input.height();
    return rect;
  }

  MOJO_ALLOW_IMPLICIT_TYPE_CONVERSION();
};

template <>
class TypeConverter<Array<test_structs::Rect>, std::vector<RedmondRect> >
    : public GenericArrayTypeConverter<test_structs::Rect, RedmondRect> {
 public:
  MOJO_ALLOW_IMPLICIT_TYPE_CONVERSION();
};

template <>
class TypeConverter<test_structs::NamedRegion, RedmondNamedRegion> {
 public:
  static test_structs::NamedRegion ConvertFrom(const RedmondNamedRegion& input,
                                               Buffer* buf) {
    test_structs::NamedRegion::Builder region(buf);
    region.set_name(String(input.name, buf));
    region.set_rects(mojo::Array<test_structs::Rect>(input.rects, buf));
    return region.Finish();
  }
  static RedmondNamedRegion ConvertTo(const test_structs::NamedRegion& input) {
    RedmondNamedRegion region;
    region.name = input.name().To<std::string>();
    region.rects = input.rects().To<std::vector<RedmondRect> >();
    return region;
  }

  MOJO_ALLOW_IMPLICIT_TYPE_CONVERSION();
};

namespace test {
namespace {

class TypeConversionTest : public testing::Test {
 private:
  Environment env_;
};

TEST_F(TypeConversionTest, String) {
  AllocationScope scope;

  const char kText[6] = "hello";

  String a = std::string(kText);
  String b(kText);
  String c(static_cast<const char*>(kText));

  EXPECT_EQ(std::string(kText), a.To<std::string>());
  EXPECT_EQ(std::string(kText), b.To<std::string>());
  EXPECT_EQ(std::string(kText), c.To<std::string>());
}

TEST_F(TypeConversionTest, String_Null) {
  String a;
  EXPECT_TRUE(a.is_null());
  EXPECT_EQ(std::string(), a.To<std::string>());

  String b(static_cast<const char*>(NULL));
  EXPECT_TRUE(b.is_null());
}

TEST_F(TypeConversionTest, String_Empty) {
  AllocationScope scope;
  String a = String::Builder(0).Finish();
  EXPECT_EQ(std::string(), a.To<std::string>());

  String b = std::string();
  EXPECT_FALSE(b.is_null());
  EXPECT_EQ(std::string(), b.To<std::string>());
}

TEST_F(TypeConversionTest, String_ShallowCopy) {
  AllocationScope scope;

  String a("hello");
  String b(a);

  EXPECT_EQ(&a[0], &b[0]);
  EXPECT_EQ(a.To<std::string>(), b.To<std::string>());
}

TEST_F(TypeConversionTest, StringWithEmbeddedNull) {
  AllocationScope scope;

  const std::string kText("hel\0lo", 6);

  String a(kText);
  EXPECT_EQ(kText, a.To<std::string>());

  // Expect truncation:
  String b(kText.c_str());
  EXPECT_EQ(std::string("hel"), b.To<std::string>());
}

TEST_F(TypeConversionTest, CustomTypeConverter) {
  AllocationScope scope;

  test_structs::Rect::Builder rect_builder;
  rect_builder.set_x(10);
  rect_builder.set_y(20);
  rect_builder.set_width(50);
  rect_builder.set_height(45);
  test_structs::Rect rect = rect_builder.Finish();

  RedmondRect rr = rect.To<RedmondRect>();
  EXPECT_EQ(10, rr.left);
  EXPECT_EQ(20, rr.top);
  EXPECT_EQ(60, rr.right);
  EXPECT_EQ(65, rr.bottom);

  test_structs::Rect rect2(rr);
  EXPECT_EQ(rect.x(), rect2.x());
  EXPECT_EQ(rect.y(), rect2.y());
  EXPECT_EQ(rect.width(), rect2.width());
  EXPECT_EQ(rect.height(), rect2.height());
}

TEST_F(TypeConversionTest, CustomTypeConverter_Array_Null) {
  Array<test_structs::Rect> rects;

  std::vector<RedmondRect> redmond_rects =
      rects.To<std::vector<RedmondRect> >();

  EXPECT_TRUE(redmond_rects.empty());
}

TEST_F(TypeConversionTest, CustomTypeConverter_Array) {
  AllocationScope scope;

  const RedmondRect kBase = { 10, 20, 30, 40 };

  Array<test_structs::Rect>::Builder rects_builder(10);
  for (size_t i = 0; i < rects_builder.size(); ++i) {
    RedmondRect rr = kBase;
    rr.left += static_cast<int32_t>(i);
    rr.top += static_cast<int32_t>(i);
    rects_builder[i] = test_structs::Rect(rr);
  }
  Array<test_structs::Rect> rects = rects_builder.Finish();

  std::vector<RedmondRect> redmond_rects =
      rects.To<std::vector<RedmondRect> >();

  // Note: assignment broken in two lines tests default constructor with
  // assignment operator. We will also test conversion constructor.
  Array<test_structs::Rect> rects2;
  rects2 = redmond_rects;
  EXPECT_TRUE(AreEqualRectArrays(rects, rects2));

  // Test conversion constructor.
  Array<test_structs::Rect> rects3(redmond_rects);
  EXPECT_TRUE(AreEqualRectArrays(rects, rects3));

  // Test explicit conversion using From().
  Array<test_structs::Rect> rects4
      = Array<test_structs::Rect>::From(redmond_rects);
  EXPECT_TRUE(AreEqualRectArrays(rects, rects4));
}

TEST_F(TypeConversionTest, CustomTypeConverter_Nested) {
  AllocationScope scope;

  RedmondNamedRegion redmond_region;
  redmond_region.name = "foopy";

  const RedmondRect kBase = { 10, 20, 30, 40 };

  for (size_t i = 0; i < 10; ++i) {
    RedmondRect rect = kBase;
    rect.left += static_cast<int32_t>(i);
    rect.top += static_cast<int32_t>(i);
    redmond_region.rects.push_back(rect);
  }

  // Round-trip through generated struct and TypeConverter.

  test_structs::NamedRegion copy = redmond_region;
  RedmondNamedRegion redmond_region2 = copy.To<RedmondNamedRegion>();

  EXPECT_EQ(redmond_region.name, redmond_region2.name);
  EXPECT_EQ(redmond_region.rects.size(), redmond_region2.rects.size());
  for (size_t i = 0; i < redmond_region.rects.size(); ++i) {
    EXPECT_EQ(redmond_region.rects[i].left, redmond_region2.rects[i].left);
    EXPECT_EQ(redmond_region.rects[i].top, redmond_region2.rects[i].top);
    EXPECT_EQ(redmond_region.rects[i].right, redmond_region2.rects[i].right);
    EXPECT_EQ(redmond_region.rects[i].bottom, redmond_region2.rects[i].bottom);
  }
}

}  // namespace
}  // namespace test
}  // namespace mojo
