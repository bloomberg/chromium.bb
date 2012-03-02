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
#include "content/browser/tab_contents/test_tab_contents.h"
#include "content/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/aura/window.h"

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

// Contains all the objects needed to create a LauncherUpdater.
struct State {
  State(Profile* profile,
        ChromeLauncherDelegate* launcher_delegate,
        const std::string& app_id,
        LauncherUpdater::Type launcher_type)
      : window(NULL),
        tab_strip(&tab_strip_delegate, profile),
        updater(&window, &tab_strip, launcher_delegate, launcher_type,
                app_id) {
    updater.Init();
  }

  aura::Window window;
  TestTabStripModelDelegate tab_strip_delegate;
  TabStripModel tab_strip;
  LauncherUpdater updater;

 private:
  DISALLOW_COPY_AND_ASSIGN(State);
};

}  // namespace

class LauncherUpdaterTest : public ChromeRenderViewHostTestHarness {
 public:
  LauncherUpdaterTest()
      : browser_thread_(content::BrowserThread::UI, &message_loop_) {
  }

  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();
    launcher_model_.reset(new ash::LauncherModel);
    launcher_delegate_.reset(
        new ChromeLauncherDelegate(profile(), launcher_model_.get()));
    app_icon_loader_ = new AppIconLoaderImpl;
    launcher_delegate_->SetAppIconLoaderForTest(app_icon_loader_);
    launcher_delegate_->Init();
  }

 protected:
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

  scoped_ptr<ash::LauncherModel> launcher_model_;
  scoped_ptr<ChromeLauncherDelegate> launcher_delegate_;

  // Owned by LauncherUpdater.
  AppIconLoaderImpl* app_icon_loader_;

 private:
  content::TestBrowserThread browser_thread_;

  DISALLOW_COPY_AND_ASSIGN(LauncherUpdaterTest);
};

// Verifies a new launcher item is added for TYPE_TABBED.
TEST_F(LauncherUpdaterTest, TabbedSetup) {
  size_t initial_size = launcher_model_->items().size();
  {
    TabContentsWrapper wrapper(CreateTestTabContents());
    State state(profile(), launcher_delegate_.get(), std::string(),
                LauncherUpdater::TYPE_TABBED);
    // Since the type is tabbed and there is nothing in the tabstrip an item
    // should not have been added.
    EXPECT_EQ(initial_size, launcher_model_->items().size());

    // Add a tab.
    state.tab_strip.InsertTabContentsAt(0, &wrapper, TabStripModel::ADD_NONE);

    // There should be one more item.
    ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
    // New item should be added at the end.
    EXPECT_EQ(ash::TYPE_TABBED, launcher_model_->items()[initial_size].type);
  }
  // Deleting the LauncherUpdater should have removed the item.
  ASSERT_EQ(initial_size, launcher_model_->items().size());

  // Do the same, but this time add the tab first.
  {
    TabContentsWrapper wrapper(CreateTestTabContents());

    TestTabStripModelDelegate tab_strip_delegate;
    TabStripModel tab_strip(&tab_strip_delegate, profile());
    tab_strip.InsertTabContentsAt(0, &wrapper, TabStripModel::ADD_NONE);
    LauncherUpdater updater(NULL, &tab_strip, launcher_delegate_.get(),
                            LauncherUpdater::TYPE_TABBED, std::string());
    updater.Init();

    // There should be one more item.
    ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
    // New item should be added at the end.
    EXPECT_EQ(ash::TYPE_TABBED, launcher_model_->items()[initial_size].type);
  }
}

// Verifies a new launcher item is added for TYPE_APP.
TEST_F(LauncherUpdaterTest, AppSetup) {
  size_t initial_size = launcher_model_->items().size();
  {
    State state(profile(), launcher_delegate_.get(), std::string(),
                LauncherUpdater::TYPE_APP);
    // There should be one more item.
    ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
    // New item should be added at the end.
    EXPECT_EQ(ash::TYPE_APP, launcher_model_->items()[initial_size].type);
  }
  // Deleting the LauncherUpdater should have removed the item.
  ASSERT_EQ(initial_size, launcher_model_->items().size());
}

