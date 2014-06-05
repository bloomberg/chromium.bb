// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/system/options_validation.h"

#include <stddef.h>
#include <stdint.h>

#include "mojo/public/c/system/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace system {
namespace {

// Declare a test options struct just as we do in actual public headers.

typedef uint32_t TestOptionsFlags;

MOJO_COMPILE_ASSERT(MOJO_ALIGNOF(int64_t) == 8, int64_t_has_weird_alignment);
struct MOJO_ALIGNAS(8) TestOptions {
  uint32_t struct_size;
  TestOptionsFlags flags;
  uint32_t member1;
  uint32_t member2;
};
MOJO_COMPILE_ASSERT(sizeof(TestOptions) == 16, TestOptions_has_wrong_size);

TEST(OptionsValidationTest, Valid) {
  const TestOptions kOptions1 = {static_cast<uint32_t>(sizeof(TestOptions))};

  EXPECT_TRUE(IsOptionsStructPointerAndSizeValid<TestOptions>(&kOptions1));
  EXPECT_TRUE(HAS_OPTIONS_STRUCT_MEMBER(TestOptions, flags, &kOptions1));
  EXPECT_TRUE(HAS_OPTIONS_STRUCT_MEMBER(TestOptions, member1, &kOptions1));
  EXPECT_TRUE(HAS_OPTIONS_STRUCT_MEMBER(TestOptions, member2, &kOptions1));

  const TestOptions kOptions2 = {offsetof(TestOptions, struct_size) +
                                     sizeof(uint32_t)};
  EXPECT_TRUE(IsOptionsStructPointerAndSizeValid<TestOptions>(&kOptions2));
  EXPECT_FALSE(HAS_OPTIONS_STRUCT_MEMBER(TestOptions, flags, &kOptions2));
  EXPECT_FALSE(HAS_OPTIONS_STRUCT_MEMBER(TestOptions, member1, &kOptions2));
  EXPECT_FALSE(HAS_OPTIONS_STRUCT_MEMBER(TestOptions, member2, &kOptions2));

  const TestOptions kOptions3 = {offsetof(TestOptions, flags) +
                                     sizeof(uint32_t)};
  EXPECT_TRUE(IsOptionsStructPointerAndSizeValid<TestOptions>(&kOptions3));
  EXPECT_TRUE(HAS_OPTIONS_STRUCT_MEMBER(TestOptions, flags, &kOptions3));
  EXPECT_FALSE(HAS_OPTIONS_STRUCT_MEMBER(TestOptions, member1, &kOptions3));
  EXPECT_FALSE(HAS_OPTIONS_STRUCT_MEMBER(TestOptions, member2, &kOptions3));

  MOJO_ALIGNAS(8) char buf[sizeof(TestOptions) + 100] = {};
  TestOptions* options = reinterpret_cast<TestOptions*>(buf);
  options->struct_size = sizeof(TestOptions) + 1;
  EXPECT_TRUE(IsOptionsStructPointerAndSizeValid<TestOptions>(options));
  EXPECT_TRUE(HAS_OPTIONS_STRUCT_MEMBER(TestOptions, flags, options));
  EXPECT_TRUE(HAS_OPTIONS_STRUCT_MEMBER(TestOptions, member1, options));
  EXPECT_TRUE(HAS_OPTIONS_STRUCT_MEMBER(TestOptions, member2, options));

  options->struct_size = sizeof(TestOptions) + 4;
  EXPECT_TRUE(IsOptionsStructPointerAndSizeValid<TestOptions>(options));
  EXPECT_TRUE(HAS_OPTIONS_STRUCT_MEMBER(TestOptions, flags, options));
  EXPECT_TRUE(HAS_OPTIONS_STRUCT_MEMBER(TestOptions, member1, options));
  EXPECT_TRUE(HAS_OPTIONS_STRUCT_MEMBER(TestOptions, member2, options));
}

TEST(OptionsValidationTest, Invalid) {
  // Null:
  EXPECT_FALSE(IsOptionsStructPointerAndSizeValid<TestOptions>(NULL));

  // Unaligned:
  EXPECT_FALSE(IsOptionsStructPointerAndSizeValid<TestOptions>(
                   reinterpret_cast<const void*>(1)));
  EXPECT_FALSE(IsOptionsStructPointerAndSizeValid<TestOptions>(
                   reinterpret_cast<const void*>(4)));

  // Size too small:
  for (size_t i = 0; i < sizeof(uint32_t); i++) {
    TestOptions options = {static_cast<uint32_t>(i)};
    EXPECT_FALSE(IsOptionsStructPointerAndSizeValid<TestOptions>(&options))
        << i;
  }
}

TEST(OptionsValidationTest, CheckFlags) {
  TestOptions kOptions1 = {static_cast<uint32_t>(sizeof(TestOptions)), 0};
  EXPECT_TRUE(AreOptionsFlagsAllKnown<TestOptions>(&kOptions1, 0u));
  EXPECT_TRUE(AreOptionsFlagsAllKnown<TestOptions>(&kOptions1, 1u));
  EXPECT_TRUE(AreOptionsFlagsAllKnown<TestOptions>(&kOptions1, 3u));
  EXPECT_TRUE(AreOptionsFlagsAllKnown<TestOptions>(&kOptions1, 7u));
  EXPECT_TRUE(AreOptionsFlagsAllKnown<TestOptions>(&kOptions1, ~0u));

  TestOptions kOptions2 = {static_cast<uint32_t>(sizeof(TestOptions)), 1};
  EXPECT_FALSE(AreOptionsFlagsAllKnown<TestOptions>(&kOptions2, 0u));
  EXPECT_TRUE(AreOptionsFlagsAllKnown<TestOptions>(&kOptions2, 1u));
  EXPECT_TRUE(AreOptionsFlagsAllKnown<TestOptions>(&kOptions2, 3u));
  EXPECT_TRUE(AreOptionsFlagsAllKnown<TestOptions>(&kOptions2, 7u));
  EXPECT_TRUE(AreOptionsFlagsAllKnown<TestOptions>(&kOptions2, ~0u));

  TestOptions kOptions3 = {static_cast<uint32_t>(sizeof(TestOptions)), 2};
  EXPECT_FALSE(AreOptionsFlagsAllKnown<TestOptions>(&kOptions3, 0u));
  EXPECT_FALSE(AreOptionsFlagsAllKnown<TestOptions>(&kOptions3, 1u));
  EXPECT_TRUE(AreOptionsFlagsAllKnown<TestOptions>(&kOptions3, 3u));
  EXPECT_TRUE(AreOptionsFlagsAllKnown<TestOptions>(&kOptions3, 7u));
  EXPECT_TRUE(AreOptionsFlagsAllKnown<TestOptions>(&kOptions3, ~0u));

  TestOptions kOptions4 = {static_cast<uint32_t>(sizeof(TestOptions)), 5};
  EXPECT_FALSE(AreOptionsFlagsAllKnown<TestOptions>(&kOptions4, 0u));
  EXPECT_FALSE(AreOptionsFlagsAllKnown<TestOptions>(&kOptions4, 1u));
  EXPECT_FALSE(AreOptionsFlagsAllKnown<TestOptions>(&kOptions4, 3u));
  EXPECT_TRUE(AreOptionsFlagsAllKnown<TestOptions>(&kOptions4, 7u));
  EXPECT_TRUE(AreOptionsFlagsAllKnown<TestOptions>(&kOptions4, ~0u));
}

}  // namespace
}  // namespace system
}  // namespace mojo
