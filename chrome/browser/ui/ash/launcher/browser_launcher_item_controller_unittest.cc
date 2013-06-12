// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/browser_launcher_item_controller.h"

#include <map>
#include <string>

#include "ash/ash_switches.h"
#include "ash/launcher/launcher_model.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/favicon/favicon_tab_helper.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/test_tab_strip_model_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/client/activation_change_observer.h"
#include "ui/aura/client/activation_delegate.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_activation_client.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/events/event.h"

// TODO(skuhne): Remove this module together with the
// browser_launcher_item_controller.* when the old launcher goes away.

namespace {

// Test implementation of AppTabHelper.
class AppTabHelperImpl : public ChromeLauncherController::AppTabHelper {
 public:
  AppTabHelperImpl() {}
  virtual ~AppTabHelperImpl() {}

  // Sets the id for the specified tab. The id is removed if Remove() is
  // invoked.
  void SetAppID(content::WebContents* tab, const std::string& id) {
    tab_id_map_[tab] = id;
  }

  // Returns true if there is an id registered for |tab|.
  bool HasAppID(content::WebContents* tab) const {
    return tab_id_map_.find(tab) != tab_id_map_.end();
  }

  // AppTabHelper implementation:
  virtual std::string GetAppID(content::WebContents* tab) OVERRIDE {
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

 private:
  typedef std::map<content::WebContents*, std::string> TabToStringMap;

  TabToStringMap tab_id_map_;

  DISALLOW_COPY_AND_ASSIGN(AppTabHelperImpl);
};

// Test implementation of AppIconLoader.
class AppIconLoaderImpl : public extensions::AppIconLoader {
 public:
  AppIconLoaderImpl() : fetch_count_(0) {}
  virtual ~AppIconLoaderImpl() {}

  // Returns the number of times FetchImage() has been invoked and resets the
  // count to 0.
  int GetAndClearFetchCount() {
    int value = fetch_count_;
    fetch_count_ = 0;
    return value;
  }

  // AppIconLoader implementation:
  virtual void FetchImage(const std::string& id) OVERRIDE {
    fetch_count_++;
  }
  virtual void ClearImage(const std::string& id) OVERRIDE {
  }
  virtual void UpdateImage(const std::string& id) OVERRIDE {
  }

 private:
  int fetch_count_;

  DISALLOW_COPY_AND_ASSIGN(AppIconLoaderImpl);
};

// Test implementation of TabStripModelDelegate.
class TabHelperTabStripModelDelegate : public TestTabStripModelDelegate {
 public:
  TabHelperTabStripModelDelegate() {}
  virtual ~TabHelperTabStripModelDelegate() {}

  virtual void WillAddWebContents(content::WebContents* contents) OVERRIDE {
    // BrowserLauncherItemController assumes that all WebContents passed to it
    // have attached an extensions::TabHelper and a FaviconTabHelper. The
    // TestTabStripModelDelegate adds an extensions::TabHelper.
    TestTabStripModelDelegate::WillAddWebContents(contents);
    FaviconTabHelper::CreateForWebContents(contents);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TabHelperTabStripModelDelegate);
};

}  // namespace

// TODO(skuhne): Several of these unit tests need to be moved into a new home
// when the old launcher & the browser launcher item controller are removed
// (several of these tests are not testing the BrowserLauncherItemController -
// but the LauncherController framework).
class LauncherItemControllerPerAppTest
    : public ChromeRenderViewHostTestHarness {
 public:
  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();

    activation_client_.reset(
        new aura::test::TestActivationClient(root_window()));
    launcher_model_.reset(new ash::LauncherModel);
    launcher_delegate_.reset(
        ChromeLauncherController::CreateInstance(profile(),
                                                 launcher_model_.get()));
    app_tab_helper_ = new AppTabHelperImpl;
    app_icon_loader_ = new AppIconLoaderImpl;
    launcher_delegate_->SetAppTabHelperForTest(app_tab_helper_);
    launcher_delegate_->SetAppIconLoaderForTest(app_icon_loader_);
    launcher_delegate_->Init();
  }

