// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "ash/common/shelf/shelf_button.h"
#include "ash/common/shelf/shelf_model.h"
#include "ash/common/shelf/shelf_view.h"
#include "ash/common/shelf/shelf_widget.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/test/test_shelf_item_delegate.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/shelf_view_test_api.h"

namespace ash {

class ShelfTest : public test::AshTestBase {
 public:
  ShelfTest() : shelf_model_(nullptr) {}

  ~ShelfTest() override {}

  void SetUp() override {
    test::AshTestBase::SetUp();

    ShelfView* shelf_view = GetPrimaryShelf()->GetShelfViewForTesting();
    shelf_model_ = shelf_view->model();

    test_.reset(new test::ShelfViewTestAPI(shelf_view));
  }

  ShelfModel* shelf_model() { return shelf_model_; }

  test::ShelfViewTestAPI* test_api() { return test_.get(); }

 private:
  ShelfModel* shelf_model_;
  std::unique_ptr<test::ShelfViewTestAPI> test_;

  DISALLOW_COPY_AND_ASSIGN(ShelfTest);
};

// Confirms that ShelfItem reflects the appropriated state.
TEST_F(ShelfTest, StatusReflection) {
  // Initially we have the app list.
  int button_count = test_api()->GetButtonCount();

  // Add a running app.
  ShelfItem item;
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
  item.type = TYPE_APP;
  item.status = STATUS_RUNNING;
  int index = shelf_model()->Add(item);

  std::unique_ptr<ShelfItemDelegate> delegate(
      new test::TestShelfItemDelegate(NULL));
  shelf_model()->SetShelfItemDelegate(shelf_model()->items()[index].id,
                                      std::move(delegate));

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
  ShelfID first_item_id = shelf_model()->next_id();

  // Add app buttons until overflow occurs.
  int items_added = 0;
  while (!test_api()->IsOverflowButtonVisible()) {
    ShelfItem item;
    item.type = TYPE_APP;
    item.status = STATUS_RUNNING;
    shelf_model()->Add(item);

    ++items_added;
    ASSERT_LT(items_added, 10000);
  }

  // Shows overflow bubble.
  test_api()->ShowOverflowBubble();
  EXPECT_TRUE(shelf_widget->IsShowingOverflowBubble());

  // Removes the first item in main shelf view.
  shelf_model()->RemoveItemAt(shelf_model()->ItemIndexByID(first_item_id));

  // Waits for all transitions to finish and there should be no crash.
  test_api()->RunMessageLoopUntilAnimationsDone();
  EXPECT_FALSE(shelf_widget->IsShowingOverflowBubble());
}

}  // namespace ash
