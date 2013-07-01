// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/public/consumer/base/supports_user_data.h"

#include "base/memory/scoped_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace ios {

namespace {
  const char* kTestData1Key = "test_data1";
  const char* kTestData2Key = "test_data2";
}  // namespace

class TestData : public SupportsUserData::Data {
 public:
  TestData(bool* was_destroyed)
      : was_destroyed_(was_destroyed) {
    *was_destroyed_ = false;
  }
  virtual ~TestData() {
    *was_destroyed_ = true;
  }

 private:
  bool* was_destroyed_;
};

class TestSupportsUserData : public SupportsUserData {
 public:
  TestSupportsUserData() {}
  virtual ~TestSupportsUserData() {}
};

class SupportsUserDataTest : public PlatformTest {
 public:
  virtual void SetUp() OVERRIDE {
    PlatformTest::SetUp();

    test_data1_was_destroyed_ = false;
    test_data1_ = new TestData(&test_data1_was_destroyed_);
    test_data2_was_destroyed_ = false;
    test_data2_ = new TestData(&test_data2_was_destroyed_);
    supports_user_data_.reset(new TestSupportsUserData());
  }

  virtual void TearDown() OVERRIDE {
    if (!test_data1_was_destroyed_ &&
        supports_user_data_ &&
        supports_user_data_->GetUserData(kTestData1Key) != test_data1_)
      delete test_data1_;
    if (!test_data2_was_destroyed_ &&
        supports_user_data_ &&
        supports_user_data_->GetUserData(kTestData2Key) != test_data2_)
      delete test_data2_;

    PlatformTest::TearDown();
  }

 protected:
  scoped_ptr<TestSupportsUserData> supports_user_data_;
  bool test_data1_was_destroyed_;
  TestData* test_data1_;
  bool test_data2_was_destroyed_;
  TestData* test_data2_;
};

TEST_F(SupportsUserDataTest, SetAndGetData) {
  EXPECT_FALSE(supports_user_data_->GetUserData(kTestData1Key));
  supports_user_data_->SetUserData(kTestData1Key, test_data1_);
  EXPECT_EQ(supports_user_data_->GetUserData(kTestData1Key), test_data1_);
}

TEST_F(SupportsUserDataTest, DataDestroyedOnDestruction) {
  EXPECT_FALSE(supports_user_data_->GetUserData(kTestData1Key));
  supports_user_data_->SetUserData(kTestData1Key, test_data1_);
  EXPECT_FALSE(test_data1_was_destroyed_);
  supports_user_data_.reset();
  EXPECT_TRUE(test_data1_was_destroyed_);
}

TEST_F(SupportsUserDataTest, DataDestroyedOnRemoval) {
  EXPECT_FALSE(supports_user_data_->GetUserData(kTestData1Key));
  supports_user_data_->SetUserData(kTestData1Key, test_data1_);
  EXPECT_FALSE(test_data1_was_destroyed_);
  supports_user_data_->RemoveUserData(kTestData1Key);
  EXPECT_TRUE(test_data1_was_destroyed_);
}

TEST_F(SupportsUserDataTest, DistinctDataStoredSeparately) {
  EXPECT_FALSE(supports_user_data_->GetUserData(kTestData2Key));
  supports_user_data_->SetUserData(kTestData1Key, test_data1_);
  EXPECT_FALSE(supports_user_data_->GetUserData(kTestData2Key));
  supports_user_data_->SetUserData(kTestData2Key, test_data2_);
  EXPECT_EQ(supports_user_data_->GetUserData(kTestData2Key), test_data2_);
}

TEST_F(SupportsUserDataTest, DistinctDataDestroyedSeparately) {
  supports_user_data_->SetUserData(kTestData1Key, test_data1_);
  supports_user_data_->SetUserData(kTestData2Key, test_data2_);
  EXPECT_FALSE(test_data1_was_destroyed_);
  EXPECT_FALSE(test_data2_was_destroyed_);

  supports_user_data_->RemoveUserData(kTestData2Key);
  EXPECT_FALSE(test_data1_was_destroyed_);
  EXPECT_TRUE(test_data2_was_destroyed_);

  supports_user_data_.reset();
  EXPECT_TRUE(test_data1_was_destroyed_);
}

}  // namespace ios
