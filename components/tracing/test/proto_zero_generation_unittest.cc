// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/test/example_proto/library.pbzero.h"
#include "components/tracing/test/example_proto/test_messages.pbzero.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {
namespace proto {

using foo::bar::CamelCaseFields;

TEST(ProtoZeroTest, Simple) {
  // TODO(kraynov) Put tests in the next CL (crbug.com/608721).

  // Test the includes for indirect public import: library.pbzero.h ->
  // library_internals/galaxies.pbzero.h -> upper_import.pbzero.h .
  EXPECT_LE(0u, sizeof(foo::bar::TrickyPublicImport));
}

TEST(ProtoZeroTest, FieldNumbers) {
  // Tests camel case conversion as well.
  EXPECT_EQ(1, CamelCaseFields::kFooBarBazFieldNumber);
  EXPECT_EQ(2, CamelCaseFields::kBarBazFieldNumber);
  EXPECT_EQ(3, CamelCaseFields::kMooMooFieldNumber);
  EXPECT_EQ(4, CamelCaseFields::kURLEncoderFieldNumber);
  EXPECT_EQ(5, CamelCaseFields::kXMapFieldNumber);
  EXPECT_EQ(6, CamelCaseFields::kUrLENcoDerFieldNumber);
  EXPECT_EQ(7, CamelCaseFields::kBigBangFieldNumber);
  EXPECT_EQ(8, CamelCaseFields::kU2FieldNumber);
  EXPECT_EQ(9, CamelCaseFields::kBangBigFieldNumber);
}

}  // namespace proto
}  // namespace tracing
