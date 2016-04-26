// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_window_watcher.h"

#include "ash/ash_switches.h"
#include "ash/shelf/shelf_item_types.h"
#include "ash/shelf/shelf_model.h"
#include "ash/shelf/shelf_util.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/shell_test_api.h"
#include "ash/wm/aura/wm_window_aura.h"
#include "ash/wm/common/window_resizer.h"
#include "ash/wm/common/window_state.h"
#include "ash/wm/window_state_aura.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"

namespace ash {

class ShelfWindowWatcherTest : public test::AshTestBase {
 public:
  ShelfWindowWatcherTest() : model_(NULL) {}
  ~ShelfWindowWatcherTest() override {}

  void SetUp() override {
    test::AshTestBase::SetUp();
    model_ = test::ShellTestApi(Shell::GetInstance()).shelf_model();
  }

  void TearDown() override {
    model_ = NULL;
    test::AshTestBase::TearDown();
  }

  ShelfID CreateShelfItem(aura::Window* window) {
    ShelfID id = model_->next_id();
    ShelfItemDetails item_details;
    item_details.type = TYPE_PLATFORM_APP;
    SetShelfItemDetailsForWindow(window, item_details);
    return id;
  }

 protected:
  ShelfModel* model_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfWindowWatcherTest);
};

TEST_F(ShelfWindowWatcherTest, CreateAndRemoveShelfItem) {
  // ShelfModel only has an APP_LIST item.
  EXPECT_EQ(1, model_->item_count());

  std::unique_ptr<aura::Window> w1(CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> w2(CreateTestWindowInShellWithId(0));

  // Create a ShelfItem for w1.
  ShelfID id_w1 = CreateShelfItem(w1.get());
  EXPECT_EQ(2, model_->item_count());

  int index_w1 = model_->ItemIndexByID(id_w1);
  EXPECT_EQ(STATUS_RUNNING, model_->items()[index_w1].status);

  // Create a ShelfItem for w2.
  ShelfID id_w2 = CreateShelfItem(w2.get());
  EXPECT_EQ(3, model_->item_count());

  int index_w2 = model_->ItemIndexByID(id_w2);
  EXPECT_EQ(STATUS_RUNNING, model_->items()[index_w2].status);

  // ShelfItem is removed when assoicated window is destroyed.
  ClearShelfItemDetailsForWindow(w1.get());
  EXPECT_EQ(2, model_->item_count());
  ClearShelfItemDetailsForWindow(w2.get());
  EXPECT_EQ(1, model_->item_count());
  // Clears twice doesn't do anything.
  ClearShelfItemDetailsForWindow(w2.get());
  EXPECT_EQ(1, model_->item_count());

}

TEST_F(ShelfWindowWatcherTest, ActivateWindow) {
  // ShelfModel only have APP_LIST item.
  EXPECT_EQ(1, model_->item_count());
  std::unique_ptr<aura::Window> w1(CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> w2(CreateTestWindowInShellWithId(0));

  // Create a ShelfItem for w1.
  ShelfID id_w1 = CreateShelfItem(w1.get());
  EXPECT_EQ(2, model_->item_count());
  int index_w1 = model_->ItemIndexByID(id_w1);
  EXPECT_EQ(STATUS_RUNNING, model_->items()[index_w1].status);

  // Create a ShelfItem for w2.
  ShelfID id_w2 = CreateShelfItem(w2.get());
  EXPECT_EQ(3, model_->item_count());
  int index_w2 = model_->ItemIndexByID(id_w2);
  EXPECT_EQ(STATUS_RUNNING, model_->items()[index_w1].status);
  EXPECT_EQ(STATUS_RUNNING, model_->items()[index_w2].status);

  // ShelfItem for w1 is active when w1 is activated.
  wm::ActivateWindow(w1.get());
  EXPECT_EQ(STATUS_ACTIVE, model_->items()[index_w1].status);

  // ShelfItem for w2 is active state when w2 is activated.
  wm::ActivateWindow(w2.get());
  EXPECT_EQ(STATUS_RUNNING, model_->items()[index_w1].status);
  EXPECT_EQ(STATUS_ACTIVE, model_->items()[index_w2].status);
}

TEST_F(ShelfWindowWatcherTest, UpdateWindowProperty) {
  // ShelfModel only has an APP_LIST item.
  EXPECT_EQ(1, model_->item_count());

  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));

  // Create a ShelfItem for |window|.
  ShelfID id = CreateShelfItem(window.get());
  EXPECT_EQ(2, model_->item_count());

  int index = model_->ItemIndexByID(id);
  EXPECT_EQ(STATUS_RUNNING, model_->items()[index].status);

  // Update ShelfItem for |window|.
  ShelfItemDetails details;
  details.type = TYPE_PLATFORM_APP;

  SetShelfItemDetailsForWindow(window.get(), details);
  // No new item is created after updating a launcher item.
  EXPECT_EQ(2, model_->item_count());
  // index and id are not changed after updating a launcher item.
  EXPECT_EQ(index, model_->ItemIndexByID(id));
  EXPECT_EQ(id, model_->items()[index].id);
}

