// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/fixed_buffer.h"
#include "mojo/public/cpp/environment/environment.h"
#include "mojo/public/interfaces/bindings/tests/test_structs.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

RectPtr MakeRect(int32_t factor = 1) {
  RectPtr rect(Rect::New());
  rect->x = 1 * factor;
  rect->y = 2 * factor;
  rect->width = 10 * factor;
  rect->height = 20 * factor;
  return rect.Pass();
}

void CheckRect(const Rect& rect, int32_t factor = 1) {
  EXPECT_EQ(1 * factor, rect.x);
  EXPECT_EQ(2 * factor, rect.y);
  EXPECT_EQ(10 * factor, rect.width);
  EXPECT_EQ(20 * factor, rect.height);
}

class StructTest : public testing::Test {
 public:
  virtual ~StructTest() {}

 private:
  Environment env_;
};

}  // namespace

TEST_F(StructTest, Rect) {
  RectPtr rect;
  EXPECT_TRUE(rect.is_null());
  EXPECT_TRUE(!rect);
  EXPECT_FALSE(rect);

  rect = MakeRect();
  EXPECT_FALSE(rect.is_null());
  EXPECT_FALSE(!rect);
  EXPECT_TRUE(rect);

  CheckRect(*rect);
}

// Serialization test of a struct with no pointer or handle members.
TEST_F(StructTest, Serialization_Basic) {
  RectPtr rect(MakeRect());

  size_t size = GetSerializedSize_(rect);
  EXPECT_EQ(8U + 16U, size);

  mojo::internal::FixedBuffer buf(size);
  internal::Rect_Data* data;
  Serialize_(rect.Pass(), &buf, &data);

  RectPtr rect2;
  Deserialize_(data, &rect2);

  CheckRect(*rect2);
}

// Serialization test of a struct with struct pointers.
TEST_F(StructTest, Serialization_StructPointers) {
  RectPairPtr pair(RectPair::New());
  pair->first = MakeRect();
  pair->second = MakeRect();

  size_t size = GetSerializedSize_(pair);
  EXPECT_EQ(8U + 16U + 2*(8U + 16U), size);

  mojo::internal::FixedBuffer buf(size);
  internal::RectPair_Data* data;
  Serialize_(pair.Pass(), &buf, &data);

  RectPairPtr pair2;
  Deserialize_(data, &pair2);

  CheckRect(*pair2->first);
  CheckRect(*pair2->second);
}

// Serialization test of a struct with an array member.
TEST_F(StructTest, Serialization_ArrayPointers) {
  NamedRegionPtr region(NamedRegion::New());
  region->name = "region";
  region->rects = Array<RectPtr>::New(4);
  for (size_t i = 0; i < region->rects.size(); ++i)
    region->rects[i] = MakeRect(static_cast<int32_t>(i) + 1);

  size_t size = GetSerializedSize_(region);
  EXPECT_EQ(8U +  // header
            8U +  // name pointer
            8U +  // rects pointer
            8U +  // name header
            8U +  // name payload (rounded up)
            8U +    // rects header
            4*8U +  // rects payload (four pointers)
            4*(8U +   // rect header
               16U),  // rect payload (four ints)
            size);

  mojo::internal::FixedBuffer buf(size);
  internal::NamedRegion_Data* data;
  Serialize_(region.Pass(), &buf, &data);

  NamedRegionPtr region2;
  Deserialize_(data, &region2);

  EXPECT_EQ(String("region"), region2->name);

  EXPECT_EQ(4U, region2->rects.size());
  for (size_t i = 0; i < region2->rects.size(); ++i)
    CheckRect(*region2->rects[i], static_cast<int32_t>(i) + 1);
}

// Serialization test of a struct with null array pointers.
TEST_F(StructTest, Serialization_NullArrayPointers) {
  NamedRegionPtr region(NamedRegion::New());
  EXPECT_TRUE(region->name.is_null());
  EXPECT_TRUE(region->rects.is_null());

  size_t size = GetSerializedSize_(region);
  EXPECT_EQ(8U +  // header
            8U +  // name pointer
            8U,   // rects pointer
            size);

  mojo::internal::FixedBuffer buf(size);
  internal::NamedRegion_Data* data;
  Serialize_(region.Pass(), &buf, &data);

  NamedRegionPtr region2;
  Deserialize_(data, &region2);

  EXPECT_TRUE(region2->name.is_null());
  EXPECT_TRUE(region2->rects.is_null());
}

}  // namespace test
}  // namespace mojo