  virtual void TearDown() OVERRIDE {
    launcher_delegate_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  // Contains all the objects needed to create a BrowserLauncherItemController.
  struct State : public aura::client::ActivationDelegate,
                 public aura::client::ActivationChangeObserver {
   public:
    State(LauncherItemControllerPerAppTest* test,
          const std::string& app_id,
          BrowserLauncherItemController::Type launcher_type)
        : launcher_test(test),
          window(NULL),
          tab_strip(&tab_strip_delegate, test->profile()),
          updater(launcher_type,
                  &window,
                  &tab_strip,
                  test->launcher_delegate_.get(),
                  app_id) {
      window.Init(ui::LAYER_NOT_DRAWN);
      launcher_test->root_window()->AddChild(&window);
      launcher_test->activation_client_->ActivateWindow(&window);
      aura::client::SetActivationDelegate(&window, this);
      aura::client::SetActivationChangeObserver(&window, this);
      updater.Init();
    }

    ash::LauncherItem GetUpdaterItem() {
      ash::LauncherID launcher_id =
          BrowserLauncherItemController::TestApi(&updater).item_id();
      int index = launcher_test->launcher_model_->ItemIndexByID(launcher_id);
      return launcher_test->launcher_model_->items()[index];
    }

    // aura::client::ActivationDelegate overrides.
    virtual bool ShouldActivate() const OVERRIDE {
      return true;
    }

    // aura::client::ActivationChangeObserver overrides:
    virtual void OnWindowActivated(aura::Window* gained_active,
                                   aura::Window* lost_active) OVERRIDE {
      DCHECK(&window == gained_active || &window == lost_active);
      updater.BrowserActivationStateChanged();
    }

    LauncherItemControllerPerAppTest* launcher_test;
    aura::Window window;
    TabHelperTabStripModelDelegate tab_strip_delegate;
    TabStripModel tab_strip;
    BrowserLauncherItemController updater;

   private:
    DISALLOW_COPY_AND_ASSIGN(State);
  };

  const std::string& GetAppID(ash::LauncherID id) const {
    return launcher_delegate_->GetAppIdFromLauncherIdForTest(id);
  }

  void ResetAppTabHelper() {
    launcher_delegate_->SetAppTabHelperForTest(app_tab_helper_);
  }

  void ResetAppIconLoader() {
    launcher_delegate_->SetAppIconLoaderForTest(app_icon_loader_);
  }

  void UnpinAppsWithID(const std::string& app_id) {
    launcher_delegate_->UnpinAppsWithID(app_id);
  }

  const ash::LauncherItem& GetItem(BrowserLauncherItemController* updater) {
    int index = launcher_model_->ItemIndexByID(
        BrowserLauncherItemController::TestApi(updater).item_id());
    return launcher_model_->items()[index];
  }

  scoped_ptr<ash::LauncherModel> launcher_model_;
  scoped_ptr<ChromeLauncherController> launcher_delegate_;

  // Owned by BrowserLauncherItemController.
  AppTabHelperImpl* app_tab_helper_;
  AppIconLoaderImpl* app_icon_loader_;

  scoped_ptr<aura::test::TestActivationClient> activation_client_;
};

// Verify that the launcher item positions are persisted and restored.
TEST_F(LauncherItemControllerPerAppTest, PersistLauncherItemPositions) {
  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT,
            launcher_model_->items()[0].type);
  EXPECT_EQ(ash::TYPE_APP_LIST,
            launcher_model_->items()[1].type);
  scoped_ptr<content::WebContents> tab1(CreateTestWebContents());
  scoped_ptr<content::WebContents> tab2(CreateTestWebContents());
  app_tab_helper_->SetAppID(tab1.get(), "1");
  app_tab_helper_->SetAppID(tab1.get(), "2");

  EXPECT_FALSE(launcher_delegate_->IsAppPinned("1"));
  launcher_delegate_->PinAppWithID("1");
  EXPECT_TRUE(launcher_delegate_->IsAppPinned("1"));
  launcher_delegate_->PinAppWithID("2");

  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT,
            launcher_model_->items()[0].type);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT,
            launcher_model_->items()[1].type);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT,
            launcher_model_->items()[2].type);
  EXPECT_EQ(ash::TYPE_APP_LIST,
            launcher_model_->items()[3].type);

  launcher_model_->Move(0, 2);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT,
            launcher_model_->items()[0].type);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT,
            launcher_model_->items()[1].type);
  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT,
            launcher_model_->items()[2].type);
  EXPECT_EQ(ash::TYPE_APP_LIST,
            launcher_model_->items()[3].type);

  launcher_delegate_.reset();
  launcher_model_.reset(new ash::LauncherModel);
  launcher_delegate_.reset(
      ChromeLauncherController::CreateInstance(profile(),
                                               launcher_model_.get()));
  app_tab_helper_ = new AppTabHelperImpl;
  app_tab_helper_->SetAppID(tab1.get(), "1");
  app_tab_helper_->SetAppID(tab2.get(), "2");
  ResetAppTabHelper();

  launcher_delegate_->Init();

  EXPECT_EQ(ash::TYPE_APP_SHORTCUT,
            launcher_model_->items()[0].type);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT,
            launcher_model_->items()[1].type);
  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT,
            launcher_model_->items()[2].type);
  EXPECT_EQ(ash::TYPE_APP_LIST,
            launcher_model_->items()[3].type);
}

