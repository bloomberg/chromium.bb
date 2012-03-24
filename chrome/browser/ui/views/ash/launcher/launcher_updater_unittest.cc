// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/launcher/launcher_updater.h"

#include <map>
#include <string>

#include "ash/launcher/launcher_model.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/views/ash/launcher/chrome_launcher_delegate.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/client/activation_delegate.h"
#include "ui/aura/test/test_activation_client.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"

namespace {

// Test implementation of AppIconLoader.
class AppIconLoaderImpl : public ChromeLauncherDelegate::AppIconLoader {
 public:
  AppIconLoaderImpl() : fetch_count_(0) {}
  virtual ~AppIconLoaderImpl() {}

  // Sets the id for the specified tab. The id is removed if Remove() is
  // invoked.
  void SetAppID(TabContentsWrapper* tab, const std::string& id) {
    tab_id_map_[tab] = id;
  }

  // Returns true if there is an id registered for |tab|.
  bool HasAppID(TabContentsWrapper* tab) const {
    return tab_id_map_.find(tab) != tab_id_map_.end();
  }

  // Returns the number of times FetchImage() has been invoked and resets the
  // count to 0.
  int GetAndClearFetchCount() {
    int value = fetch_count_;
    fetch_count_ = 0;
    return value;
  }

  // AppIconLoader implementation:
  virtual std::string GetAppID(TabContentsWrapper* tab) OVERRIDE {
    return tab_id_map_.find(tab) != tab_id_map_.end() ? tab_id_map_[tab] :
        std::string();
  }

  virtual bool IsValidID(const std::string& id) OVERRIDE {
    for (TabToStringMap::const_iterator i = tab_id_map_.begin();
         i != tab_id_map_.end(); ++i) {
      if (i->second == id)
        return true;
    }
    return false;
  }

  virtual void FetchImage(const std::string& id) OVERRIDE {
    fetch_count_++;
  }

 private:
  typedef std::map<TabContentsWrapper*, std::string> TabToStringMap;

  TabToStringMap tab_id_map_;

  int fetch_count_;

  DISALLOW_COPY_AND_ASSIGN(AppIconLoaderImpl);
};

}  // namespace

class LauncherUpdaterTest : public ChromeRenderViewHostTestHarness {
 public:
  LauncherUpdaterTest()
      : browser_thread_(content::BrowserThread::UI, &message_loop_) {
  }

  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();

    activation_client_.reset(
        new aura::test::TestActivationClient(root_window()));
    launcher_model_.reset(new ash::LauncherModel);
    launcher_delegate_.reset(
        new ChromeLauncherDelegate(profile(), launcher_model_.get()));
    app_icon_loader_ = new AppIconLoaderImpl;
    launcher_delegate_->SetAppIconLoaderForTest(app_icon_loader_);
    launcher_delegate_->Init();
  }

 protected:
  // Contains all the objects needed to create a LauncherUpdater.
  struct State : public aura::client::ActivationDelegate {
   public:
    State(LauncherUpdaterTest* test,
          const std::string& app_id,
          LauncherUpdater::Type launcher_type)
        : launcher_test(test),
          window(NULL),
          tab_strip(&tab_strip_delegate, test->profile()),
          updater(&window,
                  &tab_strip,
                  test->launcher_delegate_.get(),
                  launcher_type,
                  app_id) {
      window.Init(ui::LAYER_NOT_DRAWN);
      launcher_test->root_window()->AddChild(&window);
      launcher_test->activation_client_->ActivateWindow(&window);
      updater.Init();
      aura::client::SetActivationDelegate(&window, this);
    }

    ash::LauncherItem GetUpdaterItem() {
      ash::LauncherID launcher_id =
          LauncherUpdater::TestApi(&updater).item_id();
      int index = launcher_test->launcher_model_->ItemIndexByID(launcher_id);
      return launcher_test->launcher_model_->items()[index];
    }

    // aura::client::ActivationDelegate overrides.
    virtual bool ShouldActivate(const aura::Event* event) OVERRIDE {
      return true;
    }
    virtual void OnActivated() OVERRIDE {
      updater.BrowserActivationStateChanged();
    }
    virtual void OnLostActive() OVERRIDE {
      updater.BrowserActivationStateChanged();
    }

    LauncherUpdaterTest* launcher_test;
    aura::Window window;
    TestTabStripModelDelegate tab_strip_delegate;
    TabStripModel tab_strip;
    LauncherUpdater updater;

   private:
    DISALLOW_COPY_AND_ASSIGN(State);
  };

  LauncherUpdater* GetUpdaterByID(ash::LauncherID id) const {
    return launcher_delegate_->id_to_item_map_[id].updater;
  }

  const std::string& GetAppID(ash::LauncherID id) const {
    return launcher_delegate_->id_to_item_map_[id].app_id;
  }

  void ResetAppIconLoader() {
    launcher_delegate_->SetAppIconLoaderForTest(app_icon_loader_);
  }

  void UnpinAppsWithID(const std::string& app_id) {
    launcher_delegate_->UnpinAppsWithID(app_id);
  }

  const ash::LauncherItem& GetItem(LauncherUpdater* updater) {
    int index = launcher_model_->ItemIndexByID(
        LauncherUpdater::TestApi(updater).item_id());
    return launcher_model_->items()[index];
  }

  scoped_ptr<ash::LauncherModel> launcher_model_;
  scoped_ptr<ChromeLauncherDelegate> launcher_delegate_;

  // Owned by LauncherUpdater.
  AppIconLoaderImpl* app_icon_loader_;

  scoped_ptr<aura::test::TestActivationClient> activation_client_;

 private:
  content::TestBrowserThread browser_thread_;
  std::vector<State*> states;

  DISALLOW_COPY_AND_ASSIGN(LauncherUpdaterTest);
};

