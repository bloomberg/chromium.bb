// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/common/common_similarity_traits.h"
#include "mojo/public/tests/simple_bindings_support.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

void ExpectEqualsStringPiece(const std::string& expected,
                             const base::StringPiece& str) {
  EXPECT_EQ(expected, str.as_string());
}

void ExpectEqualsMojoString(const std::string& expected,
                            const String& str) {
  EXPECT_EQ(expected, str.To<std::string>());
}

}  // namespace

class CommonSimilarityTraitsTest : public testing::Test {
 private:
  SimpleBindingsSupport bindings_support_;
};

TEST_F(CommonSimilarityTraitsTest, StringPiece) {
  AllocationScope scope;

  std::string kText("hello world");

  base::StringPiece string_piece(kText);
  String mojo_string(string_piece);

  ExpectEqualsMojoString(kText, mojo_string);
  ExpectEqualsStringPiece(kText, mojo_string.To<base::StringPiece>());

  // Test implicit construction and conversion:
  ExpectEqualsMojoString(kText, string_piece);
  ExpectEqualsStringPiece(kText, mojo_string);

  // Test null String:
  base::StringPiece empty_string_piece = String();
  EXPECT_TRUE(empty_string_piece.empty());
}

}  // namespace test
}  // namespace mojo
