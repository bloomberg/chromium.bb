// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/safe_browsing/password_protection/password_protection_service.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "components/safe_browsing_db/test_database_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kPasswordReuseMatchWhitelistHistogramName[] =
    "PasswordManager.PasswordReuse.MainFrameMatchCsdWhitelist";
const char kWhitelistedUrl[] = "http://inwhitelist.com";
const char kNoneWhitelistedUrl[] = "http://notinwhitelist.com";

}  // namespace

namespace safe_browsing {

class MockSafeBrowsingDatabaseManager : public TestSafeBrowsingDatabaseManager {
 public:
  MockSafeBrowsingDatabaseManager() {}

  MOCK_METHOD1(MatchCsdWhitelistUrl, bool(const GURL&));

 protected:
  ~MockSafeBrowsingDatabaseManager() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSafeBrowsingDatabaseManager);
};

class PasswordProtectionServiceTest : public testing::Test {
 public:
  PasswordProtectionServiceTest(){};

  void SetUp() override {
    database_manager_ = new MockSafeBrowsingDatabaseManager();
    password_protection_service_ =
        base::MakeUnique<PasswordProtectionService>(database_manager_);
  }

 protected:
  // |thread_bundle_| is needed here because this test involves both UI and IO
  // threads.
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<MockSafeBrowsingDatabaseManager> database_manager_;
  std::unique_ptr<PasswordProtectionService> password_protection_service_;
};

TEST_F(PasswordProtectionServiceTest,
       TestPasswordReuseMatchWhitelistHistogram) {
  const GURL whitelisted_url(kWhitelistedUrl);
  const GURL not_whitelisted_url(kNoneWhitelistedUrl);
  EXPECT_CALL(*database_manager_.get(), MatchCsdWhitelistUrl(whitelisted_url))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*database_manager_.get(),
              MatchCsdWhitelistUrl(not_whitelisted_url))
      .WillOnce(testing::Return(false));
  base::HistogramTester histograms;
  histograms.ExpectTotalCount(kPasswordReuseMatchWhitelistHistogramName, 0);

  // Empty url should not increment metric.
  password_protection_service_->RecordPasswordReuse(GURL());
  base::RunLoop().RunUntilIdle();
  histograms.ExpectTotalCount(kPasswordReuseMatchWhitelistHistogramName, 0);

  // Whitelisted url should increase "True" bucket by 1.
  password_protection_service_->RecordPasswordReuse(whitelisted_url);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(
      histograms.GetAllSamples(kPasswordReuseMatchWhitelistHistogramName),
      testing::ElementsAre(base::Bucket(1, 1)));

  // Non-whitelisted url should increase "False" bucket by 1.
  password_protection_service_->RecordPasswordReuse(not_whitelisted_url);
  base::RunLoop().RunUntilIdle();
  EXPECT_THAT(
      histograms.GetAllSamples(kPasswordReuseMatchWhitelistHistogramName),
      testing::ElementsAre(base::Bucket(0, 1), base::Bucket(1, 1)));
}

}  // namespace safe_browsing