// Various assertions when adding/removing a tab that has an app associated with
// it.
TEST_F(LauncherUpdaterTest, TabbedWithApp) {
  size_t initial_size = launcher_model_->items().size();
  {
    TabContentsWrapper initial_tab(CreateTestTabContents());
    State state(profile(), launcher_delegate_.get(), std::string(),
                LauncherUpdater::TYPE_TABBED);
    // Add a tab.
    state.tab_strip.InsertTabContentsAt(0, &initial_tab,
                                        TabStripModel::ADD_NONE);

    // There should be one more item.
    ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
    // New item should be added at the end.
    EXPECT_EQ(ash::TYPE_TABBED, launcher_model_->items()[initial_size].type);
    ash::LauncherID tabbed_id = launcher_model_->items()[initial_size].id;

    // Add another tab, configure it so that the launcher thinks it's an app.
    TabContentsWrapper app_tab(CreateTestTabContents());
    app_icon_loader_->SetAppID(&app_tab, "1");
    state.tab_strip.InsertTabContentsAt(1, &app_tab, TabStripModel::ADD_NONE);

    // There should be one more item.
    ASSERT_EQ(initial_size + 2, launcher_model_->items().size());
    // New item should be added at the end.
    EXPECT_EQ(ash::TYPE_APP, launcher_model_->items()[initial_size + 1].type);

    // Remove the first tab, this should trigger removing the tabbed item.
    state.tab_strip.DetachTabContentsAt(0);
    ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
    EXPECT_EQ(ash::TYPE_APP, launcher_model_->items()[initial_size + 1].type);
    EXPECT_EQ(-1, launcher_model_->ItemIndexByID(tabbed_id));

    // Add back the tab, which triggers creating the tabbed item.
    state.tab_strip.InsertTabContentsAt(0, &initial_tab,
                                        TabStripModel::ADD_NONE);
    ASSERT_EQ(initial_size + 2, launcher_model_->items().size());
    EXPECT_EQ(ash::TYPE_TABBED,
              launcher_model_->items()[initial_size].type);

  }
  // Deleting the LauncherUpdater should have removed the item.
  ASSERT_EQ(initial_size, launcher_model_->items().size());
}

TEST_F(LauncherUpdaterTest, TabbedWithAppOnCreate) {
  size_t initial_size = launcher_model_->items().size();
  aura::Window window(NULL);
  TestTabStripModelDelegate tab_strip_delegate;
  TabStripModel tab_strip(&tab_strip_delegate, profile());
  TabContentsWrapper app_tab(CreateTestTabContents());
  app_icon_loader_->SetAppID(&app_tab, "1");
  tab_strip.InsertTabContentsAt(0, &app_tab, TabStripModel::ADD_NONE);
  LauncherUpdater updater(&window, &tab_strip, launcher_delegate_.get(),
                          LauncherUpdater::TYPE_TABBED, std::string());
  updater.Init();
  ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
  EXPECT_EQ(ash::TYPE_APP, launcher_model_->items()[initial_size].type);
}

