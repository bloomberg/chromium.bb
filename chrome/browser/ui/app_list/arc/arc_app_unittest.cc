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
#include "base/strings/string_util.h"
#include "base/task_runner_util.h"
#include "chrome/browser/ui/app_list/app_list_test_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon.h"
#include "chrome/browser/ui/app_list/arc/arc_app_item.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_model_builder.h"
#include "chrome/browser/ui/app_list/test/test_app_list_controller_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/test/fake_app_instance.h"
#include "components/arc/test/fake_arc_bridge_service.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/app_list_model.h"
#include "ui/gfx/image/image_skia.h"

namespace {

std::string GetAppId(const arc::AppInfo& app_info) {
  return ArcAppListPrefs::GetAppId(app_info.package_name, app_info.activity);
}

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

    // Make sure we have enough data for test.
    for (int i = 0; i < 3; ++i) {
      arc::AppInfo app;
      char buffer[16];
      base::snprintf(buffer, arraysize(buffer), "Fake App %d", i);
      app.name = buffer;
      base::snprintf(buffer, arraysize(buffer), "fake.app.%d", i);
      app.package_name = buffer;
      base::snprintf(buffer, arraysize(buffer), "fake.app.%d.activity", i);
      app.activity = buffer;
      fake_apps_.push_back(app);
    }

    bridge_service_.reset(new arc::FakeArcBridgeService());
    app_instance_.reset(
        new arc::FakeAppInstance(ArcAppListPrefs::Get(profile_.get())));
    arc::AppInstancePtr instance;
    app_instance_->Bind(mojo::GetProxy(&instance));
    bridge_service_->OnAppInstanceReady(std::move(instance));
    app_instance_->WaitForOnAppInstanceReady();

    // Check initial conditions.
    EXPECT_EQ(bridge_service_.get(), arc::ArcBridgeService::Get());
    EXPECT_EQ(true, !arc::ArcBridgeService::Get()->available());
    EXPECT_EQ(arc::ArcBridgeService::State::STOPPED,
              arc::ArcBridgeService::Get()->state());

    CreateBuilder();

    // At this point we should have ArcAppListPrefs as observer of service.
    EXPECT_EQ(
        true,
        bridge_service()->HasObserver(ArcAppListPrefs::Get(profile_.get())));
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
  void ValidateHaveApps(const std::vector<arc::AppInfo> apps) {
    ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
    const std::vector<std::string> ids = prefs->GetAppIds();
    ASSERT_EQ(apps.size(), ids.size());
    ASSERT_EQ(apps.size(), GetArcItemCount());
    // In principle, order of items is not defined.
    for (auto& app : apps) {
      const std::string id = GetAppId(app);
      EXPECT_NE(std::find(ids.begin(), ids.end(), id), ids.end());
      scoped_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(id);
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
  void ValidateAppReadyState(const std::vector<arc::AppInfo> apps, bool ready) {
    ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
    ASSERT_NE(nullptr, prefs);

    std::vector<std::string> ids = prefs->GetAppIds();
    EXPECT_EQ(ids.size(), GetArcItemCount());

    // Process requested apps.
    for (auto& app : apps) {
      const std::string id = GetAppId(app);
      std::vector<std::string>::iterator it_id = std::find(ids.begin(),
                                                           ids.end(),
                                                           id);
      ASSERT_NE(it_id, ids.end());
      ids.erase(it_id);

      scoped_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(id);
      ASSERT_NE(nullptr, app_info.get());
      EXPECT_EQ(ready, app_info->ready);
      const ArcAppItem* app_item = FindArcItem(id);
      ASSERT_NE(nullptr, app_item);
      EXPECT_EQ(ready, app_item->ready());
    }

    // Process the rest of the apps.
    for (auto& id : ids) {
      scoped_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(id);
      ASSERT_NE(nullptr, app_info.get());
      EXPECT_NE(ready, app_info->ready);
      const ArcAppItem* app_item = FindArcItem(id);
      ASSERT_NE(nullptr, app_item);
      EXPECT_NE(ready, app_item->ready());
    }
  }

  AppListControllerDelegate* controller() { return controller_.get(); }

  arc::FakeArcBridgeService* bridge_service() { return bridge_service_.get(); }

  arc::FakeAppInstance* app_instance() { return app_instance_.get(); }

  const std::vector<arc::AppInfo>& fake_apps() const { return fake_apps_; }

 private:
  scoped_ptr<app_list::AppListModel> model_;
  scoped_ptr<test::TestAppListControllerDelegate> controller_;
  scoped_ptr<ArcAppModelBuilder> builder_;
  scoped_ptr<arc::FakeArcBridgeService> bridge_service_;
  scoped_ptr<arc::FakeAppInstance> app_instance_;
  std::vector<arc::AppInfo> fake_apps_;

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
  ValidateHaveApps(std::vector<arc::AppInfo>());
  bridge_service()->SetReady();
  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(fake_apps());
  ValidateHaveApps(fake_apps());
}

TEST_F(ArcAppModelBuilderTest, MultipleRefreshAll) {
  ValidateHaveApps(std::vector<arc::AppInfo>());
  bridge_service()->SetReady();
  app_instance()->RefreshAppList();
  // Send info about all fake apps except last.
  std::vector<arc::AppInfo> apps1(fake_apps().begin(), fake_apps().end() - 1);
  app_instance()->SendRefreshAppList(apps1);
  // At this point all apps (except last) should exist and be ready.
  ValidateHaveApps(apps1);
  ValidateAppReadyState(apps1, true);

  // Send info about all fake apps except first.
  std::vector<arc::AppInfo> apps2(fake_apps().begin() + 1, fake_apps().end());
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
  std::vector<arc::AppInfo> no_apps;
  app_instance()->SendRefreshAppList(no_apps);
  // At this point no app should exist.
  ValidateHaveApps(no_apps);
}

TEST_F(ArcAppModelBuilderTest, StopStartServicePreserveApps) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  bridge_service()->SetReady();
  app_instance()->RefreshAppList();
  EXPECT_EQ(static_cast<size_t>(0), GetArcItemCount());
  EXPECT_EQ(static_cast<size_t>(0), prefs->GetAppIds().size());

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
  const arc::AppInfo& app_first = fake_apps()[0];
  const arc::AppInfo& app_last = fake_apps()[0];
  ArcAppItem* item_first = FindArcItem(GetAppId(app_first));
  ArcAppItem* item_last = FindArcItem(GetAppId(app_last));
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
  ASSERT_EQ(static_cast<size_t>(3), launch_requests.size());
  EXPECT_EQ(true, launch_requests[0]->IsForApp(app_first));
  EXPECT_EQ(true, launch_requests[1]->IsForApp(app_last));
  EXPECT_EQ(true, launch_requests[2]->IsForApp(app_first));

  // Test an attempt to launch of a not-ready app.
  bridge_service()->SetStopped();
  item_first = FindArcItem(GetAppId(app_first));
  ASSERT_NE(nullptr, item_first);
  size_t launch_request_count_before = app_instance()->launch_requests().size();
  item_first->Activate(0);
  // Number of launch requests must not change.
  EXPECT_EQ(launch_request_count_before,
            app_instance()->launch_requests().size());
}

TEST_F(ArcAppModelBuilderTest, RequestIcons) {
  // Make sure we are on UI thread.
  ASSERT_EQ(true,
            content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

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
      ArcAppItem* app_item = FindArcItem(GetAppId(app));
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
    const std::string id = GetAppId(app);
    ASSERT_NE(app_masks.find(id), app_masks.end());
    EXPECT_EQ(app_masks[id], expected_mask);
  }
}

TEST_F(ArcAppModelBuilderTest, InstallIcon) {
  // Make sure we are on UI thread.
  ASSERT_EQ(true,
            content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));


