// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/buffer_view.h"

#include <stddef.h>
#include <stdint.h>

#include <iterator>
#include <type_traits>
#include <vector>

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

class BufferViewTest : public testing::Test {
 protected:
  enum : size_t { kLen = 10 };

  // Some tests might modify this.
  uint8_t bytes_[kLen] = {0x10, 0x32, 0x54, 0x76, 0x98,
                          0xBA, 0xDC, 0xFE, 0x10, 0x00};
};

TEST_F(BufferViewTest, Size) {
  for (size_t len = 0; len <= kLen; ++len) {
    EXPECT_EQ(len, ConstBufferView(std::begin(bytes_), len).size());
    EXPECT_EQ(len, MutableBufferView(std::begin(bytes_), len).size());
  }
}

TEST_F(BufferViewTest, Empty) {
  // Empty view.
  EXPECT_TRUE(ConstBufferView(std::begin(bytes_), 0).empty());
  EXPECT_TRUE(MutableBufferView(std::begin(bytes_), 0).empty());

  for (size_t len = 1; len <= kLen; ++len) {
    EXPECT_FALSE(ConstBufferView(std::begin(bytes_), len).empty());
    EXPECT_FALSE(MutableBufferView(std::begin(bytes_), len).empty());
  }
}

TEST_F(BufferViewTest, FromRange) {
  ConstBufferView buffer =
      ConstBufferView::FromRange(std::begin(bytes_), std::end(bytes_));
  EXPECT_EQ(kLen, buffer.size());
  EXPECT_EQ(std::begin(bytes_), buffer.begin());

  EXPECT_DCHECK_DEATH(
      ConstBufferView::FromRange(std::end(bytes_), std::begin(bytes_)));

  EXPECT_DCHECK_DEATH(
      ConstBufferView::FromRange(std::begin(bytes_) + 1, std::begin(bytes_)));
}

TEST_F(BufferViewTest, Subscript) {
  ConstBufferView view(std::begin(bytes_), kLen);

  EXPECT_EQ(0x10, view[0]);
  static_assert(!std::is_assignable<decltype(view[0]), uint8_t>::value,
                "BufferView values should not be mutable.");

  MutableBufferView mutable_view(std::begin(bytes_), kLen);

  EXPECT_EQ(&bytes_[0], &mutable_view[0]);
  mutable_view[0] = 42;
  EXPECT_EQ(42, mutable_view[0]);
}

TEST_F(BufferViewTest, SubRegion) {
  ConstBufferView view(std::begin(bytes_), kLen);

  ConstBufferView sub_view = view[{2, 4}];
  EXPECT_EQ(view.begin() + 2, sub_view.begin());
  EXPECT_EQ(size_t(4), sub_view.size());
}

TEST_F(BufferViewTest, Shrink) {
  ConstBufferView buffer =
      ConstBufferView::FromRange(std::begin(bytes_), std::end(bytes_));

  buffer.shrink(kLen);
  EXPECT_EQ(kLen, buffer.size());
  buffer.shrink(2);
  EXPECT_EQ(size_t(2), buffer.size());
  EXPECT_DCHECK_DEATH(buffer.shrink(kLen));
}

TEST_F(BufferViewTest, Read) {
  ConstBufferView buffer =
      ConstBufferView::FromRange(std::begin(bytes_), std::end(bytes_));

  EXPECT_EQ(0x10U, buffer.read<uint8_t>(0));
  EXPECT_EQ(0x54U, buffer.read<uint8_t>(2));

  EXPECT_EQ(0x3210U, buffer.read<uint16_t>(0));
  EXPECT_EQ(0x7654U, buffer.read<uint16_t>(2));

  EXPECT_EQ(0x76543210U, buffer.read<uint32_t>(0));
  EXPECT_EQ(0xBA987654U, buffer.read<uint32_t>(2));

  EXPECT_EQ(0xFEDCBA9876543210ULL, buffer.read<uint64_t>(0));

  EXPECT_EQ(0x00, buffer.read<uint8_t>(9));
  EXPECT_DEATH(buffer.read<uint8_t>(10), "");

  EXPECT_EQ(0x0010FEDCU, buffer.read<uint32_t>(6));
  EXPECT_DEATH(buffer.read<uint32_t>(7), "");
}