// Verifies a new launcher item is added for TYPE_TABBED.
TEST_F(LauncherUpdaterTest, TabbedSetup) {
  size_t initial_size = launcher_model_->items().size();
  {
    TabContentsWrapper wrapper(CreateTestWebContents());
    State state(this, std::string(), LauncherUpdater::TYPE_TABBED);

    // There should be one more item.
    ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
    // New item should be added at the end.
    EXPECT_EQ(ash::TYPE_TABBED, state.GetUpdaterItem().type);
  }

  // Deleting the LauncherUpdater should have removed the item.
  ASSERT_EQ(initial_size, launcher_model_->items().size());

  // Do the same, but this time add the tab first.
  {
    TabContentsWrapper wrapper(CreateTestWebContents());

    TestTabStripModelDelegate tab_strip_delegate;
    TabStripModel tab_strip(&tab_strip_delegate, profile());
    tab_strip.InsertTabContentsAt(0, &wrapper, TabStripModel::ADD_ACTIVE);
    aura::Window window(NULL);
    window.Init(ui::LAYER_NOT_DRAWN);
    root_window()->AddChild(&window);
    LauncherUpdater updater(&window, &tab_strip, launcher_delegate_.get(),
                            LauncherUpdater::TYPE_TABBED, std::string());
    updater.Init();

    // There should be one more item.
    ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
    // New item should be added at the end.
    EXPECT_EQ(ash::TYPE_TABBED, GetItem(&updater).type);
  }
}

// Verifies a new launcher item is added for TYPE_APP.
TEST_F(LauncherUpdaterTest, AppSetup) {
  size_t initial_size = launcher_model_->items().size();
  {
    State state(this, std::string(), LauncherUpdater::TYPE_APP);
    // There should be one more item.
    ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
    // New item should be added at the end.
    EXPECT_EQ(ash::TYPE_APP, state.GetUpdaterItem().type);
  }
  // Deleting the LauncherUpdater should have removed the item.
  ASSERT_EQ(initial_size, launcher_model_->items().size());
}

// Verifies AppIconLoader is queried appropriately.
TEST_F(LauncherUpdaterTest, QueryAppIconLoader) {
  size_t initial_size = launcher_model_->items().size();
  {
    TabContentsWrapper initial_tab(CreateTestWebContents());
    State state(this, std::string(), LauncherUpdater::TYPE_APP);
    // AppIconLoader should have been queried.
    EXPECT_GT(app_icon_loader_->GetAndClearFetchCount(), 0);
  }
  // Deleting the LauncherUpdater should have removed the item.
  ASSERT_EQ(initial_size, launcher_model_->items().size());
}

// Verifies SetAppImage works.
TEST_F(LauncherUpdaterTest, SetAppImage) {
  size_t initial_size = launcher_model_->items().size();
  TabContentsWrapper initial_tab(CreateTestWebContents());
  State state(this, "1", LauncherUpdater::TYPE_APP);
  SkBitmap image;
  image.setConfig(SkBitmap::kARGB_8888_Config, 2, 3);
  image.allocPixels();
  launcher_delegate_->SetAppImage("1", &image);
  ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
  EXPECT_EQ(2, state.GetUpdaterItem().image.width());
  EXPECT_EQ(3, state.GetUpdaterItem().image.height());
}

