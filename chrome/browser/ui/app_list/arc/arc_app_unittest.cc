// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/scoped_vector.h"
#include "base/run_loop.h"
#include "base/task_runner_util.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon_loader.h"
#include "chrome/browser/ui/app_list/arc/arc_app_item.h"
#include "chrome/browser/ui/app_list/arc/arc_app_launcher.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_model_builder.h"
#include "chrome/browser/ui/app_list/arc/arc_app_test.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/arc/test/fake_arc_bridge_service.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/app_list_constants.h"
#include "ui/app_list/app_list_model.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"
#include "ui/gfx/image/image_skia.h"

namespace {

class FakeAppIconLoaderDelegate : public AppIconLoaderDelegate {
 public:
  FakeAppIconLoaderDelegate() {}
  ~FakeAppIconLoaderDelegate() override {}

  void OnAppImageUpdated(const std::string& app_id,
                         const gfx::ImageSkia& image) override {
    app_id_ = app_id;
    image_ = image;
    ++update_image_cnt_;
  }

  size_t update_image_cnt() const { return update_image_cnt_; }

  const std::string& app_id() const { return app_id_; }

  const gfx::ImageSkia& image() { return image_; }

 private:
  size_t update_image_cnt_ = 0;
  std::string app_id_;
  gfx::ImageSkia image_;

  DISALLOW_COPY_AND_ASSIGN(FakeAppIconLoaderDelegate);
};

}  // namespace

class ArcAppModelBuilderTest : public AppListTestBase {
 public:
  ArcAppModelBuilderTest() {}
  ~ArcAppModelBuilderTest() override {
    // Release profile file in order to keep right sequence.
    profile_.reset();
  }

  void SetUp() override {
    AppListTestBase::SetUp();

    arc_test_.SetUp(profile_.get());

    CreateBuilder();
  }

  void TearDown() override { ResetBuilder(); }

 protected:
  // Creates a new builder, destroying any existing one.
  void CreateBuilder() {
    ResetBuilder();  // Destroy any existing builder in the correct order.

    model_.reset(new app_list::AppListModel);
    controller_.reset(new test::TestAppListControllerDelegate);
    builder_.reset(new ArcAppModelBuilder(controller_.get()));
    builder_->InitializeWithProfile(profile_.get(), model_.get());
  }

  void ResetBuilder() {
    builder_.reset();
    controller_.reset();
    model_.reset();
  }

  size_t GetArcItemCount() const {
    size_t arc_count = 0;
    const size_t count = model_->top_level_item_list()->item_count();
    for (size_t i = 0; i < count; ++i) {
      app_list::AppListItem* item = model_->top_level_item_list()->item_at(i);
      if (item->GetItemType() == ArcAppItem::kItemType)
        ++arc_count;
    }
    return arc_count;
  }

  ArcAppItem* GetArcItem(size_t index) const {
    size_t arc_count = 0;
    const size_t count = model_->top_level_item_list()->item_count();
    ArcAppItem* arc_item = nullptr;
    for (size_t i = 0; i < count; ++i) {
      app_list::AppListItem* item = model_->top_level_item_list()->item_at(i);
      if (item->GetItemType() == ArcAppItem::kItemType) {
        if (arc_count++ == index) {
          arc_item = reinterpret_cast<ArcAppItem*>(item);
          break;
        }
      }
    }
    EXPECT_NE(nullptr, arc_item);
    return arc_item;
  }

  ArcAppItem* FindArcItem(const std::string& id) const {
    const size_t count = GetArcItemCount();
    for (size_t i = 0; i < count; ++i) {
      ArcAppItem* item = GetArcItem(i);
      if (item && item->id() == id)
        return item;
    }
    return nullptr;
  }

