// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/shelf_window_watcher.h"

#include "ash/common/session/session_state_delegate.h"
#include "ash/common/shelf/shelf_item_types.h"
#include "ash/common/shelf/shelf_model.h"
#include "ash/common/wm/window_resizer.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/common/wm_window_property.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/test/ash_test_base.h"
#include "ui/base/hit_test.h"
#include "ui/views/widget/widget.h"

namespace ash {

class ShelfWindowWatcherTest : public test::AshTestBase {
 public:
  ShelfWindowWatcherTest() : model_(nullptr) {}
  ~ShelfWindowWatcherTest() override {}

  void SetUp() override {
    test::AshTestBase::SetUp();
    model_ = WmShell::Get()->shelf_model();
  }

  void TearDown() override {
    model_ = nullptr;
    test::AshTestBase::TearDown();
  }

  static ShelfID CreateShelfItem(WmWindow* window) {
    ShelfID id = WmShell::Get()->shelf_model()->next_id();
    window->SetIntProperty(WmWindowProperty::SHELF_ITEM_TYPE, TYPE_DIALOG);
    return id;
  }

 protected:
  ShelfModel* model_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfWindowWatcherTest);
};

// Ensure shelf items are added and removed as windows are opened and closed.
TEST_F(ShelfWindowWatcherTest, OpenAndClose) {
  // ShelfModel only has an APP_LIST item.
  EXPECT_EQ(1, model_->item_count());

  // Adding windows with valid ShelfItemType properties adds shelf items.
  std::unique_ptr<views::Widget> widget1 =
      CreateTestWidget(nullptr, kShellWindowId_DefaultContainer, gfx::Rect());
  CreateShelfItem(WmLookup::Get()->GetWindowForWidget(widget1.get()));
  EXPECT_EQ(2, model_->item_count());
  std::unique_ptr<views::Widget> widget2 =
      CreateTestWidget(nullptr, kShellWindowId_DefaultContainer, gfx::Rect());
  CreateShelfItem(WmLookup::Get()->GetWindowForWidget(widget2.get()));
  EXPECT_EQ(3, model_->item_count());

  // Each ShelfItem is removed when the associated window is destroyed.
  widget1.reset();
  EXPECT_EQ(2, model_->item_count());
  widget2.reset();
  EXPECT_EQ(1, model_->item_count());
}

TEST_F(ShelfWindowWatcherTest, CreateAndRemoveShelfItemProperties) {
  // TODO: investigate failure in mash. http://crbug.com/695562.
  if (WmShell::Get()->IsRunningInMash())
    return;

  // ShelfModel only has an APP_LIST item.
  EXPECT_EQ(1, model_->item_count());

  // Creating windows without a valid ShelfItemType does not add items.
  std::unique_ptr<views::Widget> widget1 =
      CreateTestWidget(nullptr, kShellWindowId_DefaultContainer, gfx::Rect());
  WmWindow* window1 = WmLookup::Get()->GetWindowForWidget(widget1.get());
  std::unique_ptr<views::Widget> widget2 =
      CreateTestWidget(nullptr, kShellWindowId_DefaultContainer, gfx::Rect());
  WmWindow* window2 = WmLookup::Get()->GetWindowForWidget(widget2.get());
  EXPECT_EQ(1, model_->item_count());

  // Create a ShelfItem for the first window.
  ShelfID id_w1 = CreateShelfItem(window1);
  EXPECT_EQ(2, model_->item_count());

  int index_w1 = model_->ItemIndexByID(id_w1);
  EXPECT_EQ(STATUS_RUNNING, model_->items()[index_w1].status);

  // Create a ShelfItem for the second window.
  ShelfID id_w2 = CreateShelfItem(window2);
  EXPECT_EQ(3, model_->item_count());

  int index_w2 = model_->ItemIndexByID(id_w2);
  EXPECT_EQ(STATUS_ACTIVE, model_->items()[index_w2].status);

  // ShelfItem is removed when the item type window property is cleared.
  window1->SetIntProperty(WmWindowProperty::SHELF_ITEM_TYPE, TYPE_UNDEFINED);
  EXPECT_EQ(2, model_->item_count());
  window2->SetIntProperty(WmWindowProperty::SHELF_ITEM_TYPE, TYPE_UNDEFINED);
  EXPECT_EQ(1, model_->item_count());
  // Clearing twice doesn't do anything.
  window2->SetIntProperty(WmWindowProperty::SHELF_ITEM_TYPE, TYPE_UNDEFINED);
  EXPECT_EQ(1, model_->item_count());
}

