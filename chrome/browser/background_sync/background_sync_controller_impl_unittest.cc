// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_sync/background_sync_controller_impl.h"

#include "chrome/test/base/testing_profile.h"
#include "components/rappor/test_rappor_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class TestBackgroundSyncControllerImpl : public BackgroundSyncControllerImpl {
 public:
  TestBackgroundSyncControllerImpl(Profile* profile,
                                   rappor::TestRapporService* rappor_service)
      : BackgroundSyncControllerImpl(profile),
        rappor_service_(rappor_service) {}

 protected:
  rappor::RapporService* GetRapporService() override { return rappor_service_; }

 private:
  rappor::TestRapporService* rappor_service_;

  DISALLOW_COPY_AND_ASSIGN(TestBackgroundSyncControllerImpl);
};

class BackgroundSyncControllerImplTest : public testing::Test {
 protected:
  BackgroundSyncControllerImplTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        controller_(new TestBackgroundSyncControllerImpl(&profile_,
                                                         &rappor_service_)) {}

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  rappor::TestRapporService rappor_service_;
  scoped_ptr<TestBackgroundSyncControllerImpl> controller_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncControllerImplTest);
};

TEST_F(BackgroundSyncControllerImplTest, RapporTest) {
  GURL url("http://www.example.com/foo/");
  EXPECT_EQ(0, rappor_service_.GetReportsCount());
  controller_->NotifyBackgroundSyncRegistered(url.GetOrigin());
  EXPECT_EQ(1, rappor_service_.GetReportsCount());

  std::string sample;
  rappor::RapporType type;
  LOG(ERROR) << url.GetOrigin().GetOrigin();
  EXPECT_TRUE(rappor_service_.GetRecordedSampleForMetric(
      "BackgroundSync.Register.Origin", &sample, &type));
  EXPECT_EQ("example.com", sample);
  EXPECT_EQ(rappor::ETLD_PLUS_ONE_RAPPOR_TYPE, type);
}

TEST_F(BackgroundSyncControllerImplTest, NoRapporWhenOffTheRecord) {
  GURL url("http://www.example.com/foo/");
  controller_.reset(new TestBackgroundSyncControllerImpl(
      profile_.GetOffTheRecordProfile(), &rappor_service_));

  controller_->NotifyBackgroundSyncRegistered(url.GetOrigin());
  EXPECT_EQ(0, rappor_service_.GetReportsCount());
}

}  // namespace