// Verifies Panels items work.
TEST_F(LauncherUpdaterTest, PanelItem) {
  size_t initial_size = launcher_model_->items().size();

  // Add an App panel.
  {
    aura::Window window(NULL);
    TestTabStripModelDelegate tab_strip_delegate;
    TabStripModel tab_strip(&tab_strip_delegate, profile());
    TabContentsWrapper panel_tab(CreateTestWebContents());
    app_icon_loader_->SetAppID(&panel_tab, "1");  // Panels are apps.
    tab_strip.InsertTabContentsAt(0, &panel_tab, TabStripModel::ADD_ACTIVE);
    LauncherUpdater updater(&window, &tab_strip, launcher_delegate_.get(),
                            LauncherUpdater::TYPE_APP_PANEL, std::string());
    updater.Init();
    ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
    EXPECT_EQ(ash::TYPE_APP, GetItem(&updater).type);
    ash::LauncherID id = GetItem(&updater).id;
    EXPECT_EQ(ChromeLauncherDelegate::APP_TYPE_APP_PANEL,
              launcher_delegate_->GetAppType(id));
    EXPECT_EQ(static_cast<void*>(NULL), updater.favicon_loader_.get());
  }

  // Add an Extension panel.
  {
    aura::Window window(NULL);
    TestTabStripModelDelegate tab_strip_delegate;
    TabStripModel tab_strip(&tab_strip_delegate, profile());
    TabContentsWrapper panel_tab(CreateTestWebContents());
    app_icon_loader_->SetAppID(&panel_tab, "1");  // Panels are apps.
    tab_strip.InsertTabContentsAt(0, &panel_tab, TabStripModel::ADD_ACTIVE);
    LauncherUpdater updater(&window, &tab_strip, launcher_delegate_.get(),
                            LauncherUpdater::TYPE_EXTENSION_PANEL,
                            std::string());
    updater.Init();
    ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
    EXPECT_EQ(ash::TYPE_APP, GetItem(&updater).type);
    ash::LauncherID id = GetItem(&updater).id;
    EXPECT_EQ(ChromeLauncherDelegate::APP_TYPE_EXTENSION_PANEL,
              launcher_delegate_->GetAppType(id));
    EXPECT_NE(static_cast<void*>(NULL), updater.favicon_loader_.get());
  }
}

// Verifies pinned apps are persisted and restored.
TEST_F(LauncherUpdaterTest, PersistPinned) {
  size_t initial_size = launcher_model_->items().size();
  TabContentsWrapper tab1(CreateTestWebContents());

  app_icon_loader_->SetAppID(&tab1, "1");

  app_icon_loader_->GetAndClearFetchCount();
  launcher_delegate_->PinAppWithID("1",
                                   ChromeLauncherDelegate::APP_TYPE_WINDOW);
  EXPECT_GT(app_icon_loader_->GetAndClearFetchCount(), 0);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT,
            launcher_model_->items()[1].type);
  EXPECT_TRUE(launcher_delegate_->IsAppPinned("1"));
  EXPECT_FALSE(launcher_delegate_->IsAppPinned("0"));
  EXPECT_EQ(initial_size + 1, launcher_model_->items().size());

  launcher_delegate_.reset(
      new ChromeLauncherDelegate(profile(), launcher_model_.get()));
  app_icon_loader_ = new AppIconLoaderImpl;
  app_icon_loader_->SetAppID(&tab1, "1");
  ResetAppIconLoader();
  launcher_delegate_->Init();
  EXPECT_GT(app_icon_loader_->GetAndClearFetchCount(), 0);
  ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
  EXPECT_TRUE(launcher_delegate_->IsAppPinned("1"));
  EXPECT_FALSE(launcher_delegate_->IsAppPinned("0"));
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT,
            launcher_model_->items()[1].type);

  UnpinAppsWithID("1");
  ASSERT_EQ(initial_size, launcher_model_->items().size());
}