TEST_F(ShelfWindowWatcherTest, MaximizeAndRestoreWindow) {
  // ShelfModel only has an APP_LIST item.
  EXPECT_EQ(1, model_->item_count());

  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  wm::WindowState* window_state = wm::GetWindowState(window.get());

  // Create a ShelfItem for |window|.
  ShelfID id = CreateShelfItem(window.get());
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
  // Index and id are not changed after maximizing a window |window|.
  EXPECT_EQ(index, model_->ItemIndexByID(id));
  EXPECT_EQ(id, model_->items()[index].id);
}

// Check that an item is removed when its associated Window is re-parented.
TEST_F(ShelfWindowWatcherTest, ReparentWindow) {
  // ShelfModel only has an APP_LIST item.
  EXPECT_EQ(1, model_->item_count());

  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  window->set_owned_by_parent(false);

  // Create a ShelfItem for |window|.
  ShelfID id = CreateShelfItem(window.get());
  EXPECT_EQ(2, model_->item_count());

  int index = model_->ItemIndexByID(id);
  EXPECT_EQ(STATUS_RUNNING, model_->items()[index].status);

  aura::Window* root_window = window->GetRootWindow();
  aura::Window* default_container = Shell::GetContainer(
      root_window,
      kShellWindowId_DefaultContainer);
  EXPECT_EQ(default_container, window->parent());

  aura::Window* new_parent = Shell::GetContainer(
      root_window,
      kShellWindowId_PanelContainer);

  // Check |window|'s item is removed when it is re-parented to |new_parent|
  // which is not default container.
  new_parent->AddChild(window.get());
  EXPECT_EQ(1, model_->item_count());

  // Check |window|'s item is added when it is re-parented to
  // |default_container|.
  default_container->AddChild(window.get());
  EXPECT_EQ(2, model_->item_count());
}

// Check |window|'s item is not changed during the dragging.
// TODO(simonhong): Add a test for removing a Window during the dragging.
TEST_F(ShelfWindowWatcherTest, DragWindow) {
  // ShelfModel only has an APP_LIST item.
  EXPECT_EQ(1, model_->item_count());

  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));

  // Create a ShelfItem for |window|.
  ShelfID id = CreateShelfItem(window.get());
  EXPECT_EQ(2, model_->item_count());

  int index = model_->ItemIndexByID(id);
  EXPECT_EQ(STATUS_RUNNING, model_->items()[index].status);

  // Simulate dragging of |window| and check its item is not changed.
  std::unique_ptr<WindowResizer> resizer(
      CreateWindowResizer(wm::WmWindowAura::Get(window.get()), gfx::Point(),
                          HTCAPTION, aura::client::WINDOW_MOVE_SOURCE_MOUSE));
  ASSERT_TRUE(resizer.get());
  resizer->Drag(gfx::Point(50, 50), 0);
  resizer->CompleteDrag();

  //Index and id are not changed after dragging a |window|.
  EXPECT_EQ(index, model_->ItemIndexByID(id));
  EXPECT_EQ(id, model_->items()[index].id);
}

// Check |window|'s item is removed when it is re-parented not to default
// container during the dragging.
TEST_F(ShelfWindowWatcherTest, ReparentWindowDuringTheDragging) {
  // ShelfModel only has an APP_LIST item.
  EXPECT_EQ(1, model_->item_count());

  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithId(0));
  window->set_owned_by_parent(false);

  // Create a ShelfItem for |window|.
  ShelfID id = CreateShelfItem(window.get());
  EXPECT_EQ(2, model_->item_count());
  int index = model_->ItemIndexByID(id);
  EXPECT_EQ(STATUS_RUNNING, model_->items()[index].status);

  aura::Window* root_window = window->GetRootWindow();
  aura::Window* default_container = Shell::GetContainer(
      root_window,
      kShellWindowId_DefaultContainer);
  EXPECT_EQ(default_container, window->parent());

  aura::Window* new_parent = Shell::GetContainer(
      root_window,
      kShellWindowId_PanelContainer);

  // Simulate re-parenting to |new_parent| during the dragging.
  {
    std::unique_ptr<WindowResizer> resizer(
        CreateWindowResizer(wm::WmWindowAura::Get(window.get()), gfx::Point(),
                            HTCAPTION, aura::client::WINDOW_MOVE_SOURCE_MOUSE));
    ASSERT_TRUE(resizer.get());
    resizer->Drag(gfx::Point(50, 50), 0);
    resizer->CompleteDrag();
    EXPECT_EQ(2, model_->item_count());

    // Item should be removed when |window| is re-parented not to default
    // container before fininshing the dragging.
    EXPECT_TRUE(wm::GetWindowState(window.get())->is_dragged());
    new_parent->AddChild(window.get());
    EXPECT_EQ(1, model_->item_count());
  }
  EXPECT_FALSE(wm::GetWindowState(window.get())->is_dragged());
  EXPECT_EQ(1, model_->item_count());
}

}  // namespace ash