TEST_F(ShelfWindowWatcherTest, ActivateWindow) {
  // TODO: investigate failure in mash. http://crbug.com/695562.
  if (WmShell::Get()->IsRunningInMash())
    return;

  // ShelfModel only have APP_LIST item.
  EXPECT_EQ(1, model_->item_count());
  std::unique_ptr<views::Widget> widget1 =
      CreateTestWidget(nullptr, kShellWindowId_DefaultContainer, gfx::Rect());
  WmWindow* window1 = WmLookup::Get()->GetWindowForWidget(widget1.get());
  std::unique_ptr<views::Widget> widget2 =
      CreateTestWidget(nullptr, kShellWindowId_DefaultContainer, gfx::Rect());
  WmWindow* window2 = WmLookup::Get()->GetWindowForWidget(widget2.get());

  // Create a ShelfItem for the first window.
  ShelfID id_w1 = CreateShelfItem(window1);
  EXPECT_EQ(2, model_->item_count());
  int index_w1 = model_->ItemIndexByID(id_w1);
  EXPECT_EQ(STATUS_RUNNING, model_->items()[index_w1].status);

  // Create a ShelfItem for the second window.
  ShelfID id_w2 = CreateShelfItem(window2);
  EXPECT_EQ(3, model_->item_count());
  int index_w2 = model_->ItemIndexByID(id_w2);
  EXPECT_EQ(STATUS_RUNNING, model_->items()[index_w1].status);
  EXPECT_EQ(STATUS_ACTIVE, model_->items()[index_w2].status);

  // The ShelfItem for the first window is active when the window is activated.
  widget1->Activate();
  EXPECT_EQ(STATUS_ACTIVE, model_->items()[index_w1].status);
  EXPECT_EQ(STATUS_RUNNING, model_->items()[index_w2].status);

  // The ShelfItem for the second window is active when the window is activated.
  widget2->Activate();
  EXPECT_EQ(STATUS_RUNNING, model_->items()[index_w1].status);
  EXPECT_EQ(STATUS_ACTIVE, model_->items()[index_w2].status);
}

TEST_F(ShelfWindowWatcherTest, UpdateWindowProperty) {
  // TODO: investigate failure in mash. http://crbug.com/695562.
  if (WmShell::Get()->IsRunningInMash())
    return;

  // ShelfModel only has an APP_LIST item.
  EXPECT_EQ(1, model_->item_count());

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(nullptr, kShellWindowId_DefaultContainer, gfx::Rect());
  WmWindow* window = WmLookup::Get()->GetWindowForWidget(widget.get());

  // Create a ShelfItem for |window|.
  ShelfID id = CreateShelfItem(window);
  EXPECT_EQ(2, model_->item_count());

  int index = model_->ItemIndexByID(id);
  EXPECT_EQ(STATUS_ACTIVE, model_->items()[index].status);

  // Update the ShelfItemType for |window|.
  window->SetIntProperty(WmWindowProperty::SHELF_ITEM_TYPE, TYPE_APP);
  // No new item is created after updating a launcher item.
  EXPECT_EQ(2, model_->item_count());
  // index and id are not changed after updating a launcher item.
  EXPECT_EQ(index, model_->ItemIndexByID(id));
  EXPECT_EQ(id, model_->items()[index].id);
}