// Verifies transitioning from a normal tab to app tab and back works.
TEST_F(LauncherUpdaterTest, ChangeToApp) {
  size_t initial_size = launcher_model_->items().size();
  {
    TabContentsWrapper initial_tab(CreateTestTabContents());
    State state(profile(), launcher_delegate_.get(), std::string(),
                LauncherUpdater::TYPE_TABBED);
    // Add a tab.
    state.tab_strip.InsertTabContentsAt(0, &initial_tab,
                                        TabStripModel::ADD_NONE);

    // There should be one more item.
    ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
    // New item should be added at the end.
    EXPECT_EQ(ash::TYPE_TABBED, launcher_model_->items()[initial_size].type);
    ash::LauncherID tabbed_id = launcher_model_->items()[initial_size].id;

    app_icon_loader_->SetAppID(&initial_tab, "1");
    // Triggers LauncherUpdater seeing the tab changed to an app.
    state.updater.TabChangedAt(&initial_tab, 0, TabStripModelObserver::ALL);
    ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
    EXPECT_EQ(ash::TYPE_APP, launcher_model_->items()[initial_size].type);
    EXPECT_EQ(tabbed_id, launcher_model_->items()[initial_size].id);

    // Change back to a non-app and make sure the tabbed item is added back.
    app_icon_loader_->SetAppID(&initial_tab, std::string());
    state.updater.TabChangedAt(&initial_tab, 0, TabStripModelObserver::ALL);
    ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
    EXPECT_EQ(ash::TYPE_TABBED, launcher_model_->items()[initial_size].type);
    EXPECT_EQ(tabbed_id, launcher_model_->items()[initial_size].id);
  }
  // Deleting the LauncherUpdater should have removed the item.
  ASSERT_EQ(initial_size, launcher_model_->items().size());
}

// Verifies AppIconLoader is queried appropriately.
TEST_F(LauncherUpdaterTest, QueryAppIconLoader) {
  size_t initial_size = launcher_model_->items().size();
  {
    TabContentsWrapper initial_tab(CreateTestTabContents());
    State state(profile(), launcher_delegate_.get(), std::string(),
                LauncherUpdater::TYPE_TABBED);
    // Configure the tab as an app.
    app_icon_loader_->SetAppID(&initial_tab, "1");
    // Add a tab.
    state.tab_strip.InsertTabContentsAt(0, &initial_tab,
                                        TabStripModel::ADD_NONE);
    // AppIconLoader should have been queried.
    EXPECT_GT(app_icon_loader_->GetAndClearFetchCount(), 0);
    // Remove the tab.
    state.tab_strip.DetachTabContentsAt(0);
  }
  // Deleting the LauncherUpdater should have removed the item.
  ASSERT_EQ(initial_size, launcher_model_->items().size());
}

// Verifies SetAppImage works.
TEST_F(LauncherUpdaterTest, SetAppImage) {
  size_t initial_size = launcher_model_->items().size();
  TabContentsWrapper initial_tab(CreateTestTabContents());
  State state(profile(), launcher_delegate_.get(), std::string(),
              LauncherUpdater::TYPE_TABBED);
  // Configure the tab as an app.
  app_icon_loader_->SetAppID(&initial_tab, "1");
  // Add a tab.
  state.tab_strip.InsertTabContentsAt(0, &initial_tab,
                                      TabStripModel::ADD_NONE);
  SkBitmap image;
  image.setConfig(SkBitmap::kARGB_8888_Config, 2, 3);
  image.allocPixels();
  launcher_delegate_->SetAppImage("1", &image);
  ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
  EXPECT_EQ(2, launcher_model_->items()[initial_size].image.width());
  EXPECT_EQ(3, launcher_model_->items()[initial_size].image.height());
}

