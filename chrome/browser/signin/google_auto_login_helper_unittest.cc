// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <vector>

#include "base/message_loop/message_loop.h"
#include "chrome/browser/signin/google_auto_login_helper.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockUbertokenFetcher : public UbertokenFetcher {
 public:
  MockUbertokenFetcher(Profile* profile, UbertokenConsumer* consumer) :
      UbertokenFetcher(profile, consumer), account_id("") {}

  void completePendingRequest() {
    OnUberAuthTokenSuccess("mock token: " + account_id);
  }

  virtual void StartFetchingToken(const std::string& id) OVERRIDE {
    account_id = id;
  }

  std::string account_id;
};

// Counts number of InstrumentedGoogleAutoLoginHelper created.
// We can EXPECT_* to be zero at the end of our unit tests
// to make sure everything is properly deleted.

int total = 0;

class InstrumentedGoogleAutoLoginHelper : public GoogleAutoLoginHelper {
 public:
  std::vector<MockUbertokenFetcher*>* mock_uber_fetchers_;
  TestingProfile profile_;

  InstrumentedGoogleAutoLoginHelper(
    std::vector<MockUbertokenFetcher*>* fetchers) :
    GoogleAutoLoginHelper(0), mock_uber_fetchers_(fetchers) {
    total++;
  }

  virtual ~InstrumentedGoogleAutoLoginHelper() {
    total--;
  }

  virtual void OnUbertokenSuccess(const std::string& token) OVERRIDE {
    OnMergeSessionSuccess(token);
  }

  virtual UbertokenFetcher* CreateNewUbertokenFetcher() OVERRIDE {
    MockUbertokenFetcher* m = new MockUbertokenFetcher(&profile_, this);
    mock_uber_fetchers_->push_back(m);
    return m;
  }

  void completePendingFetchers() {
    while (!mock_uber_fetchers_->empty()) {
      MockUbertokenFetcher* fetcher = mock_uber_fetchers_->back();
      mock_uber_fetchers_->pop_back();
      fetcher->completePendingRequest();
    }
  }

  MOCK_METHOD1(OnMergeSessionSuccess, void(const std::string& uber_token));

  void OnMergeSessionSuccessConcrete(const std::string& uber_token) {
    GoogleAutoLoginHelper::OnMergeSessionSuccess(uber_token);
  }
};

class GoogleAutoLoginHelperTest : public testing::Test {
 public:
  std::vector<MockUbertokenFetcher*> mock_uber_fetchers_;
  base::MessageLoop message_loop_;
};

} // namespace

using ::testing::Invoke;
using ::testing::_;

TEST_F(GoogleAutoLoginHelperTest, AllRequestsInOneGo) {
  total = 0;

  InstrumentedGoogleAutoLoginHelper* helper =
      new InstrumentedGoogleAutoLoginHelper(&mock_uber_fetchers_);
  EXPECT_CALL(*helper, OnMergeSessionSuccess("mock token: acc1@gmail.com"));
  EXPECT_CALL(*helper, OnMergeSessionSuccess("mock token: acc2@gmail.com"));
  EXPECT_CALL(*helper, OnMergeSessionSuccess("mock token: acc3@gmail.com"));
  EXPECT_CALL(*helper, OnMergeSessionSuccess("mock token: acc4@gmail.com"));
  EXPECT_CALL(*helper, OnMergeSessionSuccess("mock token: acc5@gmail.com"));

  // Don't completely mock out OnMergeSessionSuccess, let GoogleAutoLoginHelper
  // actually call OnMergeSessionSuccess to clean up it's work queue because
  // it'll delete itself once the work queue becomes empty.
  ON_CALL(*helper, OnMergeSessionSuccess(_)).WillByDefault(Invoke(helper,
          &InstrumentedGoogleAutoLoginHelper::OnMergeSessionSuccessConcrete));
  helper->LogIn("acc1@gmail.com");
  helper->LogIn("acc2@gmail.com");
  helper->LogIn("acc3@gmail.com");
  helper->LogIn("acc4@gmail.com");
  helper->LogIn("acc5@gmail.com");
  helper->completePendingFetchers();

  // Make sure GoogleAutoLoginHelper cleans itself up after everything is done.
  message_loop_.RunUntilIdle();
  EXPECT_EQ(0, total);
}
