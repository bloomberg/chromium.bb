// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/public/cpp/property_type_converters.h"

#include <stdint.h>

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/skia_util.h"

namespace mojo {
namespace {

// Use a different width and height so tests can tell the values apart.
const int kBitmapWidth = 16;
const int kBitmapHeight = 32;

// Makes a small rectangular bitmap.
SkBitmap MakeBitmap() {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(kBitmapWidth, kBitmapHeight);
  bitmap.eraseARGB(255, 11, 22, 33);
  return bitmap;
}

// Tests that an SkBitmap can be serialized.
TEST(PropertyTypeConvertersTest, SkBitmapSerialize) {
  SkBitmap bitmap = MakeBitmap();
  std::vector<uint8_t> bytes =
      TypeConverter<std::vector<uint8_t>, SkBitmap>::Convert(bitmap);

  // Size should be 4 bytes of header plus size of RGBA pixels.
  ASSERT_EQ(4 + bitmap.getSize(), bytes.size());

  // Header contains width first then height.
  EXPECT_EQ(0, bytes[0]);
  EXPECT_EQ(kBitmapWidth, bytes[1]);
  EXPECT_EQ(0, bytes[2]);
  EXPECT_EQ(kBitmapHeight, bytes[3]);

  // The rest of the bytes are the image data.
  EXPECT_EQ(0, memcmp(&bytes[4], bitmap.getPixels(), bitmap.getSize()));
}

// Tests that an SkBitmap can be deserialized.
TEST(PropertyTypeConvertersTest, SkBitmapDeserialize) {
  // Make a 1x2 pixel bitmap.
  std::vector<uint8_t> bytes = {0, 1, 0, 2, 11, 22, 33, 44, 55, 66, 77, 88};
  SkBitmap bitmap =
      TypeConverter<SkBitmap, std::vector<uint8_t>>::Convert(bytes);
  EXPECT_EQ(1, bitmap.width());
  EXPECT_EQ(2, bitmap.height());
  // The image pixels match the vector bytes.
  ASSERT_EQ(8U, bitmap.getSize());
  EXPECT_EQ(0, memcmp(bitmap.getPixels(), &bytes[4], 8));
}

// Tests round-trip serializing and deserializing an SkBitmap.
TEST(PropertyTypeConvertersTest, SkBitmapRoundTrip) {
  SkBitmap bitmap1 = MakeBitmap();
  std::vector<uint8_t> bytes =
      TypeConverter<std::vector<uint8_t>, SkBitmap>::Convert(bitmap1);
  SkBitmap bitmap2 =
      TypeConverter<SkBitmap, std::vector<uint8_t>>::Convert(bytes);
  EXPECT_TRUE(gfx::BitmapsAreEqual(bitmap1, bitmap2));
}

// Tests that an empty SkBitmap serializes to an empty vector.
TEST(PropertyTypeConvertersTest, SkBitmapSerializeEmpty) {
  SkBitmap bitmap;
  std::vector<uint8_t> bytes =
      TypeConverter<std::vector<uint8_t>, SkBitmap>::Convert(bitmap);
  EXPECT_TRUE(bytes.empty());
}

// Tests that an empty vector deserializes to a null SkBitmap.
TEST(PropertyTypeConvertersTest, SkBitmapDeserializeEmpty) {
  std::vector<uint8_t> bytes;
  SkBitmap bitmap =
      TypeConverter<SkBitmap, std::vector<uint8_t>>::Convert(bytes);
  EXPECT_TRUE(bitmap.isNull());
}

}  // namespace
}  // namespace mojo