  // Validate that prefs and model have right content.
  void ValidateHaveApps(const std::vector<arc::mojom::AppInfo> apps) {
    ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
    const std::vector<std::string> ids = prefs->GetAppIds();
    ASSERT_EQ(apps.size(), ids.size());
    ASSERT_EQ(apps.size(), GetArcItemCount());
    // In principle, order of items is not defined.
    for (auto& app : apps) {
      const std::string id = ArcAppTest::GetAppId(app);
      EXPECT_NE(std::find(ids.begin(), ids.end(), id), ids.end());
      std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(id);
      ASSERT_NE(nullptr, app_info.get());
      EXPECT_EQ(app.name, app_info->name);
      EXPECT_EQ(app.package_name, app_info->package_name);
      EXPECT_EQ(app.activity, app_info->activity);

      const ArcAppItem* app_item = FindArcItem(id);
      ASSERT_NE(nullptr, app_item);
      EXPECT_EQ(app.name, app_item->GetDisplayName());
    }
  }

  // Validate that requested apps have required ready state and other apps have
  // opposite state.
  void ValidateAppReadyState(const std::vector<arc::mojom::AppInfo> apps,
                             bool ready) {
    ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
    ASSERT_NE(nullptr, prefs);

    std::vector<std::string> ids = prefs->GetAppIds();
    EXPECT_EQ(ids.size(), GetArcItemCount());

    // Process requested apps.
    for (auto& app : apps) {
      const std::string id = ArcAppTest::GetAppId(app);
      std::vector<std::string>::iterator it_id = std::find(ids.begin(),
                                                           ids.end(),
                                                           id);
      ASSERT_NE(it_id, ids.end());
      ids.erase(it_id);

      std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(id);
      ASSERT_NE(nullptr, app_info.get());
      EXPECT_EQ(ready, app_info->ready);
      const ArcAppItem* app_item = FindArcItem(id);
      ASSERT_NE(nullptr, app_item);
      EXPECT_EQ(ready, app_item->ready());
    }

    // Process the rest of the apps.
    for (auto& id : ids) {
      std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(id);
      ASSERT_NE(nullptr, app_info.get());
      EXPECT_NE(ready, app_info->ready);
      const ArcAppItem* app_item = FindArcItem(id);
      ASSERT_NE(nullptr, app_item);
      EXPECT_NE(ready, app_item->ready());
    }
  }

  // Validates that provided image is acceptable as Arc app icon.
  void ValidateIcon(const gfx::ImageSkia& image) {
    EXPECT_EQ(app_list::kGridIconDimension, image.width());
    EXPECT_EQ(app_list::kGridIconDimension, image.height());

    const std::vector<ui::ScaleFactor>& scale_factors =
        ui::GetSupportedScaleFactors();
    for (auto& scale_factor : scale_factors) {
      const float scale = ui::GetScaleForScaleFactor(scale_factor);
      EXPECT_TRUE(image.HasRepresentation(scale));
      const gfx::ImageSkiaRep& representation = image.GetRepresentation(scale);
      EXPECT_FALSE(representation.is_null());
      EXPECT_EQ(gfx::ToCeiledInt(app_list::kGridIconDimension * scale),
          representation.pixel_width());
      EXPECT_EQ(gfx::ToCeiledInt(app_list::kGridIconDimension * scale),
          representation.pixel_height());
    }
  }

  AppListControllerDelegate* controller() { return controller_.get(); }

  Profile* profile() { return profile_.get(); }

  const std::vector<arc::mojom::AppInfo>& fake_apps() const {
    return arc_test_.fake_apps();
  }

  arc::FakeArcBridgeService* bridge_service() {
    return arc_test_.bridge_service();
  }

  arc::FakeAppInstance* app_instance() {
    return arc_test_.app_instance();
  }

 private:
  ArcAppTest arc_test_;
  std::unique_ptr<app_list::AppListModel> model_;
  std::unique_ptr<test::TestAppListControllerDelegate> controller_;
  std::unique_ptr<ArcAppModelBuilder> builder_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppModelBuilderTest);
};

TEST_F(ArcAppModelBuilderTest, RefreshAllOnReady) {
  // There should already have been one call, when the interface was
  // registered.
  EXPECT_EQ(1, app_instance()->refresh_app_list_count());
  bridge_service()->SetReady();
  app_instance()->RefreshAppList();
  EXPECT_EQ(2, app_instance()->refresh_app_list_count());
}

TEST_F(ArcAppModelBuilderTest, RefreshAllFillsContent) {
  ValidateHaveApps(std::vector<arc::mojom::AppInfo>());
  bridge_service()->SetReady();
  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(fake_apps());
  ValidateHaveApps(fake_apps());
}