TEST_F(BufferViewTest, Write) {
  MutableBufferView buffer =
      MutableBufferView::FromRange(std::begin(bytes_), std::end(bytes_));

  buffer.write<uint32_t>(0, 0x01234567);
  buffer.write<uint32_t>(4, 0x89ABCDEF);
  EXPECT_EQ(std::vector<uint8_t>(
                {0x67, 0x45, 0x23, 0x01, 0xEF, 0xCD, 0xAB, 0x89, 0x10, 0x00}),
            std::vector<uint8_t>(buffer.begin(), buffer.end()));

  buffer.write<uint8_t>(9, 0xFF);
  EXPECT_DEATH(buffer.write<uint8_t>(10, 0xFF), "");

  buffer.write<uint32_t>(6, 0xFFFFFFFF);
  EXPECT_DEATH(buffer.write<uint32_t>(7, 0xFFFFFFFF), "");
}

TEST_F(BufferViewTest, CanAccess) {
  MutableBufferView buffer =
      MutableBufferView::FromRange(std::begin(bytes_), std::end(bytes_));
  EXPECT_TRUE(buffer.can_access<uint32_t>(0));
  EXPECT_TRUE(buffer.can_access<uint32_t>(6));
  EXPECT_FALSE(buffer.can_access<uint32_t>(7));
  EXPECT_FALSE(buffer.can_access<uint32_t>(10));
  EXPECT_FALSE(buffer.can_access<uint32_t>(0xFFFFFFFFU));

  EXPECT_TRUE(buffer.can_access<uint8_t>(0));
  EXPECT_TRUE(buffer.can_access<uint8_t>(7));
  EXPECT_TRUE(buffer.can_access<uint8_t>(9));
  EXPECT_FALSE(buffer.can_access<uint8_t>(10));
  EXPECT_FALSE(buffer.can_access<uint8_t>(0xFFFFFFFF));
}

TEST_F(BufferViewTest, LocalRegion) {
  ConstBufferView view(std::begin(bytes_), kLen);

  BufferRegion region = view.local_region();
  EXPECT_EQ(0U, region.offset);
  EXPECT_EQ(kLen, region.size);
}

TEST_F(BufferViewTest, Covers) {
  EXPECT_FALSE(ConstBufferView().covers({0, 0}));
  EXPECT_FALSE(ConstBufferView().covers({0, 1}));

  ConstBufferView view(std::begin(bytes_), kLen);

  EXPECT_TRUE(view.covers({0, 0}));
  EXPECT_TRUE(view.covers({0, 1}));
  EXPECT_TRUE(view.covers({0, kLen}));
  EXPECT_FALSE(view.covers({0, kLen + 1}));
  EXPECT_FALSE(view.covers({1, kLen}));

  EXPECT_TRUE(view.covers({kLen - 1, 0}));
  EXPECT_TRUE(view.covers({kLen - 1, 1}));
  EXPECT_FALSE(view.covers({kLen - 1, 2}));
  EXPECT_FALSE(view.covers({kLen, 0}));
  EXPECT_FALSE(view.covers({kLen, 1}));

  EXPECT_FALSE(view.covers({1, size_t(-1)}));
  EXPECT_FALSE(view.covers({size_t(-1), 1}));
  EXPECT_FALSE(view.covers({size_t(-1), size_t(-1)}));
}

TEST_F(BufferViewTest, Equals) {
  // Almost identical to |bytes_|, except at [5] and [6].
  uint8_t bytes2[kLen] = {0x10, 0x32, 0x54, 0x76, 0x98,
                          0xAB, 0xCD, 0xFE, 0x10, 0x00};
  ConstBufferView view1(std::begin(bytes_), kLen);
  ConstBufferView view2(std::begin(bytes2), kLen);

  EXPECT_TRUE(view1.equals(view1));
  EXPECT_TRUE(view2.equals(view2));
  EXPECT_FALSE(view1.equals(view2));
  EXPECT_FALSE(view2.equals(view1));

  EXPECT_TRUE((view1[{0, 0}]).equals(view2[{0, 0}]));
  EXPECT_TRUE((view1[{0, 0}]).equals(view2[{5, 0}]));
  EXPECT_TRUE((view1[{0, 5}]).equals(view2[{0, 5}]));
  EXPECT_FALSE((view1[{0, 6}]).equals(view2[{0, 6}]));
  EXPECT_FALSE((view1[{0, 7}]).equals(view1[{0, 6}]));
  EXPECT_TRUE((view1[{5, 3}]).equals(view1[{5, 3}]));
  EXPECT_FALSE((view1[{5, 1}]).equals(view1[{5, 3}]));
  EXPECT_TRUE((view2[{0, 1}]).equals(view2[{8, 1}]));
  EXPECT_FALSE((view2[{1, 1}]).equals(view2[{8, 1}]));
}

}  // namespace zucchini