TEST_F(ShelfWindowWatcherTest, MaximizeAndRestoreWindow) {
  // TODO: investigate failure in mash. http://crbug.com/695562.
  if (WmShell::Get()->IsRunningInMash())
    return;

  // ShelfModel only has an APP_LIST item.
  EXPECT_EQ(1, model_->item_count());

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(nullptr, kShellWindowId_DefaultContainer, gfx::Rect());
  WmWindow* window = WmLookup::Get()->GetWindowForWidget(widget.get());
  wm::WindowState* window_state = window->GetWindowState();

  // Create a ShelfItem for |window|.
  ShelfID id = CreateShelfItem(window);
  EXPECT_EQ(2, model_->item_count());

  int index = model_->ItemIndexByID(id);
  EXPECT_EQ(STATUS_ACTIVE, model_->items()[index].status);

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

// Check that an item is maintained when its associated Window is docked.
TEST_F(ShelfWindowWatcherTest, DockWindow) {
  // TODO: investigate failure in mash. http://crbug.com/695562.
  if (WmShell::Get()->IsRunningInMash())
    return;

  // ShelfModel only has an APP_LIST item.
  EXPECT_EQ(1, model_->item_count());

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(nullptr, kShellWindowId_DefaultContainer, gfx::Rect());
  WmWindow* window = WmLookup::Get()->GetWindowForWidget(widget.get());

  // Create a ShelfItem for |window|.
  ShelfID id = CreateShelfItem(window);
  EXPECT_EQ(2, model_->item_count());

  int index = model_->ItemIndexByID(id);
  EXPECT_EQ(STATUS_ACTIVE, model_->items()[index].status);

  WmWindow* root_window = window->GetRootWindow();
  WmWindow* default_container =
      root_window->GetChildByShellWindowId(kShellWindowId_DefaultContainer);
  EXPECT_EQ(default_container, window->GetParent());

  WmWindow* docked_container =
      root_window->GetChildByShellWindowId(kShellWindowId_DockedContainer);

  // Check |window|'s item is not removed when it is re-parented to the dock.
  docked_container->AddChild(window);
  EXPECT_EQ(docked_container, window->GetParent());
  EXPECT_EQ(2, model_->item_count());

  // The shelf item is removed when the window is closed, even if it is in the
  // docked container at the time.
  widget.reset();
  EXPECT_EQ(1, model_->item_count());
}

// Check |window|'s item is not changed during the dragging.
// TODO(simonhong): Add a test for removing a Window during the dragging.
TEST_F(ShelfWindowWatcherTest, DragWindow) {
  // TODO: investigate failure in mash. http://crbug.com/695562.
  if (WmShell::Get()->IsRunningInMash())
    return;

  // ShelfModel only has an APP_LIST item.
  EXPECT_EQ(1, model_->item_count());

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(nullptr, kShellWindowId_DefaultContainer, gfx::Rect());
  WmWindow* window = WmLookup::Get()->GetWindowForWidget(widget.get());

  // Create a ShelfItem for |window|.
  ShelfID id = CreateShelfItem(window);
  EXPECT_EQ(2, model_->item_count());

  int index = model_->ItemIndexByID(id);
  EXPECT_EQ(STATUS_ACTIVE, model_->items()[index].status);

  // Simulate dragging of |window| and check its item is not changed.
  std::unique_ptr<WindowResizer> resizer(CreateWindowResizer(
      window, gfx::Point(), HTCAPTION, aura::client::WINDOW_MOVE_SOURCE_MOUSE));
  ASSERT_TRUE(resizer.get());
  resizer->Drag(gfx::Point(50, 50), 0);
  resizer->CompleteDrag();

  // Index and id are not changed after dragging a |window|.
  EXPECT_EQ(index, model_->ItemIndexByID(id));
  EXPECT_EQ(id, model_->items()[index].id);
}

// Ensure shelf items are added and removed as panels are opened and closed.
TEST_F(ShelfWindowWatcherTest, PanelWindow) {
  // TODO: investigate failure in mash. http://crbug.com/695562.
  if (WmShell::Get()->IsRunningInMash())
    return;

  // ShelfModel only has an APP_LIST item.
  EXPECT_EQ(1, model_->item_count());

  // Adding windows with valid ShelfItemType properties adds shelf items.
  std::unique_ptr<views::Widget> widget1 =
      CreateTestWidget(nullptr, kShellWindowId_PanelContainer, gfx::Rect());
  WmWindow* window1 = WmLookup::Get()->GetWindowForWidget(widget1.get());
  window1->SetIntProperty(WmWindowProperty::SHELF_ITEM_TYPE, TYPE_APP_PANEL);
  EXPECT_EQ(2, model_->item_count());
  std::unique_ptr<views::Widget> widget2 =
      CreateTestWidget(nullptr, kShellWindowId_DefaultContainer, gfx::Rect());
  WmWindow* window2 = WmLookup::Get()->GetWindowForWidget(widget2.get());
  window2->SetIntProperty(WmWindowProperty::SHELF_ITEM_TYPE, TYPE_APP_PANEL);
  EXPECT_EQ(3, model_->item_count());

  // Create a panel-type widget to mimic Chrome's app panel windows.
  views::Widget panel_widget;
  views::Widget::InitParams panel_params(views::Widget::InitParams::TYPE_PANEL);
  panel_params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  WmShell::Get()
      ->GetPrimaryRootWindow()
      ->GetRootWindowController()
      ->ConfigureWidgetInitParamsForContainer(
          &panel_widget, kShellWindowId_PanelContainer, &panel_params);
  panel_widget.Init(panel_params);
  panel_widget.Show();
  WmWindow* panel_window = WmLookup::Get()->GetWindowForWidget(&panel_widget);
  panel_window->SetIntProperty(WmWindowProperty::SHELF_ITEM_TYPE,
                               TYPE_APP_PANEL);
  EXPECT_EQ(4, model_->item_count());

  // Each ShelfItem is removed when the associated window is destroyed.
  panel_widget.CloseNow();
  EXPECT_EQ(3, model_->item_count());
  widget2.reset();
  EXPECT_EQ(2, model_->item_count());
  widget1.reset();
  EXPECT_EQ(1, model_->item_count());
}

TEST_F(ShelfWindowWatcherTest, DontCreateShelfEntriesForChildWindows) {
  const int initial_item_count = model_->item_count();

  WmWindow* window = WmShell::Get()->NewWindow(ui::wm::WINDOW_TYPE_NORMAL,
                                               ui::LAYER_NOT_DRAWN);
  window->SetIntProperty(WmWindowProperty::SHELF_ITEM_TYPE, TYPE_APP);
  WmShell::Get()
      ->GetPrimaryRootWindow()
      ->GetChildByShellWindowId(kShellWindowId_DefaultContainer)
      ->AddChild(window);
  window->Show();
  EXPECT_EQ(initial_item_count + 1, model_->item_count());

  WmWindow* child_window = WmShell::Get()->NewWindow(ui::wm::WINDOW_TYPE_NORMAL,
                                                     ui::LAYER_NOT_DRAWN);
  child_window->SetIntProperty(WmWindowProperty::SHELF_ITEM_TYPE, TYPE_APP);
  window->AddChild(child_window);
  child_window->Show();
  // |child_window| should not result in adding a new entry.
  EXPECT_EQ(initial_item_count + 1, model_->item_count());

  child_window->Destroy();
  window->Destroy();
  EXPECT_EQ(initial_item_count, model_->item_count());
}

// Ensures ShelfWindowWatcher supports windows opened prior to session start.
using ShelfWindowWatcherSessionStartTest = test::NoSessionAshTestBase;
TEST_F(ShelfWindowWatcherSessionStartTest, PreExistingWindow) {
  ShelfModel* model = WmShell::Get()->shelf_model();
  ASSERT_FALSE(
      WmShell::Get()->GetSessionStateDelegate()->IsActiveUserSessionStarted());

  // ShelfModel only has an APP_LIST item.
  EXPECT_EQ(1, model->item_count());

  // Construct a window that should get a shelf item once the session starts.
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(nullptr, kShellWindowId_DefaultContainer, gfx::Rect());
  WmWindow* window = WmLookup::Get()->GetWindowForWidget(widget.get());
  ShelfWindowWatcherTest::CreateShelfItem(window);
  EXPECT_EQ(1, model->item_count());

  // Start the test user session; ShelfWindowWatcher will find the open window.
  SetSessionStarted(true);
  EXPECT_EQ(2, model->item_count());
}

}  // namespace ash