class BrowserLauncherItemControllerTest
    : public LauncherItemControllerPerAppTest {
 public:
  BrowserLauncherItemControllerTest() {}

  virtual void SetUp() OVERRIDE {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kAshDisablePerAppLauncher);

    LauncherItemControllerPerAppTest::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    LauncherItemControllerPerAppTest::TearDown();
  }
};

// Verifies a new launcher item is added for TYPE_TABBED.
TEST_F(BrowserLauncherItemControllerTest, TabbedSetup) {
  size_t initial_size = launcher_model_->items().size();
  {
    scoped_ptr<content::WebContents> web_contents(CreateTestWebContents());
    State state(this, std::string(),
                BrowserLauncherItemController::TYPE_TABBED);

    // There should be one more item.
    ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
    // New item should be added at the end.
    EXPECT_EQ(ash::TYPE_TABBED, state.GetUpdaterItem().type);
  }

  // Deleting the BrowserLauncherItemController should have removed the item.
  ASSERT_EQ(initial_size, launcher_model_->items().size());

  // Do the same, but this time add the tab first.
  {
    scoped_ptr<content::WebContents> web_contents(CreateTestWebContents());

    TabHelperTabStripModelDelegate tab_strip_delegate;
    TabStripModel tab_strip(&tab_strip_delegate, profile());
    tab_strip.InsertWebContentsAt(0,
                                  web_contents.get(),
                                  TabStripModel::ADD_ACTIVE);
    aura::Window window(NULL);
    window.Init(ui::LAYER_NOT_DRAWN);
    root_window()->AddChild(&window);
    BrowserLauncherItemController updater(
        LauncherItemController::TYPE_TABBED,
        &window, &tab_strip, launcher_delegate_.get(),
        std::string());
    updater.Init();

    // There should be one more item.
    ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
    // New item should be added at the end.
    EXPECT_EQ(ash::TYPE_TABBED, GetItem(&updater).type);
  }
}

// Verifies pinned apps are persisted and restored.
TEST_F(BrowserLauncherItemControllerTest, PersistPinned) {
  size_t initial_size = launcher_model_->items().size();
  scoped_ptr<content::WebContents> tab1(CreateTestWebContents());

  app_tab_helper_->SetAppID(tab1.get(), "1");

  app_icon_loader_->GetAndClearFetchCount();
  launcher_delegate_->PinAppWithID("1");
  ash::LauncherID id = launcher_delegate_->GetLauncherIDForAppID("1");
  int app_index = launcher_model_->ItemIndexByID(id);
  EXPECT_GT(app_icon_loader_->GetAndClearFetchCount(), 0);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT,
            launcher_model_->items()[app_index].type);
  EXPECT_TRUE(launcher_delegate_->IsAppPinned("1"));
  EXPECT_FALSE(launcher_delegate_->IsAppPinned("0"));
  EXPECT_EQ(initial_size + 1, launcher_model_->items().size());

  launcher_delegate_.reset();
  launcher_model_.reset(new ash::LauncherModel);
  launcher_delegate_.reset(
      ChromeLauncherController::CreateInstance(profile(),
                                               launcher_model_.get()));
  app_tab_helper_ = new AppTabHelperImpl;
  app_tab_helper_->SetAppID(tab1.get(), "1");
  ResetAppTabHelper();
  app_icon_loader_ = new AppIconLoaderImpl;
  ResetAppIconLoader();
  launcher_delegate_->Init();
  EXPECT_GT(app_icon_loader_->GetAndClearFetchCount(), 0);
  ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
  EXPECT_TRUE(launcher_delegate_->IsAppPinned("1"));
  EXPECT_FALSE(launcher_delegate_->IsAppPinned("0"));
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT,
            launcher_model_->items()[app_index].type);

  UnpinAppsWithID("1");
  ASSERT_EQ(initial_size, launcher_model_->items().size());
}

