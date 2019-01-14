// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_launch_event_logger.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "components/ukm/app_source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_util.h"

namespace app_list {

const char kPhotosPWAApp[] = "ncmjhecbjeaamljdfahankockkkdmedg";
const char kGmailChromeApp[] = "pjkljhegncpnkpknbcohdijeoejaedia";

namespace {

bool TestIsWebstoreExtension(base::StringPiece id) {
  return (id == kGmailChromeApp);
}

}  // namespace

class AppLaunchEventLoggerTest : public testing::Test {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(ukm::kUkmAppLogging);
    // Need to create a task runner each time as AppLaunchEventLogger is a
    // singleton and so its constructor won't run for every test.
    AppLaunchEventLogger::GetInstance().CreateTaskRunner();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
};

TEST_F(AppLaunchEventLoggerTest, CheckUkmCodePWA) {
  GURL url("https://photos.google.com/");

  AppLaunchEventLogger::GetInstance().OnGridClicked(kPhotosPWAApp);

  scoped_task_environment_.RunUntilIdle();

  const auto entries = test_ukm_recorder_.GetEntriesByName("AppListAppLaunch");
  ASSERT_EQ(1ul, entries.size());
  const auto* entry = entries.back();
  test_ukm_recorder_.ExpectEntrySourceHasUrl(entry, url);
}

TEST_F(AppLaunchEventLoggerTest, CheckUkmCodeChrome) {
  GURL url(std::string("chrome-extension://") + kGmailChromeApp + "/");

  url::AddStandardScheme("chrome-extension", url::SCHEME_WITH_HOST);
  test_ukm_recorder_.SetIsWebstoreExtensionCallback(
      base::BindRepeating(&TestIsWebstoreExtension));

  AppLaunchEventLogger::GetInstance().OnGridClicked(kGmailChromeApp);

  scoped_task_environment_.RunUntilIdle();

  const auto entries = test_ukm_recorder_.GetEntriesByName("AppListAppLaunch");
  ASSERT_EQ(1ul, entries.size());
  const auto* entry = entries.back();
  test_ukm_recorder_.ExpectEntrySourceHasUrl(entry, url);
}

}  // namespace app_list
