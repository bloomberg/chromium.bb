// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "ash/public/cpp/shelf_model.h"
#include "ash/root_window_controller.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_button.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "mojo/public/cpp/bindings/associated_binding.h"

namespace ash {
namespace {

Shelf* GetShelfForDisplay(int64_t display_id) {
  return Shell::GetRootWindowControllerWithDisplayId(display_id)->shelf();
}

// Tracks shelf initialization display ids.
class ShelfInitializationObserver : public mojom::ShelfObserver {
 public:
  ShelfInitializationObserver() = default;
  ~ShelfInitializationObserver() override = default;

  // mojom::ShelfObserver:
  void OnShelfInitialized(int64_t display_id) override {
    shelf_initialized_display_ids_.push_back(display_id);
  }
  void OnAlignmentChanged(ShelfAlignment alignment,
                          int64_t display_id) override {}
  void OnAutoHideBehaviorChanged(ShelfAutoHideBehavior auto_hide,
                                 int64_t display_id) override {}
  void OnShelfItemAdded(int32_t, const ShelfItem&) override {}
  void OnShelfItemRemoved(const ShelfID&) override {}
  void OnShelfItemMoved(const ShelfID&, int32_t) override {}
  void OnShelfItemUpdated(const ShelfItem&) override {}
  void OnShelfItemDelegateChanged(const ShelfID&,
                                  mojom::ShelfItemDelegatePtr) override {}

  std::vector<int64_t> shelf_initialized_display_ids_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfInitializationObserver);
};

}  // namespace

class ShelfTest : public AshTestBase {
 public:
  ShelfTest() : shelf_model_(nullptr) {}

  ~ShelfTest() override {}

  void SetUp() override {
    AshTestBase::SetUp();

    ShelfView* shelf_view = GetPrimaryShelf()->GetShelfViewForTesting();
    shelf_model_ = shelf_view->model();

    test_.reset(new ShelfViewTestAPI(shelf_view));
  }

  ShelfModel* shelf_model() { return shelf_model_; }

  ShelfViewTestAPI* test_api() { return test_.get(); }

 private:
  ShelfModel* shelf_model_;
  std::unique_ptr<ShelfViewTestAPI> test_;

  DISALLOW_COPY_AND_ASSIGN(ShelfTest);
};

// Confirms that ShelfItem reflects the appropriated state.
TEST_F(ShelfTest, StatusReflection) {
  // Initially we have the app list.
  int button_count = test_api()->GetButtonCount();

  // Add a running app.
  ShelfItem item;
  item.id = ShelfID("foo");
  item.type = TYPE_APP;
  item.status = STATUS_RUNNING;
  int index = shelf_model()->Add(item);
  ASSERT_EQ(++button_count, test_api()->GetButtonCount());
  ShelfButton* button = test_api()->GetButton(index);
  EXPECT_EQ(ShelfButton::STATE_RUNNING, button->state());

  // Remove it.
  shelf_model()->RemoveItemAt(index);
  ASSERT_EQ(--button_count, test_api()->GetButtonCount());
}

// Confirm that using the menu will clear the hover attribute. To avoid another
// browser test we check this here.
TEST_F(ShelfTest, CheckHoverAfterMenu) {
  // Initially we have the app list.
  int button_count = test_api()->GetButtonCount();

  // Add a running app.
  ShelfItem item;
  item.id = ShelfID("foo");
  item.type = TYPE_APP;
  item.status = STATUS_RUNNING;
  int index = shelf_model()->Add(item);

  ASSERT_EQ(++button_count, test_api()->GetButtonCount());
  ShelfButton* button = test_api()->GetButton(index);
  button->AddState(ShelfButton::STATE_HOVERED);
  button->ShowContextMenu(gfx::Point(), ui::MENU_SOURCE_MOUSE);
  EXPECT_FALSE(button->state() & ShelfButton::STATE_HOVERED);

  // Remove it.
  shelf_model()->RemoveItemAt(index);
}