TEST_F(ArcAppModelBuilderTest, MultipleRefreshAll) {
  ValidateHaveApps(std::vector<arc::mojom::AppInfo>());
  bridge_service()->SetReady();
  app_instance()->RefreshAppList();
  // Send info about all fake apps except last.
  std::vector<arc::mojom::AppInfo> apps1(fake_apps().begin(),
                                         fake_apps().end() - 1);
  app_instance()->SendRefreshAppList(apps1);
  // At this point all apps (except last) should exist and be ready.
  ValidateHaveApps(apps1);
  ValidateAppReadyState(apps1, true);

  // Send info about all fake apps except first.
  std::vector<arc::mojom::AppInfo> apps2(fake_apps().begin() + 1,
                                         fake_apps().end());
  app_instance()->SendRefreshAppList(apps2);
  // At this point all apps should exist but first one should be non-ready.
  ValidateHaveApps(apps2);
  ValidateAppReadyState(apps2, true);

  // Send info about all fake apps.
  app_instance()->SendRefreshAppList(fake_apps());
  // At this point all apps should exist and be ready.
  ValidateHaveApps(fake_apps());
  ValidateAppReadyState(fake_apps(), true);

  // Send info no app available.
  std::vector<arc::mojom::AppInfo> no_apps;
  app_instance()->SendRefreshAppList(no_apps);
  // At this point no app should exist.
  ValidateHaveApps(no_apps);
}

TEST_F(ArcAppModelBuilderTest, StopStartServicePreserveApps) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  bridge_service()->SetReady();
  app_instance()->RefreshAppList();
  EXPECT_EQ(0u, GetArcItemCount());
  EXPECT_EQ(0u, prefs->GetAppIds().size());

  app_instance()->SendRefreshAppList(fake_apps());
  std::vector<std::string> ids = prefs->GetAppIds();
  EXPECT_EQ(fake_apps().size(), ids.size());
  ValidateAppReadyState(fake_apps(), true);

  // Stopping service does not delete items. It makes them non-ready.
  bridge_service()->SetStopped();
  // Ids should be the same.
  EXPECT_EQ(ids, prefs->GetAppIds());
  ValidateAppReadyState(fake_apps(), false);

  // Setting service ready does not change anything because RefreshAppList is
  // not called.
  bridge_service()->SetReady();
  // Ids should be the same.
  EXPECT_EQ(ids, prefs->GetAppIds());
  ValidateAppReadyState(fake_apps(), false);

  // Refreshing app list makes apps available.
  app_instance()->SendRefreshAppList(fake_apps());
  EXPECT_EQ(ids, prefs->GetAppIds());
  ValidateAppReadyState(fake_apps(), true);
}

TEST_F(ArcAppModelBuilderTest, RestartPreserveApps) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  // Start from scratch and fill with apps.
  bridge_service()->SetReady();
  app_instance()->SendRefreshAppList(fake_apps());
  std::vector<std::string> ids = prefs->GetAppIds();
  EXPECT_EQ(fake_apps().size(), ids.size());
  ValidateAppReadyState(fake_apps(), true);

  // This recreates model and ARC apps will be read from prefs.
  bridge_service()->SetStopped();
  CreateBuilder();
  ValidateAppReadyState(fake_apps(), false);
}