// Confirm that tabbed browsers handle activation correctly.
TEST_F(LauncherUpdaterTest, ActivateBrowsers) {
  State state1(this, std::string(), LauncherUpdater::TYPE_TABBED);

  // First browser is running.
  EXPECT_EQ(ash::STATUS_RUNNING, state1.GetUpdaterItem().status);

  {
    // Both running.
    State state2(this, std::string(), LauncherUpdater::TYPE_TABBED);
    EXPECT_EQ(ash::STATUS_RUNNING, state2.GetUpdaterItem().status);
    EXPECT_EQ(ash::STATUS_RUNNING, state1.GetUpdaterItem().status);

    // Make first browser active again.
    activation_client_->ActivateWindow(&state1.window);
    EXPECT_EQ(ash::STATUS_ACTIVE, state1.GetUpdaterItem().status);
    EXPECT_EQ(ash::STATUS_RUNNING, state2.GetUpdaterItem().status);

    // And back to second.
    activation_client_->ActivateWindow(&state2.window);
    EXPECT_EQ(ash::STATUS_ACTIVE, state2.GetUpdaterItem().status);
    EXPECT_EQ(ash::STATUS_RUNNING, state1.GetUpdaterItem().status);
  }

  // First browser should be active again after second is closed.
  EXPECT_EQ(ash::STATUS_ACTIVE, state1.GetUpdaterItem().status);
}

// Confirm that applications  handle activation correctly.
TEST_F(LauncherUpdaterTest, ActivateApps) {
  State state1(this, "1", LauncherUpdater::TYPE_APP);

  // First app is active.
  EXPECT_EQ(ash::STATUS_ACTIVE, state1.GetUpdaterItem().status);

  {
    // Second app is active and first is inactive.
    State state2(this, "2", LauncherUpdater::TYPE_APP);
    EXPECT_EQ(ash::STATUS_ACTIVE, state2.GetUpdaterItem().status);
    EXPECT_EQ(ash::STATUS_RUNNING, state1.GetUpdaterItem().status);

    // Make first app active again.
    activation_client_->ActivateWindow(&state1.window);
    EXPECT_EQ(ash::STATUS_ACTIVE, state1.GetUpdaterItem().status);
    EXPECT_EQ(ash::STATUS_RUNNING, state2.GetUpdaterItem().status);

    // And back to second.
    activation_client_->ActivateWindow(&state2.window);
    EXPECT_EQ(ash::STATUS_ACTIVE, state2.GetUpdaterItem().status);
    EXPECT_EQ(ash::STATUS_RUNNING, state1.GetUpdaterItem().status);
  }

  // First app should be active again after second is closed.
  EXPECT_EQ(ash::STATUS_ACTIVE, state1.GetUpdaterItem().status);
}

// Confirm that window activation works through the model.
TEST_F(LauncherUpdaterTest, SwitchDirectlyToApp) {
  State state1(this, "1", LauncherUpdater::TYPE_APP);
  int index1 = launcher_model_->ItemIndexByID(state1.GetUpdaterItem().id);

  // Second app is active and first is inactive.
  State state2(this, "2", LauncherUpdater::TYPE_APP);
  int index2 = launcher_model_->ItemIndexByID(state2.GetUpdaterItem().id);

  EXPECT_EQ(ash::STATUS_RUNNING, state1.GetUpdaterItem().status);
  EXPECT_EQ(ash::STATUS_ACTIVE, state2.GetUpdaterItem().status);
  EXPECT_EQ(&state2.window, activation_client_->GetActiveWindow());

  // Test that we can properly switch to the first item.
  ash::LauncherItem new_item1(launcher_model_->items()[index1]);
  new_item1.status = ash::STATUS_ACTIVE;
  launcher_model_->Set(index1, new_item1);
  EXPECT_EQ(ash::STATUS_ACTIVE, launcher_model_->items()[index1].status);
  EXPECT_EQ(ash::STATUS_RUNNING, launcher_model_->items()[index2].status);
  EXPECT_EQ(&state1.window, activation_client_->GetActiveWindow());

  // And to the second item active.
  ash::LauncherItem new_item2(launcher_model_->items()[index2]);
  new_item2.status = ash::STATUS_ACTIVE;
  launcher_model_->Set(index2, new_item2);
  EXPECT_EQ(ash::STATUS_RUNNING, launcher_model_->items()[index1].status);
  EXPECT_EQ(ash::STATUS_ACTIVE, launcher_model_->items()[index2].status);
  EXPECT_EQ(&state2.window, activation_client_->GetActiveWindow());
}
