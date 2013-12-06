// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_window_watcher.h"

#include "ash/launcher/launcher_types.h"
#include "ash/shelf/shelf_model.h"
#include "ash/shelf/shelf_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/shell_test_api.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"

namespace ash {
namespace internal {

class ShelfWindowWatcherTest : public test::AshTestBase {
 public:
  ShelfWindowWatcherTest() : model_(NULL) {}
  virtual ~ShelfWindowWatcherTest() {}

  virtual void SetUp() OVERRIDE {
    test::AshTestBase::SetUp();
    model_ = test::ShellTestApi(Shell::GetInstance()).shelf_model();
  }

  virtual void TearDown() OVERRIDE {
    model_ = NULL;
    test::AshTestBase::TearDown();
  }

  ash::LauncherID CreateLauncherItem(aura::Window* window) {
    LauncherID id = model_->next_id();
    ash::LauncherItemDetails item_details;
    item_details.type = TYPE_PLATFORM_APP;
    SetLauncherItemDetailsForWindow(window, item_details);
    return id;
  }

  void UpdateLauncherItem(aura::Window* window) {
  }

 protected:
  ShelfModel* model_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfWindowWatcherTest);
};

TEST_F(ShelfWindowWatcherTest, CreateAndRemoveLauncherItem) {
  // ShelfModel only has an APP_LIST item.
  EXPECT_EQ(1, model_->item_count());

  scoped_ptr<aura::Window> w1(CreateTestWindowInShellWithId(0));
  scoped_ptr<aura::Window> w2(CreateTestWindowInShellWithId(0));

  // Create a LauncherItem for w1.
  LauncherID id_w1 = CreateLauncherItem(w1.get());
  EXPECT_EQ(2, model_->item_count());

  int index_w1 = model_->ItemIndexByID(id_w1);
  EXPECT_EQ(STATUS_RUNNING, model_->items()[index_w1].status);

  // Create a LauncherItem for w2.
  LauncherID id_w2 = CreateLauncherItem(w2.get());
  EXPECT_EQ(3, model_->item_count());

  int index_w2 = model_->ItemIndexByID(id_w2);
  EXPECT_EQ(STATUS_RUNNING, model_->items()[index_w2].status);

  // LauncherItem is removed when assoicated window is destroyed.
  ClearLauncherItemDetailsForWindow(w1.get());
  EXPECT_EQ(2, model_->item_count());
  ClearLauncherItemDetailsForWindow(w2.get());
  EXPECT_EQ(1, model_->item_count());
  // Clears twice doesn't do anything.
  ClearLauncherItemDetailsForWindow(w2.get());
  EXPECT_EQ(1, model_->item_count());

}

TEST_F(ShelfWindowWatcherTest, ActivateWindow) {
  // ShelfModel only have APP_LIST item.
  EXPECT_EQ(1, model_->item_count());
  scoped_ptr<aura::Window> w1(CreateTestWindowInShellWithId(0));
  scoped_ptr<aura::Window> w2(CreateTestWindowInShellWithId(0));

  // Create a LauncherItem for w1.
  LauncherID id_w1 = CreateLauncherItem(w1.get());
  EXPECT_EQ(2, model_->item_count());
  int index_w1 = model_->ItemIndexByID(id_w1);
  EXPECT_EQ(STATUS_RUNNING, model_->items()[index_w1].status);

  // Create a LauncherItem for w2.
  LauncherID id_w2 = CreateLauncherItem(w2.get());
  EXPECT_EQ(3, model_->item_count());
  int index_w2 = model_->ItemIndexByID(id_w2);
  EXPECT_EQ(STATUS_RUNNING, model_->items()[index_w1].status);
  EXPECT_EQ(STATUS_RUNNING, model_->items()[index_w2].status);

  // LauncherItem for w1 is active when w1 is activated.
  wm::ActivateWindow(w1.get());
  EXPECT_EQ(STATUS_ACTIVE, model_->items()[index_w1].status);

  // LauncherItem for w2 is active state when w2 is activated.
  wm::ActivateWindow(w2.get());
  EXPECT_EQ(STATUS_RUNNING, model_->items()[index_w1].status);
  EXPECT_EQ(STATUS_ACTIVE, model_->items()[index_w2].status);
}

TEST_F(ShelfWindowWatcherTest, UpdateWindowProperty) {
  // ShelfModel only has an APP_LIST item.
  EXPECT_EQ(1, model_->item_count());

  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));

  // Create a LauncherItem for |window|.
  LauncherID id = CreateLauncherItem(window.get());
  EXPECT_EQ(2, model_->item_count());

  int index = model_->ItemIndexByID(id);
  EXPECT_EQ(STATUS_RUNNING, model_->items()[index].status);

  // Update LauncherItem for |window|.
  LauncherItemDetails details;
  details.type = TYPE_PLATFORM_APP;

  SetLauncherItemDetailsForWindow(window.get(), details);
  // No new item is created after updating a launcher item.
  EXPECT_EQ(2, model_->item_count());
  // index and id are not changed after updating a launcher item.
  EXPECT_EQ(index, model_->ItemIndexByID(id));
  EXPECT_EQ(id, model_->items()[index].id);
}

TEST_F(ShelfWindowWatcherTest, MaximizeAndRestoreWindow) {
  // ShelfModel only has an APP_LIST item.
  EXPECT_EQ(1, model_->item_count());

  scoped_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  wm::WindowState* window_state = wm::GetWindowState(window.get());

  // Create a LauncherItem for |window|.
  LauncherID id = CreateLauncherItem(window.get());
  EXPECT_EQ(2, model_->item_count());

  int index = model_->ItemIndexByID(id);
  EXPECT_EQ(STATUS_RUNNING, model_->items()[index].status);

  // Maximize window |window|.
  EXPECT_FALSE(window_state->IsMaximized());
  window_state->Maximize();
  EXPECT_TRUE(window_state->IsMaximized());
  // No new item is created after maximizing a window |window|.
  EXPECT_EQ(2, model_->item_count());
  // index and id are not changed after maximizing a window |window|.
  EXPECT_EQ(index, model_->ItemIndexByID(id));
  EXPECT_EQ(id, model_->items()[index].id);

  // Restore window |window|.
  window_state->Restore();
  EXPECT_FALSE(window_state->IsMaximized());
  // No new item is created after restoring a window |window|.
  EXPECT_EQ(2, model_->item_count());
  // index and id are not changed after maximizing a window |window|.
  EXPECT_EQ(index, model_->ItemIndexByID(id));
  EXPECT_EQ(id, model_->items()[index].id);
}

}  // namespace internal
}  // namespace ash