TEST_F(ArcAppModelBuilderTest, LaunchApps) {
  // Disable attempts to dismiss app launcher view.
  ChromeAppListItem::OverrideAppListControllerDelegateForTesting(controller());

  bridge_service()->SetReady();
  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(fake_apps());

  // Simulate item activate.
  const arc::mojom::AppInfo& app_first = fake_apps()[0];
  const arc::mojom::AppInfo& app_last = fake_apps()[0];
  ArcAppItem* item_first = FindArcItem(ArcAppTest::GetAppId(app_first));
  ArcAppItem* item_last = FindArcItem(ArcAppTest::GetAppId(app_last));
  ASSERT_NE(nullptr, item_first);
  ASSERT_NE(nullptr, item_last);
  item_first->Activate(0);
  app_instance()->WaitForIncomingMethodCall();
  item_last->Activate(0);
  app_instance()->WaitForIncomingMethodCall();
  item_first->Activate(0);
  app_instance()->WaitForIncomingMethodCall();

  const ScopedVector<arc::FakeAppInstance::Request>& launch_requests =
      app_instance()->launch_requests();
  ASSERT_EQ(3u, launch_requests.size());
  EXPECT_EQ(true, launch_requests[0]->IsForApp(app_first));
  EXPECT_EQ(true, launch_requests[1]->IsForApp(app_last));
  EXPECT_EQ(true, launch_requests[2]->IsForApp(app_first));

  // Test an attempt to launch of a not-ready app.
  bridge_service()->SetStopped();
  item_first = FindArcItem(ArcAppTest::GetAppId(app_first));
  ASSERT_NE(nullptr, item_first);
  size_t launch_request_count_before = app_instance()->launch_requests().size();
  item_first->Activate(0);
  // Number of launch requests must not change.
  EXPECT_EQ(launch_request_count_before,
            app_instance()->launch_requests().size());
}