// Verifies app tabs are added right after the existing tabbed item.
TEST_F(LauncherUpdaterTest, AddAppAfterTabbed) {
  size_t initial_size = launcher_model_->items().size();
  TabContentsWrapper tab1(CreateTestTabContents());
  State state1(profile(), launcher_delegate_.get(), std::string(),
               LauncherUpdater::TYPE_TABBED);
  // Add a tab.
  state1.tab_strip.InsertTabContentsAt(0, &tab1, TabStripModel::ADD_NONE);
  ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
  ash::LauncherID tabbed_id = launcher_model_->items()[initial_size].id;

  // Create another LauncherUpdater.
  State state2(profile(), launcher_delegate_.get(), std::string(),
               LauncherUpdater::TYPE_APP);

  // Should be two extra items.
  EXPECT_EQ(initial_size + 2, launcher_model_->items().size());

  // Add an app tab to state1, it should go after the item for state1 but
  // before the item for state2.
  int next_id = launcher_model_->next_id();
  TabContentsWrapper app_tab(CreateTestTabContents());
  app_icon_loader_->SetAppID(&app_tab, "1");
  state1.tab_strip.InsertTabContentsAt(1, &app_tab, TabStripModel::ADD_NONE);

  ASSERT_EQ(initial_size + 3, launcher_model_->items().size());
  EXPECT_EQ(next_id, launcher_model_->items()[initial_size + 1].id);
  EXPECT_EQ(ash::TYPE_APP, launcher_model_->items()[initial_size + 1].type);

  // Remove the non-app tab.
  state1.tab_strip.DetachTabContentsAt(0);
  // Should have removed one item.
  EXPECT_EQ(initial_size + 2, launcher_model_->items().size());
  EXPECT_EQ(-1, launcher_model_->ItemIndexByID(tabbed_id));
  next_id = launcher_model_->next_id();
  // Add the non-app tab back. It should go to the original position (but get a
  // new id).
  state1.tab_strip.InsertTabContentsAt(0, &tab1, TabStripModel::ADD_NONE);
  ASSERT_EQ(initial_size + 3, launcher_model_->items().size());
  EXPECT_EQ(next_id, launcher_model_->items()[initial_size].id);
}

// Verifies GetWindowAndTabByID works.
TEST_F(LauncherUpdaterTest, GetUpdaterByID) {
  size_t initial_size = launcher_model_->items().size();
  TabContentsWrapper tab1(CreateTestTabContents());
  TabContentsWrapper tab2(CreateTestTabContents());
  TabContentsWrapper tab3(CreateTestTabContents());

  // Create 3 states:
  // . tabbed with an app tab and normal tab.
  // . tabbed with a normal tab.
  // . app.
  State state1(profile(), launcher_delegate_.get(), std::string(),
               LauncherUpdater::TYPE_TABBED);
  state1.tab_strip.InsertTabContentsAt(0, &tab1, TabStripModel::ADD_NONE);
  app_icon_loader_->SetAppID(&tab2, "1");
  state1.tab_strip.InsertTabContentsAt(0, &tab2, TabStripModel::ADD_NONE);
  State state2(profile(), launcher_delegate_.get(), std::string(),
               LauncherUpdater::TYPE_TABBED);
  state2.tab_strip.InsertTabContentsAt(0, &tab3, TabStripModel::ADD_NONE);
  State state3(profile(), launcher_delegate_.get(), std::string(),
               LauncherUpdater::TYPE_APP);
  ASSERT_EQ(initial_size + 4, launcher_model_->items().size());

  // Tabbed item from first state.
  ash::LauncherID id = launcher_model_->items()[initial_size].id;
  LauncherUpdater* updater = GetUpdaterByID(id);
  EXPECT_EQ(&(state1.updater), updater);
  ASSERT_TRUE(updater);
  EXPECT_TRUE(updater->GetTab(id) == NULL);

  // App item from first state.
  id = launcher_model_->items()[initial_size + 1].id;
  updater = GetUpdaterByID(id);
  EXPECT_EQ(&(state1.updater), updater);
  ASSERT_TRUE(updater);
  EXPECT_TRUE(updater->GetTab(id) == &tab2);

  // Tabbed item from second state.
  id = launcher_model_->items()[initial_size + 2].id;
  updater = GetUpdaterByID(id);
  EXPECT_EQ(&(state2.updater), updater);
  ASSERT_TRUE(updater);
  EXPECT_TRUE(updater->GetTab(id) == NULL);

  // App item.
  id = launcher_model_->items()[initial_size + 3].id;
  updater = GetUpdaterByID(id);
  EXPECT_EQ(&(state3.updater), updater);
  ASSERT_TRUE(updater);
  EXPECT_TRUE(updater->GetTab(id) == NULL);
}

