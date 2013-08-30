// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_app_registry.h"

#include "base/files/file_path.h"
#include "base/prefs/testing_pref_service.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/drive/job_scheduler.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/gdata_wapi_parser.h"
#include "chrome/browser/google_apis/test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {

class DriveAppRegistryTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    pref_service_.reset(new TestingPrefServiceSimple);
    test_util::RegisterDrivePrefs(pref_service_->registry());

    fake_drive_service_.reset(new FakeDriveService);
    fake_drive_service_->LoadAppListForDriveApi("drive/applist.json");

    scheduler_.reset(new JobScheduler(pref_service_.get(),
                                      fake_drive_service_.get(),
                                      base::MessageLoopProxy::current().get()));

    web_apps_registry_.reset(new DriveAppRegistry(scheduler_.get()));
  }

  bool VerifyApp(const ScopedVector<DriveAppInfo>& list,
                 const std::string& web_store_id,
                 const std::string& app_id,
                 const std::string& app_name,
                 const std::string& object_type,
                 bool is_primary) {
    bool found = false;
    for (ScopedVector<DriveAppInfo>::const_iterator it = list.begin();
         it != list.end(); ++it) {
      const DriveAppInfo* app = *it;
      if (web_store_id == app->web_store_id) {
        EXPECT_EQ(app_id, app->app_id);
        EXPECT_EQ(app_name, app->app_name);
        EXPECT_EQ(object_type, app->object_type);
        EXPECT_EQ(is_primary, app->is_primary_selector);
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found) << "Unable to find app with web_store_id "
                       << web_store_id;
    return found;
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<TestingPrefServiceSimple> pref_service_;
  scoped_ptr<FakeDriveService> fake_drive_service_;
  scoped_ptr<JobScheduler> scheduler_;
  scoped_ptr<DriveAppRegistry> web_apps_registry_;
};

TEST_F(DriveAppRegistryTest, LoadAndFindDriveApps) {
  web_apps_registry_->Update();
  base::RunLoop().RunUntilIdle();

  // Find by primary extension 'exe'.
  ScopedVector<DriveAppInfo> ext_results;
  base::FilePath ext_file(FILE_PATH_LITERAL("drive/file.exe"));
  web_apps_registry_->GetAppsForFile(ext_file, std::string(), &ext_results);
  ASSERT_EQ(1U, ext_results.size());
  VerifyApp(ext_results, "abcdefghabcdefghabcdefghabcdefgh", "123456788192",
            "Drive app 1", "", true);

  // Find by primary MIME type.
  ScopedVector<DriveAppInfo> primary_app;
  web_apps_registry_->GetAppsForFile(base::FilePath(),
      "application/vnd.google-apps.drive-sdk.123456788192", &primary_app);
  ASSERT_EQ(1U, primary_app.size());
  VerifyApp(primary_app, "abcdefghabcdefghabcdefghabcdefgh", "123456788192",
            "Drive app 1", "", true);

  // Find by secondary MIME type.
  ScopedVector<DriveAppInfo> secondary_app;
  web_apps_registry_->GetAppsForFile(
      base::FilePath(), "text/html", &secondary_app);
  ASSERT_EQ(1U, secondary_app.size());
  VerifyApp(secondary_app, "abcdefghabcdefghabcdefghabcdefgh", "123456788192",
            "Drive app 1", "", false);
}

TEST_F(DriveAppRegistryTest, UpdateFromAppList) {
  scoped_ptr<base::Value> app_info_value =
      google_apis::test_util::LoadJSONFile("drive/applist.json");
  scoped_ptr<google_apis::AppList> app_list(
      google_apis::AppList::CreateFrom(*app_info_value));

  web_apps_registry_->UpdateFromAppList(*app_list);

  // Confirm that something was loaded from applist.json.
  ScopedVector<DriveAppInfo> ext_results;
  base::FilePath ext_file(FILE_PATH_LITERAL("drive/file.exe"));
  web_apps_registry_->GetAppsForFile(ext_file, std::string(), &ext_results);
  ASSERT_EQ(1U, ext_results.size());
}

TEST_F(DriveAppRegistryTest, MultipleUpdate) {
  // Call Update().
  web_apps_registry_->Update();

  // Call Update() again.
  // This call should be ignored because there is already an ongoing update.
  web_apps_registry_->Update();

  // The app list should be loaded only once.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1, fake_drive_service_->app_list_load_count());
}

TEST(DriveAppRegistryUtilTest, FindPreferredIcon_Empty) {
  google_apis::InstalledApp::IconList icons;
  EXPECT_EQ("",
            util::FindPreferredIcon(icons, util::kPreferredIconSize).spec());
}

TEST(DriveAppRegistryUtilTest, FindPreferredIcon_) {
  const char kSmallerIconUrl[] = "http://example.com/smaller.png";
  const char kMediumIconUrl[] = "http://example.com/medium.png";
  const char kBiggerIconUrl[] = "http://example.com/bigger.png";
  const int kMediumSize = 16;

  google_apis::InstalledApp::IconList icons;
  // The icons are not sorted by the size.
  icons.push_back(std::make_pair(kMediumSize,
                                 GURL(kMediumIconUrl)));
  icons.push_back(std::make_pair(kMediumSize + 2,
                                 GURL(kBiggerIconUrl)));
  icons.push_back(std::make_pair(kMediumSize - 2,
                                 GURL(kSmallerIconUrl)));

  // Exact match.
  EXPECT_EQ(kMediumIconUrl,
            util::FindPreferredIcon(icons, kMediumSize).spec());
  // The requested size is in-between of smaller.png and
  // medium.png. medium.png should be returned.
  EXPECT_EQ(kMediumIconUrl,
            util::FindPreferredIcon(icons, kMediumSize - 1).spec());
  // The requested size is smaller than the smallest icon. The smallest icon
  // should be returned.
  EXPECT_EQ(kSmallerIconUrl,
            util::FindPreferredIcon(icons, kMediumSize - 3).spec());
  // The requested size is larger than the largest icon. The largest icon
  // should be returned.
  EXPECT_EQ(kBiggerIconUrl,
            util::FindPreferredIcon(icons, kMediumSize + 3).spec());
}

}  // namespace drive
