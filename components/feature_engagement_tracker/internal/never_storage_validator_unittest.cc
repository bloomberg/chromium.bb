// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feature_engagement_tracker/internal/never_storage_validator.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace feature_engagement_tracker {

namespace {

class NeverStorageValidatorTest : public ::testing::Test {
 public:
  NeverStorageValidatorTest() = default;

 protected:
  NeverStorageValidator validator_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NeverStorageValidatorTest);
};

}  // namespace

TEST_F(NeverStorageValidatorTest, ShouldNeverKeep) {
  EXPECT_FALSE(validator_.ShouldStore("dummy event"));
}

TEST_F(NeverStorageValidatorTest, ShouldNeverStore) {
  EXPECT_FALSE(validator_.ShouldKeep("dummy event", 99, 100));
  EXPECT_FALSE(validator_.ShouldKeep("dummy event", 100, 100));
  EXPECT_FALSE(validator_.ShouldKeep("dummy event", 101, 100));
}

}  // namespace feature_engagement_tracker