// Various assertions around pinning. In particular verifies destroying a
// LauncherUpdater doesn't remove the entry for a pinned app.
TEST_F(LauncherUpdaterTest, Pin) {
  size_t initial_size = launcher_model_->items().size();
  TabContentsWrapper tab1(CreateTestTabContents());
  TabContentsWrapper tab2(CreateTestTabContents());
  TabContentsWrapper tab3(CreateTestTabContents());

  ash::LauncherID id;
  {
    State state1(profile(), launcher_delegate_.get(), std::string(),
                 LauncherUpdater::TYPE_TABBED);
    app_icon_loader_->SetAppID(&tab1, "1");
    state1.tab_strip.InsertTabContentsAt(0, &tab1, TabStripModel::ADD_NONE);
    ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
    id = launcher_model_->items()[initial_size].id;
    EXPECT_EQ(ash::TYPE_APP, launcher_model_->items()[initial_size].type);
    // Shouldn't be pinned.
    EXPECT_FALSE(launcher_delegate_->IsPinned(id));
    launcher_delegate_->Pin(id);
    EXPECT_TRUE(launcher_delegate_->IsPinned(id));
  }

  // Should still have the item.
  ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
  EXPECT_TRUE(launcher_delegate_->IsPinned(id));
  EXPECT_TRUE(GetUpdaterByID(id) == NULL);

  // Create another app tab, it shouldn't get the same id.
  {
    State state1(profile(), launcher_delegate_.get(), std::string(),
                 LauncherUpdater::TYPE_TABBED);
    app_icon_loader_->SetAppID(&tab2, "2");
    state1.tab_strip.InsertTabContentsAt(0, &tab2, TabStripModel::ADD_NONE);
    ASSERT_EQ(initial_size + 2, launcher_model_->items().size());
    ash::LauncherID new_id = launcher_model_->items()[initial_size + 1].id;
    EXPECT_NE(id, new_id);
    EXPECT_EQ(ash::TYPE_APP, launcher_model_->items()[initial_size + 1].type);
    // Shouldn't be pinned.
    EXPECT_FALSE(launcher_delegate_->IsPinned(new_id));
    // But existing one should still be pinned.
    EXPECT_TRUE(launcher_delegate_->IsPinned(id));
  }

  // Add it back and make sure we don't get another entry.
  ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
  {
    State state1(profile(), launcher_delegate_.get(), std::string(),
                 LauncherUpdater::TYPE_TABBED);
    app_icon_loader_->SetAppID(&tab1, "1");
    state1.tab_strip.InsertTabContentsAt(0, &tab1, TabStripModel::ADD_NONE);
    ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
    ash::LauncherID new_id = launcher_model_->items()[initial_size].id;
    EXPECT_EQ(id, new_id);
    EXPECT_EQ(ash::TYPE_APP, launcher_model_->items()[initial_size].type);
    EXPECT_TRUE(launcher_delegate_->IsPinned(id));
    EXPECT_EQ(&(state1.updater), GetUpdaterByID(id));

    // Add another tab.
    state1.tab_strip.InsertTabContentsAt(0, &tab3, TabStripModel::ADD_NONE);
    ASSERT_EQ(initial_size + 2, launcher_model_->items().size());
    new_id = launcher_model_->items()[initial_size].id;
    EXPECT_NE(id, new_id);
    EXPECT_EQ(ash::TYPE_TABBED, launcher_model_->items()[initial_size].type);
    EXPECT_EQ(&(state1.updater), GetUpdaterByID(new_id));
  }

  // Add it back and make sure we don't get another entry.
  ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
  {
    State state1(profile(), launcher_delegate_.get(), std::string(),
                 LauncherUpdater::TYPE_TABBED);
    state1.tab_strip.InsertTabContentsAt(0, &tab3, TabStripModel::ADD_NONE);
    ASSERT_EQ(initial_size + 2, launcher_model_->items().size());
    ash::LauncherID new_id = launcher_model_->items()[initial_size + 1].id;
    EXPECT_NE(id, new_id);
    EXPECT_EQ(ash::TYPE_TABBED,
              launcher_model_->items()[initial_size + 1].type);
    EXPECT_TRUE(GetUpdaterByID(id) == NULL);
    EXPECT_EQ(&(state1.updater), GetUpdaterByID(new_id));

    // Add the app tab.
    state1.tab_strip.InsertTabContentsAt(0, &tab1, TabStripModel::ADD_NONE);
    ASSERT_EQ(initial_size + 2, launcher_model_->items().size());
    new_id = launcher_model_->items()[initial_size].id;
    EXPECT_EQ(id, new_id);
    EXPECT_EQ(ash::TYPE_APP, launcher_model_->items()[initial_size].type);
    EXPECT_EQ(&(state1.updater), GetUpdaterByID(new_id));
  }
}