TEST_F(ShelfTest, ShowOverflowBubble) {
  ShelfWidget* shelf_widget = GetPrimaryShelf()->shelf_widget();

  // Add app buttons until overflow occurs.
  ShelfItem item;
  item.type = TYPE_APP;
  item.status = STATUS_RUNNING;
  while (!test_api()->IsOverflowButtonVisible()) {
    item.id = ShelfID(base::IntToString(shelf_model()->item_count()));
    shelf_model()->Add(item);
    ASSERT_LT(shelf_model()->item_count(), 10000);
  }

  // Shows overflow bubble.
  test_api()->ShowOverflowBubble();
  EXPECT_TRUE(shelf_widget->IsShowingOverflowBubble());

  // Remove one of the first items in the main shelf view.
  ASSERT_GT(shelf_model()->item_count(), 1);
  shelf_model()->RemoveItemAt(1);

  // Waits for all transitions to finish and there should be no crash.
  test_api()->RunMessageLoopUntilAnimationsDone();
  EXPECT_FALSE(shelf_widget->IsShowingOverflowBubble());
}

// Verifies that shelves are re-initialized after display swap, which will
// reload their alignment prefs. http://crbug.com/748291
TEST_F(ShelfTest, ShelfInitializedOnDisplaySwap) {
  ShelfController* controller = Shell::Get()->shelf_controller();
  ShelfInitializationObserver observer;
  mojom::ShelfObserverAssociatedPtr observer_ptr;
  mojo::AssociatedBinding<mojom::ShelfObserver> binding(
      &observer, mojo::MakeIsolatedRequest(&observer_ptr));
  controller->AddObserver(observer_ptr.PassInterface());
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(0u, observer.shelf_initialized_display_ids_.size());

  // Simulate adding an external display at the lock screen. The shelf on the
  // new display is initialized.
  GetSessionControllerClient()->RequestLockScreen();
  UpdateDisplay("1024x768,800x600");
  base::RunLoop().RunUntilIdle();
  const int64_t internal_display_id = GetPrimaryDisplay().id();
  const int64_t external_display_id = GetSecondaryDisplay().id();
  ASSERT_EQ(1u, observer.shelf_initialized_display_ids_.size());
  EXPECT_EQ(external_display_id, observer.shelf_initialized_display_ids_[0]);
  observer.shelf_initialized_display_ids_.clear();

  // Simulate the external display becoming the primary display. Each shelf is
  // re-initialized.
  SwapPrimaryDisplay();
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(2u, observer.shelf_initialized_display_ids_.size());
  EXPECT_TRUE(base::ContainsValue(observer.shelf_initialized_display_ids_,
                                  internal_display_id));
  EXPECT_TRUE(base::ContainsValue(observer.shelf_initialized_display_ids_,
                                  external_display_id));

  // Simulate shelf state being set from prefs, which is how Chrome responds to
  // the initialization request.
  controller->SetAlignment(SHELF_ALIGNMENT_LEFT, internal_display_id);
  controller->SetAlignment(SHELF_ALIGNMENT_RIGHT, external_display_id);
  controller->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS,
                                  internal_display_id);
  controller->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS,
                                  external_display_id);

  // Shelf is still locked to bottom because screen is locked.
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM_LOCKED,
            GetShelfForDisplay(internal_display_id)->alignment());
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM_LOCKED,
            GetShelfForDisplay(external_display_id)->alignment());

  // After screen unlock all shelves should have an alignment.
  GetSessionControllerClient()->UnlockScreen();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SHELF_ALIGNMENT_LEFT,
            GetShelfForDisplay(internal_display_id)->alignment());
  EXPECT_EQ(SHELF_ALIGNMENT_RIGHT,
            GetShelfForDisplay(external_display_id)->alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS,
            GetShelfForDisplay(internal_display_id)->auto_hide_behavior());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS,
            GetShelfForDisplay(external_display_id)->auto_hide_behavior());
}

}  // namespace ash
