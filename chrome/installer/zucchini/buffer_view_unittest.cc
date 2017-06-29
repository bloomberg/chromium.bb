// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/zucchini/buffer_view.h"

#include <iterator>
#include <type_traits>

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace zucchini {

class BufferViewTest : public testing::Test {
 protected:
  static constexpr size_t kLen = 10;

  // Some tests might modify this.
  uint8_t bytes_[kLen] = {0x10, 0x32, 0x54, 0x76, 0x98,
                          0xBA, 0xDC, 0xFE, 0x10, 0x00};
};

constexpr size_t BufferViewTest::kLen;

TEST_F(BufferViewTest, Size) {
  for (size_t len = 0; len <= kLen; ++len) {
    EXPECT_EQ(len, BufferView(std::begin(bytes_), len).size());
    EXPECT_EQ(len, MutableBufferView(std::begin(bytes_), len).size());
  }
}

TEST_F(BufferViewTest, Empty) {
  // Empty view.
  EXPECT_TRUE(BufferView(std::begin(bytes_), 0).empty());
  EXPECT_TRUE(MutableBufferView(std::begin(bytes_), 0).empty());

  for (size_t len = 1; len <= kLen; ++len) {
    EXPECT_FALSE(BufferView(std::begin(bytes_), len).empty());
    EXPECT_FALSE(MutableBufferView(std::begin(bytes_), len).empty());
  }
}

TEST_F(BufferViewTest, FromRange) {
  BufferView buffer =
      BufferView::FromRange(std::begin(bytes_), std::end(bytes_));
  EXPECT_EQ(kLen, buffer.size());
  EXPECT_EQ(std::begin(bytes_), buffer.begin());

  EXPECT_DCHECK_DEATH(
      BufferView::FromRange(std::end(bytes_), std::begin(bytes_)));

  EXPECT_DCHECK_DEATH(
      BufferView::FromRange(std::begin(bytes_) + 1, std::begin(bytes_)));
}

TEST_F(BufferViewTest, Subscript) {
  BufferView view(std::begin(bytes_), kLen);

  EXPECT_EQ(0x10, view[0]);
  static_assert(!std::is_assignable<decltype(view[0]), uint8_t>::value,
                "BufferView values should not be mutable.");

  MutableBufferView mutable_view(std::begin(bytes_), kLen);

  EXPECT_EQ(&bytes_[0], &mutable_view[0]);
  mutable_view[0] = 42;
  EXPECT_EQ(42, mutable_view[0]);
}

TEST_F(BufferViewTest, Shrink) {
  BufferView buffer =
      BufferView::FromRange(std::begin(bytes_), std::end(bytes_));

  buffer.shrink(kLen);
  EXPECT_EQ(kLen, buffer.size());
  buffer.shrink(2);
  EXPECT_EQ(size_t(2), buffer.size());
  EXPECT_DCHECK_DEATH(buffer.shrink(kLen));
}

}  // namespace zucchini
