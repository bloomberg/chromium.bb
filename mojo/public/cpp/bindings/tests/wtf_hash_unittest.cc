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

TEST_F(WTFHashTest, NestedStruct) {
  // Just check that this template instantiation compiles.
  ASSERT_EQ(::mojo::internal::Hash(
                ::mojo::internal::kHashSeed,
                blink::SimpleNestedStruct::New(blink::ContainsOther::New(1))),
            ::mojo::internal::Hash(
                ::mojo::internal::kHashSeed,
                blink::SimpleNestedStruct::New(blink::ContainsOther::New(1))));
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
