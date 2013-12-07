// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/lib/bindings.h"
#include "mojo/public/tests/simple_bindings_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {

// Tests that basic Array operations work.
TEST(ArrayTest, Basic) {
  SimpleBindingsSupport bindings_support;

  // 8 bytes for the array, with 8 bytes left over for elements.
  internal::FixedBuffer buf(8 + 8*sizeof(char));
  EXPECT_EQ(16u, buf.size());

  Array<char>::Builder builder(8);
  EXPECT_EQ(8u, builder.size());
  for (size_t i = 0; i < builder.size(); ++i) {
    char val = static_cast<char>(i*2);
    builder[i] = val;
    EXPECT_EQ(val, builder.at(i));
  }
  Array<char> array = builder.Finish();
  for (size_t i = 0; i < array.size(); ++i) {
    char val = static_cast<char>(i) * 2;
    EXPECT_EQ(val, array[i]);
  }
}

// Tests that basic Array<bool> operations work, and that it's packed into 1
// bit per element.
TEST(ArrayTest, Bool) {
  SimpleBindingsSupport bindings_support;

  // 8 bytes for the array header, with 8 bytes left over for elements.
  internal::FixedBuffer buf(8 + 3);
  EXPECT_EQ(16u, buf.size());

  // An array of 64 bools can fit into 8 bytes.
  Array<bool>::Builder builder(64);
  EXPECT_EQ(64u, builder.size());
  for (size_t i = 0; i < builder.size(); ++i) {
    bool val = i % 3 == 0;
    builder[i] = val;
    EXPECT_EQ(val, builder.at(i));
  }
  Array<bool> array = builder.Finish();
  for (size_t i = 0; i < array.size(); ++i) {
    bool val = i % 3 == 0;
    EXPECT_EQ(val, array[i]);
  }
}

}  // namespace test
}  // namespace mojo
