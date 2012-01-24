// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/aura/launcher_icon_updater.h"

#include <map>
#include <string>

#include "ash/launcher/launcher_model.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
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
class AppIconLoaderImpl : public LauncherIconUpdater::AppIconLoader {
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

  virtual void Remove(TabContentsWrapper* tab) OVERRIDE {
    tab_id_map_.erase(tab);
  }

  virtual void FetchImage(TabContentsWrapper* tab) OVERRIDE {
    fetch_count_++;
  }

 private:
  typedef std::map<TabContentsWrapper*, std::string> TabToStringMap;

  TabToStringMap tab_id_map_;

  int fetch_count_;

  DISALLOW_COPY_AND_ASSIGN(AppIconLoaderImpl);
};

// Contains all the objects needed to create a LauncherIconUpdater.
struct State {
  State(Profile* profile,
        ash::LauncherModel* launcher_model,
        LauncherIconUpdater::Type launcher_type)
      : window(NULL),
        tab_strip(&tab_strip_delegate, profile),
        icon_updater(&window, &tab_strip, launcher_model, launcher_type),
        app_icon_loader(new AppIconLoaderImpl) {
    icon_updater.SetAppIconLoaderForTest(app_icon_loader);
    icon_updater.Init();
  }

  aura::Window window;
  TestTabStripModelDelegate tab_strip_delegate;
  TabStripModel tab_strip;
  LauncherIconUpdater icon_updater;

  // Owned by LauncherIconUpdater.
  AppIconLoaderImpl* app_icon_loader;

 private:
  DISALLOW_COPY_AND_ASSIGN(State);
};

}  // namespace

class LauncherIconUpdaterTest : public ChromeRenderViewHostTestHarness {
 public:
  LauncherIconUpdaterTest()
      : browser_thread_(content::BrowserThread::UI, &message_loop_) {
  }

  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();
    launcher_model_.reset(new ash::LauncherModel);
  }

 protected:
  scoped_ptr<ash::LauncherModel> launcher_model_;

 private:
  content::TestBrowserThread browser_thread_;

  DISALLOW_COPY_AND_ASSIGN(LauncherIconUpdaterTest);
};