TEST_F(ArcAppModelBuilderTest, RequestIcons) {
  // Make sure we are on UI thread.
  ASSERT_TRUE(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  bridge_service()->SetReady();
  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(fake_apps());

  // Validate that no icon exists at the beginning and request icon for
  // each supported scale factor. This will start asynchronous loading.
  uint32_t expected_mask = 0;
  const std::vector<ui::ScaleFactor>& scale_factors =
      ui::GetSupportedScaleFactors();
  for (auto& scale_factor : scale_factors) {
    expected_mask |= 1 <<  scale_factor;
    for (auto& app : fake_apps()) {
      ArcAppItem* app_item = FindArcItem(ArcAppTest::GetAppId(app));
      ASSERT_NE(nullptr, app_item);
      const float scale = ui::GetScaleForScaleFactor(scale_factor);
      app_item->icon().GetRepresentation(scale);
    }
  }

  // Process pending tasks.
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  base::RunLoop().RunUntilIdle();
  // Normally just one call to RunUntilIdle() suffices to make sure
  // all RequestAppIcon() calls are delivered, but on slower machines
  // (especially when running under Valgrind), they might not get
  // delivered on time. Wait for the remaining tasks individually.
  const size_t expected_size = scale_factors.size() * fake_apps().size();
  while (app_instance()->icon_requests().size() < expected_size) {
    app_instance()->WaitForIncomingMethodCall();
  }

  // At this moment we should receive all requests for icon loading.
  const ScopedVector<arc::FakeAppInstance::IconRequest>& icon_requests =
      app_instance()->icon_requests();
  EXPECT_EQ(expected_size, icon_requests.size());
  std::map<std::string, uint32_t> app_masks;
  for (size_t i = 0; i < icon_requests.size(); ++i) {
    const arc::FakeAppInstance::IconRequest* icon_request = icon_requests[i];
    const std::string id = ArcAppListPrefs::GetAppId(
        icon_request->package_name(), icon_request->activity());
    // Make sure no double requests.
    EXPECT_NE(app_masks[id],
              app_masks[id] | (1 << icon_request->scale_factor()));
    app_masks[id] |= (1 << icon_request->scale_factor());
  }

  // Validate that we have a request for each icon for each supported scale
  // factor.
  EXPECT_EQ(fake_apps().size(), app_masks.size());
  for (auto& app : fake_apps()) {
    const std::string id = ArcAppTest::GetAppId(app);
    ASSERT_NE(app_masks.find(id), app_masks.end());
    EXPECT_EQ(app_masks[id], expected_mask);
  }
}

TEST_F(ArcAppModelBuilderTest, InstallIcon) {
  // Make sure we are on UI thread.
  ASSERT_TRUE(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  bridge_service()->SetReady();
  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(std::vector<arc::mojom::AppInfo>(
      fake_apps().begin(), fake_apps().begin() + 1));
  const arc::mojom::AppInfo& app = fake_apps()[0];

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  const ui::ScaleFactor scale_factor = ui::GetSupportedScaleFactors()[0];
  const float scale = ui::GetScaleForScaleFactor(scale_factor);
  const base::FilePath icon_path = prefs->GetIconPath(ArcAppTest::GetAppId(app),
                                                      scale_factor);
  EXPECT_EQ(true, !base::PathExists(icon_path));

  const ArcAppItem* app_item = FindArcItem(ArcAppTest::GetAppId(app));
  EXPECT_NE(nullptr, app_item);
  // This initiates async loading.
  app_item->icon().GetRepresentation(scale);

  // Process pending tasks.
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  base::RunLoop().RunUntilIdle();

  // Validating decoded content does not fit well for unit tests.
  ArcAppIcon::DisableSafeDecodingForTesting();

  // Now send generated icon for the app.
  std::string png_data;
  EXPECT_EQ(true, app_instance()->GenerateAndSendIcon(
                      app, static_cast<arc::mojom::ScaleFactor>(scale_factor),
                      &png_data));

  // Process pending tasks.
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  base::RunLoop().RunUntilIdle();

  // Validate that icons are installed, have right content and icon is
  // refreshed for ARC app item.
  EXPECT_EQ(true, base::PathExists(icon_path));

  std::string icon_data;
  // Read the file from disk and compare with reference data.
  EXPECT_EQ(true, base::ReadFileToString(icon_path, &icon_data));
  ASSERT_EQ(icon_data, png_data);
}

TEST_F(ArcAppModelBuilderTest, RemoveAppCleanUpFolder) {
  // Make sure we are on UI thread.
  ASSERT_TRUE(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  bridge_service()->SetReady();
  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(std::vector<arc::mojom::AppInfo>(
      fake_apps().begin(), fake_apps().begin() + 1));
  const arc::mojom::AppInfo& app = fake_apps()[0];

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  const std::string app_id = ArcAppTest::GetAppId(app);
  const base::FilePath app_path = prefs->GetAppPath(app_id);
  const ui::ScaleFactor scale_factor = ui::GetSupportedScaleFactors()[0];

  // No app folder by default.
  EXPECT_EQ(true, !base::PathExists(app_path));

  // Request icon, this will create app folder.
  prefs->RequestIcon(app_id, scale_factor);
  // Now send generated icon for the app.
  std::string png_data;
  EXPECT_EQ(true, app_instance()->GenerateAndSendIcon(
                      app, static_cast<arc::mojom::ScaleFactor>(scale_factor),
                      &png_data));
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(true, base::PathExists(app_path));

  // Send empty app list. This will delete app and its folder.
  app_instance()->SendRefreshAppList(std::vector<arc::mojom::AppInfo>());
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(true, !base::PathExists(app_path));
}

TEST_F(ArcAppModelBuilderTest, LastLaunchTime) {
  // Make sure we are on UI thread.
  ASSERT_TRUE(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  bridge_service()->SetReady();
  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(std::vector<arc::mojom::AppInfo>(
      fake_apps().begin(), fake_apps().begin() + 2));
  const arc::mojom::AppInfo& app1 = fake_apps()[0];
  const arc::mojom::AppInfo& app2 = fake_apps()[1];
  const std::string id1 = ArcAppTest::GetAppId(app1);
  const std::string id2 = ArcAppTest::GetAppId(app2);

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(id1);
  ASSERT_NE(nullptr, app_info.get());

  EXPECT_EQ(base::Time(), app_info->last_launch_time);

  // Test direct setting last launch time.
  base::Time now_time = base::Time::Now();
  prefs->SetLastLaunchTime(id1, now_time);

  app_info = prefs->GetApp(id1);
  ASSERT_NE(nullptr, app_info.get());
  EXPECT_EQ(now_time, app_info->last_launch_time);

  // Test setting last launch time via LaunchApp.
  app_info = prefs->GetApp(id2);
  ASSERT_NE(nullptr, app_info.get());
  EXPECT_EQ(base::Time(), app_info->last_launch_time);

  base::Time time_before = base::Time::Now();
  arc::LaunchApp(profile(), id2);
  base::Time time_after = base::Time::Now();

  app_info = prefs->GetApp(id2);
  ASSERT_NE(nullptr, app_info.get());
  ASSERT_LE(time_before, app_info->last_launch_time);
  ASSERT_GE(time_after, app_info->last_launch_time);
}

TEST_F(ArcAppModelBuilderTest, IconLoader) {
  // Validating decoded content does not fit well for unit tests.
  ArcAppIcon::DisableSafeDecodingForTesting();

  const arc::mojom::AppInfo& app = fake_apps()[0];
  const std::string app_id = ArcAppTest::GetAppId(app);

  bridge_service()->SetReady();
  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(std::vector<arc::mojom::AppInfo>(
      fake_apps().begin(), fake_apps().begin() + 1));

  FakeAppIconLoaderDelegate delegate;
  ArcAppIconLoader icon_loader(profile(), app_list::kListIconSize, &delegate);
  EXPECT_EQ(0UL, delegate.update_image_cnt());
  icon_loader.FetchImage(app_id);
  EXPECT_EQ(1UL, delegate.update_image_cnt());
  EXPECT_EQ(app_id, delegate.app_id());

  // Validate default image.
  ValidateIcon(delegate.image());

  const std::vector<ui::ScaleFactor>& scale_factors =
      ui::GetSupportedScaleFactors();
  for (auto& scale_factor : scale_factors) {
    std::string png_data;
    EXPECT_TRUE(app_instance()->GenerateAndSendIcon(
        app, static_cast<arc::mojom::ScaleFactor>(scale_factor), &png_data));
  }

  // Process pending tasks, this installs icons.
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  base::RunLoop().RunUntilIdle();

  // Allow one more circle to read and decode installed icons.
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  base::RunLoop().RunUntilIdle();

  // Validate loaded image.
  EXPECT_EQ(1 + scale_factors.size(), delegate.update_image_cnt());
  EXPECT_EQ(app_id, delegate.app_id());
  ValidateIcon(delegate.image());
}

TEST_F(ArcAppModelBuilderTest, AppLauncher) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile());

  // App1 is called in deferred mode, after refreshing apps.
  // App2 is never called since app is not avaialble.
  // App3 is never called immediately because app is available already.
  const arc::mojom::AppInfo& app1 = fake_apps()[0];
  const arc::mojom::AppInfo& app2 = fake_apps()[1];
  const arc::mojom::AppInfo& app3 = fake_apps()[2];
  const std::string id1 = ArcAppTest::GetAppId(app1);
  const std::string id2 = ArcAppTest::GetAppId(app2);
  const std::string id3 = ArcAppTest::GetAppId(app3);

  ArcAppLauncher launcher1(profile(), id1, true);
  EXPECT_FALSE(launcher1.app_launched());
  EXPECT_TRUE(prefs->HasObserver(&launcher1));

  bridge_service()->SetReady();

  ArcAppLauncher launcher3(profile(), id3, true);
  EXPECT_FALSE(launcher1.app_launched());
  EXPECT_TRUE(prefs->HasObserver(&launcher1));
  EXPECT_FALSE(launcher3.app_launched());
  EXPECT_TRUE(prefs->HasObserver(&launcher3));

  EXPECT_EQ(0u, app_instance()->launch_requests().size());

  std::vector<arc::mojom::AppInfo> apps(fake_apps().begin(),
                                        fake_apps().begin() + 2);
  app_instance()->SendRefreshAppList(apps);

  EXPECT_TRUE(launcher1.app_launched());
  app_instance()->WaitForIncomingMethodCall();
  ASSERT_EQ(1u, app_instance()->launch_requests().size());
  EXPECT_TRUE(app_instance()->launch_requests()[0]->IsForApp(app1));
  EXPECT_FALSE(launcher3.app_launched());
  EXPECT_FALSE(prefs->HasObserver(&launcher1));
  EXPECT_TRUE(prefs->HasObserver(&launcher3));

  ArcAppLauncher launcher2(profile(), id2, true);
  EXPECT_TRUE(launcher2.app_launched());
  app_instance()->WaitForIncomingMethodCall();
  EXPECT_FALSE(prefs->HasObserver(&launcher2));
  ASSERT_EQ(2u, app_instance()->launch_requests().size());
  EXPECT_TRUE(app_instance()->launch_requests()[1]->IsForApp(app2));
}