  bridge_service()->SetReady();
  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(
      std::vector<arc::AppInfo>(fake_apps().begin(), fake_apps().begin() + 1));
  const arc::AppInfo& app = fake_apps()[0];

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  const ui::ScaleFactor scale_factor = ui::GetSupportedScaleFactors()[0];
  const float scale = ui::GetScaleForScaleFactor(scale_factor);
  const base::FilePath icon_path = prefs->GetIconPath(GetAppId(app),
                                                      scale_factor);
  EXPECT_EQ(true, !base::PathExists(icon_path));

  const ArcAppItem* app_item = FindArcItem(GetAppId(app));
  EXPECT_NE(nullptr, app_item);
  // This initiates async loading.
  app_item->icon().GetRepresentation(scale);

  // Process pending tasks.
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  base::RunLoop().RunUntilIdle();

  // Validating decoded content does not fit well for unit tests.
  ArcAppIcon::DisableDecodingForTesting();

  // Now send generated icon for the app.
  std::string png_data;
  EXPECT_EQ(true,
            app_instance()->GenerateAndSendIcon(
                app, static_cast<arc::ScaleFactor>(scale_factor), &png_data));

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
  ASSERT_EQ(true,
            content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  bridge_service()->SetReady();
  app_instance()->RefreshAppList();
  app_instance()->SendRefreshAppList(
      std::vector<arc::AppInfo>(fake_apps().begin(), fake_apps().begin() + 1));
  const arc::AppInfo& app = fake_apps()[0];

  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_.get());
  ASSERT_NE(nullptr, prefs);

  const std::string app_id = GetAppId(app);
  const base::FilePath app_path = prefs->GetAppPath(app_id);
  const ui::ScaleFactor scale_factor = ui::GetSupportedScaleFactors()[0];

  // No app folder by default.
  EXPECT_EQ(true, !base::PathExists(app_path));

  // Request icon, this will create app folder.
  prefs->RequestIcon(app_id, scale_factor);
  // Now send generated icon for the app.
  std::string png_data;
  EXPECT_EQ(true,
            app_instance()->GenerateAndSendIcon(
                app, static_cast<arc::ScaleFactor>(scale_factor), &png_data));
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(true, base::PathExists(app_path));

  // Send empty app list. This will delete app and its folder.
  app_instance()->SendRefreshAppList(std::vector<arc::AppInfo>());
  content::BrowserThread::GetBlockingPool()->FlushForTesting();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(true, !base::PathExists(app_path));
}