// Verifies a new launcher item is added for TYPE_TABBED.
TEST_F(LauncherIconUpdaterTest, TabbedSetup) {
  size_t initial_size = launcher_model_->items().size();
  {
    TabContentsWrapper wrapper(CreateTestTabContents());
    State state(profile(), launcher_model_.get(),
                LauncherIconUpdater::TYPE_TABBED);
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
  // Deleting the LauncherIconUpdater should have removed the item.
  ASSERT_EQ(initial_size, launcher_model_->items().size());

  // Do the same, but this time add the tab first.
  {
    TabContentsWrapper wrapper(CreateTestTabContents());

    TestTabStripModelDelegate tab_strip_delegate;
    TabStripModel tab_strip(&tab_strip_delegate, profile());
    tab_strip.InsertTabContentsAt(0, &wrapper, TabStripModel::ADD_NONE);
    LauncherIconUpdater icon_updater(NULL, &tab_strip, launcher_model_.get(),
                                     LauncherIconUpdater::TYPE_TABBED);
    icon_updater.SetAppIconLoaderForTest(new AppIconLoaderImpl);
    icon_updater.Init();

    // There should be one more item.
    ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
    // New item should be added at the end.
    EXPECT_EQ(ash::TYPE_TABBED, launcher_model_->items()[initial_size].type);
  }
}

// Verifies a new launcher item is added for TYPE_APP.
TEST_F(LauncherIconUpdaterTest, AppSetup) {
  size_t initial_size = launcher_model_->items().size();
  {
    State state(profile(), launcher_model_.get(),
                LauncherIconUpdater::TYPE_APP);
    // There should be one more item.
    ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
    // New item should be added at the end.
    EXPECT_EQ(ash::TYPE_APP, launcher_model_->items()[initial_size].type);
  }
  // Deleting the LauncherIconUpdater should have removed the item.
  ASSERT_EQ(initial_size, launcher_model_->items().size());
}

// Various assertions when adding/removing a tab that has an app associated with
// it.
TEST_F(LauncherIconUpdaterTest, TabbedWithApp) {
  size_t initial_size = launcher_model_->items().size();
  {
    TabContentsWrapper initial_tab(CreateTestTabContents());
    State state(profile(), launcher_model_.get(),
                LauncherIconUpdater::TYPE_TABBED);
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
    state.app_icon_loader->SetAppID(&app_tab, "1");
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
  // Deleting the LauncherIconUpdater should have removed the item.
  ASSERT_EQ(initial_size, launcher_model_->items().size());
}

// Verifies transitioning from a normal tab to app tab and back works.
TEST_F(LauncherIconUpdaterTest, ChangeToApp) {
  size_t initial_size = launcher_model_->items().size();
  {
    TabContentsWrapper initial_tab(CreateTestTabContents());
    State state(profile(), launcher_model_.get(),
                LauncherIconUpdater::TYPE_TABBED);
    // Add a tab.
    state.tab_strip.InsertTabContentsAt(0, &initial_tab,
                                        TabStripModel::ADD_NONE);

    // There should be one more item.
    ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
    // New item should be added at the end.
    EXPECT_EQ(ash::TYPE_TABBED, launcher_model_->items()[initial_size].type);
    ash::LauncherID tabbed_id = launcher_model_->items()[initial_size].id;

    state.app_icon_loader->SetAppID(&initial_tab, "1");
    // Triggers LauncherIconUpdater seeing the tab changed to an app.
    state.icon_updater.TabChangedAt(
        &initial_tab, 0, TabStripModelObserver::ALL);
    ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
    EXPECT_EQ(ash::TYPE_APP, launcher_model_->items()[initial_size].type);
    EXPECT_EQ(tabbed_id, launcher_model_->items()[initial_size].id);

    // Change back to a non-app and make sure the tabbed item is added back.
    state.app_icon_loader->SetAppID(&initial_tab, std::string());
    state.icon_updater.TabChangedAt(
        &initial_tab, 0, TabStripModelObserver::ALL);
    ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
    EXPECT_EQ(ash::TYPE_TABBED, launcher_model_->items()[initial_size].type);
    EXPECT_EQ(tabbed_id, launcher_model_->items()[initial_size].id);
  }
  // Deleting the LauncherIconUpdater should have removed the item.
  ASSERT_EQ(initial_size, launcher_model_->items().size());
}

// Verifies AppIconLoader is queried appropriately.
TEST_F(LauncherIconUpdaterTest, QueryAppIconLoader) {
  size_t initial_size = launcher_model_->items().size();
  {
    TabContentsWrapper initial_tab(CreateTestTabContents());
    State state(profile(), launcher_model_.get(),
                LauncherIconUpdater::TYPE_TABBED);
    // Configure the tab as an app.
    state.app_icon_loader->SetAppID(&initial_tab, "1");
    // Add a tab.
    state.tab_strip.InsertTabContentsAt(0, &initial_tab,
                                        TabStripModel::ADD_NONE);
    // AppIconLoader should have been queried.
    EXPECT_GT(state.app_icon_loader->GetAndClearFetchCount(), 0);
    // Remove the tab.
    state.tab_strip.DetachTabContentsAt(0);
    // Removing the tab should have invoked Remove, which unregisters.
    EXPECT_FALSE(state.app_icon_loader->HasAppID(&initial_tab));
  }
  // Deleting the LauncherIconUpdater should have removed the item.
  ASSERT_EQ(initial_size, launcher_model_->items().size());
}

// Verifies SetAppImage works.
TEST_F(LauncherIconUpdaterTest, SetAppImage) {
  size_t initial_size = launcher_model_->items().size();
  TabContentsWrapper initial_tab(CreateTestTabContents());
  State state(profile(), launcher_model_.get(),
              LauncherIconUpdater::TYPE_TABBED);
  // Configure the tab as an app.
  state.app_icon_loader->SetAppID(&initial_tab, "1");
  // Add a tab.
  state.tab_strip.InsertTabContentsAt(0, &initial_tab,
                                      TabStripModel::ADD_NONE);
  SkBitmap image;
  image.setConfig(SkBitmap::kARGB_8888_Config, 2, 3);
  image.allocPixels();
  state.icon_updater.SetAppImage(&initial_tab, &image);
  ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
  EXPECT_EQ(2, launcher_model_->items()[initial_size].image.width());
  EXPECT_EQ(3, launcher_model_->items()[initial_size].image.height());
}

// Verifies app tabs are added right after the existing tabbed item.
TEST_F(LauncherIconUpdaterTest, AddAppAfterTabbed) {
  size_t initial_size = launcher_model_->items().size();
  TabContentsWrapper tab1(CreateTestTabContents());
  State state1(profile(), launcher_model_.get(),
               LauncherIconUpdater::TYPE_TABBED);
  // Add a tab.
  state1.tab_strip.InsertTabContentsAt(0, &tab1, TabStripModel::ADD_NONE);
  ASSERT_EQ(initial_size + 1, launcher_model_->items().size());
  ash::LauncherID tabbed_id = launcher_model_->items()[initial_size].id;

  // Create another LauncherIconUpdater.
  State state2(profile(), launcher_model_.get(),
               LauncherIconUpdater::TYPE_APP);

  // Should be two extra items.
  EXPECT_EQ(initial_size + 2, launcher_model_->items().size());

  // Add an app tab to state1, it should go after the item for state1 but
  // before the item for state2.
  int next_id = launcher_model_->next_id();
  TabContentsWrapper app_tab(CreateTestTabContents());
  state1.app_icon_loader->SetAppID(&app_tab, "1");
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
TEST_F(LauncherIconUpdaterTest, GetLauncherByID) {
  size_t initial_size = launcher_model_->items().size();
  TabContentsWrapper tab1(CreateTestTabContents());
  TabContentsWrapper tab2(CreateTestTabContents());
  TabContentsWrapper tab3(CreateTestTabContents());

  // Create 3 states:
  // . tabbed with an app tab and normal tab.
  // . tabbed with a normal tab.
  // . app.
  State state1(profile(), launcher_model_.get(),
               LauncherIconUpdater::TYPE_TABBED);
  state1.tab_strip.InsertTabContentsAt(0, &tab1, TabStripModel::ADD_NONE);
  state1.app_icon_loader->SetAppID(&tab2, "1");
  state1.tab_strip.InsertTabContentsAt(0, &tab2, TabStripModel::ADD_NONE);
  State state2(profile(), launcher_model_.get(),
               LauncherIconUpdater::TYPE_TABBED);
  state2.tab_strip.InsertTabContentsAt(0, &tab3, TabStripModel::ADD_NONE);
  State state3(profile(), launcher_model_.get(),
               LauncherIconUpdater::TYPE_APP);
  ASSERT_EQ(initial_size + 4, launcher_model_->items().size());

  // Tabbed item from first state.
  TabContentsWrapper* tab = NULL;
  const LauncherIconUpdater* updater =
      LauncherIconUpdater::GetLauncherByID(
          launcher_model_->items()[initial_size].id, &tab);
  EXPECT_EQ(&(state1.icon_updater), updater);
  EXPECT_TRUE(tab == NULL);

  // App item from first state.
  updater = LauncherIconUpdater::GetLauncherByID(
      launcher_model_->items()[initial_size + 1].id, &tab);
  EXPECT_EQ(&(state1.icon_updater), updater);
  EXPECT_TRUE(tab == &tab2);

  // Tabbed item from second state.
  updater = LauncherIconUpdater::GetLauncherByID(
      launcher_model_->items()[initial_size + 2].id, &tab);
  EXPECT_EQ(&(state2.icon_updater), updater);
  EXPECT_TRUE(tab == NULL);

  // App item.
  updater = LauncherIconUpdater::GetLauncherByID(
      launcher_model_->items()[initial_size + 3].id, &tab);
  EXPECT_EQ(&(state3.icon_updater), updater);
  EXPECT_TRUE(tab == NULL);
}
