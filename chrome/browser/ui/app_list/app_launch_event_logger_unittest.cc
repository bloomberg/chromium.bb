// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/app_launch_event_logger.h"

#include "base/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "components/arc/arc_prefs.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/ukm/app_source_url_recorder.h"
#include "components/ukm/test_ukm_recorder.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_util.h"

namespace app_list {

const char kGmailChromeApp[] = "pjkljhegncpnkpknbcohdijeoejaedia";
const char kMapsArcApp[] = "gmhipfhgnoelkiiofcnimehjnpaejiel";
const char kPhotosPWAApp[] = "ncmjhecbjeaamljdfahankockkkdmedg";

const char kMapsPackageName[] = "com.google.android.apps.maps";

namespace {

bool TestIsWebstoreExtension(base::StringPiece id) {
  return (id == kGmailChromeApp);
}

}  // namespace

class AppLaunchEventLoggerTest : public testing::Test {
 protected:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(ukm::kUkmAppLogging);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
};

TEST_F(AppLaunchEventLoggerTest, CheckUkmCodePWA) {
  GURL url("https://photos.google.com/");

  AppLaunchEventLogger app_launch_event_logger_;
  app_launch_event_logger_.SetAppDataForTesting(nullptr, nullptr, nullptr);
  app_launch_event_logger_.OnGridClicked(kPhotosPWAApp);

  scoped_task_environment_.RunUntilIdle();

  const auto entries = test_ukm_recorder_.GetEntriesByName("AppListAppLaunch");
  ASSERT_EQ(1ul, entries.size());
  const auto* entry = entries.back();
  test_ukm_recorder_.ExpectEntrySourceHasUrl(entry, url);
}

TEST_F(AppLaunchEventLoggerTest, CheckUkmCodeChrome) {
  extensions::ExtensionRegistry registry(nullptr);
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder("test")
          .SetID(kGmailChromeApp)
          .AddFlags(extensions::Extension::FROM_WEBSTORE)
          .Build();
  registry.AddEnabled(extension);

  GURL url(std::string("chrome-extension://") + kGmailChromeApp + "/");

  url::AddStandardScheme("chrome-extension", url::SCHEME_WITH_HOST);
  test_ukm_recorder_.SetIsWebstoreExtensionCallback(
      base::BindRepeating(&TestIsWebstoreExtension));

  AppLaunchEventLogger app_launch_event_logger_;
  app_launch_event_logger_.SetAppDataForTesting(&registry, nullptr, nullptr);
  app_launch_event_logger_.OnGridClicked(kGmailChromeApp);

  scoped_task_environment_.RunUntilIdle();

  const auto entries = test_ukm_recorder_.GetEntriesByName("AppListAppLaunch");
  ASSERT_EQ(1ul, entries.size());
  const auto* entry = entries.back();
  test_ukm_recorder_.ExpectEntrySourceHasUrl(entry, url);
}

TEST_F(AppLaunchEventLoggerTest, CheckUkmCodeArc) {
  base::Value package(base::Value::Type::DICTIONARY);
  package.SetKey(AppLaunchEventLogger::kShouldSync, base::Value(true));

  auto packages = std::make_unique<base::DictionaryValue>();
  packages->SetKey(kMapsPackageName, package.Clone());

  base::Value app(base::Value::Type::DICTIONARY);
  app.SetKey(AppLaunchEventLogger::kPackageName, base::Value(kMapsPackageName));

  auto arc_apps = std::make_unique<base::DictionaryValue>();
  arc_apps->SetKey(kMapsArcApp, app.Clone());

  GURL url("app://play/gbpfhehadcpcndihhameeacbdmbjbhgi");

  AppLaunchEventLogger app_launch_event_logger_;
  app_launch_event_logger_.SetAppDataForTesting(nullptr, arc_apps.get(),
                                                packages.get());
  app_launch_event_logger_.OnGridClicked(kMapsArcApp);

  scoped_task_environment_.RunUntilIdle();

  const auto entries = test_ukm_recorder_.GetEntriesByName("AppListAppLaunch");
  ASSERT_EQ(1ul, entries.size());
  const auto* entry = entries.back();
  test_ukm_recorder_.ExpectEntrySourceHasUrl(entry, url);
}

TEST_F(AppLaunchEventLoggerTest, CheckUkmCodeSuggestionChip) {
  GURL url("https://photos.google.com/");

  AppLaunchEventLogger app_launch_event_logger_;
  app_launch_event_logger_.SetAppDataForTesting(nullptr, nullptr, nullptr);
  app_launch_event_logger_.OnSuggestionChipClicked(kPhotosPWAApp, 2);

  scoped_task_environment_.RunUntilIdle();

  const auto entries = test_ukm_recorder_.GetEntriesByName("AppListAppLaunch");
  ASSERT_EQ(1ul, entries.size());
  const auto* entry = entries.back();
  test_ukm_recorder_.ExpectEntrySourceHasUrl(entry, url);
  test_ukm_recorder_.ExpectEntryMetric(entry, "PositionIndex", 2);
}

}  // namespace app_list
