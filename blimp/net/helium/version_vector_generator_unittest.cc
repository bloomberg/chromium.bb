// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/helium/version_vector_generator.h"

#include "base/macros.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blimp {
namespace {

class VersionVectorGeneratorTest : public testing::Test {
 public:
  VersionVectorGeneratorTest() {}
  ~VersionVectorGeneratorTest() override {}

 protected:
  VersionVectorGenerator gen_;

 private:
  DISALLOW_COPY_AND_ASSIGN(VersionVectorGeneratorTest);
};

TEST_F(VersionVectorGeneratorTest, CheckCurrentDoesntIncrease) {
  VersionVector c1 = gen_.current();
  EXPECT_EQ(VersionVector::Comparison::EqualTo, c1.CompareTo(gen_.current()));
}

TEST_F(VersionVectorGeneratorTest, MonotonicallyIncreasing) {
  VersionVector c1 = gen_.current();
  gen_.Increment();
  VersionVector c2 = gen_.current();
  EXPECT_EQ(VersionVector::Comparison::GreaterThan, c2.CompareTo(c1));
  EXPECT_EQ(VersionVector::Comparison::EqualTo, c2.CompareTo(gen_.current()));
}

}  // namespace
}  // namespace blimp
