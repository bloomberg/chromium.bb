// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/hash_util.h"

#include "mojo/public/interfaces/bindings/tests/test_structs.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

using HashTest = testing::Test;

static ContainsOtherPtr MakeContainsOther(uint64_t other) {
  ContainsOtherPtr ptr = ContainsOther::New();
  ptr->other = other;
  return ptr;
}

static SimpleNestedStructPtr MakeSimpleNestedStruct(ContainsOtherPtr nested) {
  SimpleNestedStructPtr ptr = SimpleNestedStruct::New();
  ptr->nested = std::move(nested);
  return ptr;
}

TEST_F(HashTest, NestedStruct) {
  // Just check that this template instantiation compiles.
  ASSERT_EQ(
      ::mojo::internal::Hash(::mojo::internal::kHashSeed,
                             MakeSimpleNestedStruct(MakeContainsOther(1))),
      ::mojo::internal::Hash(::mojo::internal::kHashSeed,
                             MakeSimpleNestedStruct(MakeContainsOther(1))));
}

TEST_F(HashTest, UnmappedNativeStruct) {
  // Just check that this template instantiation compiles.
  ASSERT_EQ(::mojo::internal::Hash(::mojo::internal::kHashSeed,
                                   UnmappedNativeStruct::New()),
            ::mojo::internal::Hash(::mojo::internal::kHashSeed,
                                   UnmappedNativeStruct::New()));
}

}  // namespace
}  // namespace test
}  // namespace mojo
