// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/wtf_hash_util.h"

#include "mojo/public/interfaces/bindings/tests/test_structs.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

using WTFHashTest = testing::Test;

static blink::ContainsOtherPtr MakeContainsOther(uint64_t other) {
  blink::ContainsOtherPtr ptr = blink::ContainsOther::New();
  ptr->other = other;
  return ptr;
}

static blink::SimpleNestedStructPtr MakeSimpleNestedStruct(
    blink::ContainsOtherPtr nested) {
  blink::SimpleNestedStructPtr ptr = blink::SimpleNestedStruct::New();
  ptr->nested = std::move(nested);
  return ptr;
}

TEST_F(WTFHashTest, NestedStruct) {
  // Just check that this template instantiation compiles.
  ASSERT_EQ(
      ::mojo::internal::Hash(::mojo::internal::kHashSeed,
                             MakeSimpleNestedStruct(MakeContainsOther(1))),
      ::mojo::internal::Hash(::mojo::internal::kHashSeed,
                             MakeSimpleNestedStruct(MakeContainsOther(1))));
}

TEST_F(WTFHashTest, UnmappedNativeStruct) {
  // Just check that this template instantiation compiles.
  ASSERT_EQ(::mojo::internal::Hash(::mojo::internal::kHashSeed,
                                   blink::UnmappedNativeStruct::New()),
            ::mojo::internal::Hash(::mojo::internal::kHashSeed,
                                   blink::UnmappedNativeStruct::New()));
}

}  // namespace
}  // namespace test
}  // namespace mojo