// Verify that launcher item positions are persisted and restored.
TEST_F(BrowserLauncherItemControllerTest,
    PersistLauncherItemPositionsPerBrowser) {
  int browser_shortcut_index = 0;
  int app_list_index = 1;

  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT,
            launcher_model_->items()[browser_shortcut_index].type);
  EXPECT_EQ(ash::TYPE_APP_LIST,
            launcher_model_->items()[app_list_index].type);

  scoped_ptr<content::WebContents> tab1(CreateTestWebContents());
  scoped_ptr<content::WebContents> tab2(CreateTestWebContents());

  app_tab_helper_->SetAppID(tab1.get(), "1");
  app_tab_helper_->SetAppID(tab2.get(), "2");

  app_icon_loader_->GetAndClearFetchCount();
  launcher_delegate_->PinAppWithID("1");
  ash::LauncherID id = launcher_delegate_->GetLauncherIDForAppID("1");
  int app1_index = launcher_model_->ItemIndexByID(id);

  launcher_delegate_->PinAppWithID("2");
  id = launcher_delegate_->GetLauncherIDForAppID("2");
  int app2_index = launcher_model_->ItemIndexByID(id);

  launcher_model_->Move(browser_shortcut_index, app1_index);

  browser_shortcut_index = 1;
  app1_index = 0;

  EXPECT_GT(app_icon_loader_->GetAndClearFetchCount(), 0);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT,
            launcher_model_->items()[app1_index].type);
  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT,
            launcher_model_->items()[browser_shortcut_index].type);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT,
            launcher_model_->items()[app2_index].type);

  launcher_delegate_.reset();
  launcher_model_.reset(new ash::LauncherModel);
  launcher_delegate_.reset(
      ChromeLauncherController::CreateInstance(profile(),
                                               launcher_model_.get()));

  app_tab_helper_ = new AppTabHelperImpl;
  app_tab_helper_->SetAppID(tab1.get(), "1");
  app_tab_helper_->SetAppID(tab2.get(), "2");
  ResetAppTabHelper();
  app_icon_loader_ = new AppIconLoaderImpl;
  ResetAppIconLoader();
  launcher_delegate_->Init();

  EXPECT_EQ(ash::TYPE_APP_SHORTCUT,
            launcher_model_->items()[app1_index].type);
  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT,
            launcher_model_->items()[browser_shortcut_index].type);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT,
            launcher_model_->items()[app2_index].type);
}

// Confirm that tabbed browsers handle activation correctly.
TEST_F(BrowserLauncherItemControllerTest, ActivateBrowsers) {
  State state1(this, std::string(), BrowserLauncherItemController::TYPE_TABBED);

  // First browser is active.
  EXPECT_EQ(ash::STATUS_ACTIVE, state1.GetUpdaterItem().status);

  {
    // Both running.
    State state2(this, std::string(),
                 BrowserLauncherItemController::TYPE_TABBED);
    EXPECT_EQ(ash::STATUS_ACTIVE, state2.GetUpdaterItem().status);
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

// Confirm that window activation works through the model.
TEST_F(BrowserLauncherItemControllerTest, SwitchDirectlyToApp) {
  State state1(this, std::string(),
               BrowserLauncherItemController::TYPE_TABBED);
  int index1 = launcher_model_->ItemIndexByID(state1.GetUpdaterItem().id);

  // Second app is active and first is inactive.
  State state2(this, std::string(),
               BrowserLauncherItemController::TYPE_TABBED);
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

// Test attention states of windows.
TEST_F(BrowserLauncherItemControllerTest, FlashWindow) {
  // App panel first
  State app_state(this, "1", BrowserLauncherItemController::TYPE_APP_PANEL);
  EXPECT_EQ(ash::STATUS_ACTIVE, app_state.GetUpdaterItem().status);

  // Active windows don't show attention.
  app_state.window.SetProperty(aura::client::kDrawAttentionKey, true);
  EXPECT_EQ(ash::STATUS_ACTIVE, app_state.GetUpdaterItem().status);

  // Then browser window
  State browser_state(
      this, std::string(), BrowserLauncherItemController::TYPE_TABBED);
  // First browser is active.
  EXPECT_EQ(ash::STATUS_ACTIVE, browser_state.GetUpdaterItem().status);
  EXPECT_EQ(ash::STATUS_RUNNING, app_state.GetUpdaterItem().status);

  // App window should go to attention state.
  app_state.window.SetProperty(aura::client::kDrawAttentionKey, true);
  EXPECT_EQ(ash::STATUS_ATTENTION, app_state.GetUpdaterItem().status);

  // Activating app window should clear attention state.
  activation_client_->ActivateWindow(&app_state.window);
  EXPECT_EQ(ash::STATUS_ACTIVE, app_state.GetUpdaterItem().status);
}