// Verifies pinned apps are persisted and restored.
TEST_F(LauncherUpdaterTest, PersistPinned) {
  size_t initial_size = launcher_model_->items().size();
  TabContentsWrapper tab1(CreateTestTabContents());

  app_icon_loader_->SetAppID(&tab1, "1");
  app_icon_loader_->SetAppID(NULL, "2");
  {
    State state1(profile(), launcher_delegate_.get(), std::string(),
                 LauncherUpdater::TYPE_TABBED);
    state1.tab_strip.InsertTabContentsAt(0, &tab1, TabStripModel::ADD_NONE);
    ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
    ash::LauncherID id = launcher_model_->items()[initial_size].id;
    EXPECT_EQ(ash::TYPE_APP, launcher_model_->items()[initial_size].type);
    // Shouldn't be pinned.
    EXPECT_FALSE(launcher_delegate_->IsPinned(id));
    launcher_delegate_->Pin(id);
    EXPECT_TRUE(launcher_delegate_->IsPinned(id));

    // Create an app window.
    State state2(profile(), launcher_delegate_.get(), "2",
                 LauncherUpdater::TYPE_APP);
    ASSERT_EQ(initial_size + 2, launcher_model_->items().size());
    ash::LauncherID id2 = launcher_model_->items()[initial_size + 1].id;
    EXPECT_NE(id, id2);
    EXPECT_EQ(ash::TYPE_APP, launcher_model_->items()[initial_size + 1].type);
    launcher_delegate_->Pin(id2);
    EXPECT_TRUE(launcher_delegate_->IsPinned(id2));
  }

  launcher_delegate_.reset(NULL);
  ASSERT_EQ(initial_size, launcher_model_->items().size());

  launcher_delegate_.reset(
      new ChromeLauncherDelegate(profile(), launcher_model_.get()));
  app_icon_loader_ = new AppIconLoaderImpl;
  app_icon_loader_->SetAppID(&tab1, "1");
  app_icon_loader_->SetAppID(NULL, "2");
  ResetAppIconLoader();
  launcher_delegate_->Init();
  ASSERT_EQ(initial_size + 2, launcher_model_->items().size());
  EXPECT_EQ(ash::TYPE_APP, launcher_model_->items()[initial_size].type);
  ash::LauncherID id = launcher_model_->items()[initial_size].id;
  EXPECT_EQ("1", GetAppID(id));
  EXPECT_EQ(ChromeLauncherDelegate::APP_TYPE_TAB,
            launcher_delegate_->GetAppType(id));
  EXPECT_TRUE(launcher_delegate_->IsPinned(id));
  EXPECT_EQ(ash::TYPE_APP, launcher_model_->items()[initial_size + 1].type);
  ash::LauncherID id2 = launcher_model_->items()[initial_size + 1].id;
  EXPECT_EQ("2", GetAppID(id2));
  EXPECT_EQ(ChromeLauncherDelegate::APP_TYPE_WINDOW,
            launcher_delegate_->GetAppType(id2));
  EXPECT_TRUE(launcher_delegate_->IsPinned(id2));

  UnpinAppsWithID("1");
  ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
  ash::LauncherID id3 = launcher_model_->items()[initial_size].id;
  EXPECT_EQ(id2, id3);
}
