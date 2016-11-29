// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/shelf/shelf_view.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "ash/common/shelf/app_list_button.h"
#include "ash/common/shelf/overflow_bubble.h"
#include "ash/common/shelf/overflow_bubble_view.h"
#include "ash/common/shelf/overflow_button.h"
#include "ash/common/shelf/shelf_button.h"
#include "ash/common/shelf/shelf_constants.h"
#include "ash/common/shelf/shelf_menu_model.h"
#include "ash/common/shelf/shelf_model.h"
#include "ash/common/shelf/shelf_tooltip_manager.h"
#include "ash/common/shelf/shelf_widget.h"
#include "ash/common/shelf/wm_shelf.h"
#include "ash/common/shelf/wm_shelf_observer.h"
#include "ash/common/system/web_notification/web_notification_tray.h"
#include "ash/common/test/material_design_controller_test_api.h"
#include "ash/common/test/test_shelf_delegate.h"
#include "ash/common/test/test_shelf_item_delegate.h"
#include "ash/common/test/test_system_tray_delegate.h"
#include "ash/common/wm_lookup.h"
#include "ash/common/wm_root_window_controller.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/overflow_bubble_view_test_api.h"
#include "ash/test/shelf_view_test_api.h"
#include "ash/test/test_shell_delegate.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/test/user_action_tester.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/app_list/presenter/app_list_presenter.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/test/ink_drop_host_view_test_api.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/view_model.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"

using testing::ElementsAre;
using testing::IsEmpty;

namespace ash {
namespace test {

////////////////////////////////////////////////////////////////////////////////
// WmShelfObserver::OnShelfIconPositionsChanged tests.

class TestWmShelfObserver : public WmShelfObserver {
 public:
  explicit TestWmShelfObserver(WmShelf* shelf) : shelf_(shelf) {
    shelf_->AddObserver(this);
  }

  ~TestWmShelfObserver() override { shelf_->RemoveObserver(this); }

  // WmShelfObserver implementation.
  void OnShelfIconPositionsChanged() override {
    icon_positions_changed_ = true;
  }

  bool icon_positions_changed() const { return icon_positions_changed_; }
  void Reset() { icon_positions_changed_ = false; }

 private:
  WmShelf* shelf_;
  bool icon_positions_changed_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestWmShelfObserver);
};

class WmShelfObserverIconTest : public AshTestBase {
 public:
  WmShelfObserverIconTest() {}
  ~WmShelfObserverIconTest() override {}

  void SetUp() override {
    AshTestBase::SetUp();
    observer_.reset(new TestWmShelfObserver(GetPrimaryShelf()));
    shelf_view_test_.reset(
        new ShelfViewTestAPI(GetPrimaryShelf()->GetShelfViewForTesting()));
    shelf_view_test_->SetAnimationDuration(1);
  }

  void TearDown() override {
    observer_.reset();
    AshTestBase::TearDown();
  }

  TestWmShelfObserver* observer() { return observer_.get(); }

  ShelfViewTestAPI* shelf_view_test() { return shelf_view_test_.get(); }

 private:
  std::unique_ptr<TestWmShelfObserver> observer_;
  std::unique_ptr<ShelfViewTestAPI> shelf_view_test_;

  DISALLOW_COPY_AND_ASSIGN(WmShelfObserverIconTest);
};

// TestShelfItemDelegate which tracks whether it gets selected.
class ShelfItemSelectionTracker : public TestShelfItemDelegate {
 public:
  ShelfItemSelectionTracker()
      : TestShelfItemDelegate(NULL),
        selected_(false),
        item_selected_action_(kNoAction) {}

  ~ShelfItemSelectionTracker() override {}

  // Resets to the initial state.
  void Reset() { selected_ = false; }

  void set_item_selected_action(
      ShelfItemDelegate::PerformedAction item_selected_action) {
    item_selected_action_ = item_selected_action;
  }

  // Returns true if the delegate was selected.
  bool WasSelected() { return selected_; }

  // TestShelfItemDelegate:
  ShelfItemDelegate::PerformedAction ItemSelected(
      const ui::Event& event) override {
    selected_ = true;
    return item_selected_action_;
  }

 private:
  bool selected_;

  // The action returned from ItemSelected(const ui::Event&).
  ShelfItemDelegate::PerformedAction item_selected_action_;

  DISALLOW_COPY_AND_ASSIGN(ShelfItemSelectionTracker);
};

TEST_F(WmShelfObserverIconTest, AddRemove) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = CurrentContext();
  views::Widget widget;
  widget.Init(params);

  TestShelfDelegate::instance()->AddShelfItem(
      WmLookup::Get()->GetWindowForWidget(&widget));
  shelf_view_test()->RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(observer()->icon_positions_changed());
  observer()->Reset();

  widget.Show();
  widget.GetNativeWindow()->parent()->RemoveChild(widget.GetNativeWindow());
  shelf_view_test()->RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(observer()->icon_positions_changed());
  observer()->Reset();
}

// Make sure creating/deleting an window on one displays notifies a
// shelf on external display as well as one on primary.
TEST_F(WmShelfObserverIconTest, AddRemoveWithMultipleDisplays) {
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("400x400,400x400");
  WmWindow* second_root = WmShell::Get()->GetAllRootWindows()[1];
  WmShelf* second_shelf = second_root->GetRootWindowController()->GetShelf();
  TestWmShelfObserver second_observer(second_shelf);

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = CurrentContext();
  views::Widget widget;
  widget.Init(params);

  TestShelfDelegate::instance()->AddShelfItem(
      WmLookup::Get()->GetWindowForWidget(&widget));
  shelf_view_test()->RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(observer()->icon_positions_changed());
  EXPECT_TRUE(second_observer.icon_positions_changed());
  observer()->Reset();
  second_observer.Reset();

  widget.GetNativeWindow()->parent()->RemoveChild(widget.GetNativeWindow());
  shelf_view_test()->RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(observer()->icon_positions_changed());
  EXPECT_TRUE(second_observer.icon_positions_changed());

  observer()->Reset();
  second_observer.Reset();
}

TEST_F(WmShelfObserverIconTest, BoundsChanged) {
  views::Widget* widget =
      GetPrimaryShelf()->GetShelfViewForTesting()->GetWidget();
  gfx::Rect shelf_bounds = widget->GetWindowBoundsInScreen();
  shelf_bounds.set_width(shelf_bounds.width() / 2);
  ASSERT_GT(shelf_bounds.width(), 0);
  widget->SetBounds(shelf_bounds);
  // No animation happens for ShelfView bounds change.
  EXPECT_TRUE(observer()->icon_positions_changed());
  observer()->Reset();
}

////////////////////////////////////////////////////////////////////////////////
// ShelfView tests.

// A ShelfDelegate test double that will always convert between ShelfIDs and app
// ids. This does not support pinning and unpinning operations and the return
// value of IsAppPinned(...) is configurable.
class TestShelfDelegateForShelfView : public ShelfDelegate {
 public:
  TestShelfDelegateForShelfView() {}
  ~TestShelfDelegateForShelfView() override {}

  void set_is_app_pinned(bool is_pinned) { is_app_pinned_ = is_pinned; }

  // ShelfDelegate overrides:
  ShelfID GetShelfIDForAppID(const std::string& app_id) override {
    ShelfID id = 0;
    EXPECT_TRUE(base::StringToInt(app_id, &id));
    return id;
  }

  ShelfID GetShelfIDForAppIDAndLaunchID(const std::string& app_id,
                                        const std::string& launch_id) override {
    return GetShelfIDForAppID(app_id);
  }

  bool HasShelfIDToAppIDMapping(ShelfID id) const override { return true; }

  const std::string& GetAppIDForShelfID(ShelfID id) override {
    // Use |app_id_| member variable because returning a reference to local
    // variable is not allowed.
    app_id_ = base::IntToString(id);
    return app_id_;
  }

  void PinAppWithID(const std::string& app_id) override { NOTREACHED(); }

  bool IsAppPinned(const std::string& app_id) override {
    return is_app_pinned_;
  }

  void UnpinAppWithID(const std::string& app_id) override { NOTREACHED(); }

 private:
  // Tracks whether apps are pinned or not.
  bool is_app_pinned_ = false;

  // Temp member variable for returning a value. See the comment in the
  // GetAppIDForShelfID().
  std::string app_id_;

  DISALLOW_COPY_AND_ASSIGN(TestShelfDelegateForShelfView);
};

class ShelfViewTest : public AshTestBase {
 public:
  static const char*
      kTimeBetweenWindowMinimizedAndActivatedActionsHistogramName;

  ShelfViewTest() : model_(nullptr), shelf_view_(nullptr), browser_index_(1) {}
  ~ShelfViewTest() override {}

  void SetUp() override {
    AshTestBase::SetUp();
    model_ = WmShell::Get()->shelf_model();
    shelf_view_ = GetPrimaryShelf()->GetShelfViewForTesting();

    WebNotificationTray::DisableAnimationsForTest(true);

    // The bounds should be big enough for 4 buttons + overflow chevron.
    shelf_view_->SetBounds(0, 0, 500, GetShelfConstant(SHELF_SIZE));

    test_api_.reset(new ShelfViewTestAPI(shelf_view_));
    test_api_->SetAnimationDuration(1);  // Speeds up animation for test.

    ReplaceShelfDelegate();

    // Add browser shortcut shelf item at index 0 for test.
    AddBrowserShortcut();
  }

  void TearDown() override {
    WebNotificationTray::DisableAnimationsForTest(false);  // Reenable animation

    shelf_delegate_ = nullptr;
    test_api_.reset();
    AshTestBase::TearDown();
  }

 protected:
  void CreateAndSetShelfItemDelegateForID(ShelfID id) {
    std::unique_ptr<ShelfItemDelegate> delegate(
        new TestShelfItemDelegate(NULL));
    model_->SetShelfItemDelegate(id, std::move(delegate));
  }

  ShelfID AddBrowserShortcut() {
    ShelfItem browser_shortcut;
    browser_shortcut.type = TYPE_BROWSER_SHORTCUT;

    ShelfID id = model_->next_id();
    model_->AddAt(browser_index_, browser_shortcut);
    CreateAndSetShelfItemDelegateForID(id);
    test_api_->RunMessageLoopUntilAnimationsDone();
    return id;
  }

  ShelfID AddAppShortcut() {
    ShelfItem item;
    item.type = TYPE_APP_SHORTCUT;
    item.status = STATUS_CLOSED;

    ShelfID id = model_->next_id();
    model_->Add(item);
    CreateAndSetShelfItemDelegateForID(id);
    test_api_->RunMessageLoopUntilAnimationsDone();
    return id;
  }

  ShelfID AddPanel() {
    ShelfID id = AddPanelNoWait();
    test_api_->RunMessageLoopUntilAnimationsDone();
    return id;
  }

  ShelfID AddAppNoWait() {
    ShelfItem item;
    item.type = TYPE_APP;
    item.status = STATUS_RUNNING;

    ShelfID id = model_->next_id();
    model_->Add(item);
    CreateAndSetShelfItemDelegateForID(id);
    return id;
  }

  ShelfID AddPanelNoWait() {
    ShelfItem item;
    item.type = TYPE_APP_PANEL;
    item.status = STATUS_RUNNING;

    ShelfID id = model_->next_id();
    model_->Add(item);
    CreateAndSetShelfItemDelegateForID(id);
    return id;
  }

  ShelfID AddApp() {
    ShelfID id = AddAppNoWait();
    test_api_->RunMessageLoopUntilAnimationsDone();
    return id;
  }

  void SetShelfItemTypeToAppShortcut(ShelfID id) {
    int index = model_->ItemIndexByID(id);
    DCHECK_GE(index, 0);

    ShelfItem item = model_->items()[index];

    if (item.type == TYPE_APP) {
      item.type = TYPE_APP_SHORTCUT;
      model_->Set(index, item);
    }
    test_api_->RunMessageLoopUntilAnimationsDone();
  }

  void RemoveByID(ShelfID id) {
    model_->RemoveItemAt(model_->ItemIndexByID(id));
    test_api_->RunMessageLoopUntilAnimationsDone();
  }

  ShelfButton* GetButtonByID(ShelfID id) {
    int index = model_->ItemIndexByID(id);
    return test_api_->GetButton(index);
  }

  ShelfItem GetItemByID(ShelfID id) {
    ShelfItems::const_iterator items = model_->ItemByID(id);
    return *items;
  }

  void CheckModelIDs(
      const std::vector<std::pair<ShelfID, views::View*>>& id_map) {
    size_t map_index = 0;
    for (size_t model_index = 0; model_index < model_->items().size();
         ++model_index) {
      ShelfItem item = model_->items()[model_index];
      ShelfID id = item.id;
      EXPECT_EQ(id_map[map_index].first, id);
      EXPECT_EQ(id_map[map_index].second, GetButtonByID(id));
      ++map_index;
    }
    ASSERT_EQ(map_index, id_map.size());
  }

  void VerifyShelfItemBoundsAreValid() {
    for (int i = 0; i <= test_api_->GetLastVisibleIndex(); ++i) {
      if (test_api_->GetButton(i)) {
        gfx::Rect shelf_view_bounds = shelf_view_->GetLocalBounds();
        gfx::Rect item_bounds = test_api_->GetBoundsByIndex(i);
        EXPECT_GE(item_bounds.x(), 0);
        EXPECT_GE(item_bounds.y(), 0);
        EXPECT_LE(item_bounds.right(), shelf_view_bounds.width());
        EXPECT_LE(item_bounds.bottom(), shelf_view_bounds.height());
      }
    }
  }

  ShelfButton* SimulateButtonPressed(ShelfView::Pointer pointer,
                                     int button_index) {
    ShelfButton* button = test_api_->GetButton(button_index);
    ui::MouseEvent click_event(ui::ET_MOUSE_PRESSED, gfx::Point(),
                               button->GetBoundsInScreen().origin(),
                               ui::EventTimeForNow(), 0, 0);
    shelf_view_->PointerPressedOnButton(button, pointer, click_event);
    return button;
  }

  // Simulates a single mouse click.
  void SimulateClick(int button_index) {
    ShelfButton* button = SimulateButtonPressed(ShelfView::MOUSE, button_index);
    ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, gfx::Point(),
                                 button->GetBoundsInScreen().origin(),
                                 ui::EventTimeForNow(), 0, 0);
    test_api_->ButtonPressed(
        button, release_event,
        views::test::InkDropHostViewTestApi(button).GetInkDrop());
    shelf_view_->PointerReleasedOnButton(button, ShelfView::MOUSE, false);
  }

  // Simulates the second click of a double click.
  void SimulateDoubleClick(int button_index) {
    ShelfButton* button = SimulateButtonPressed(ShelfView::MOUSE, button_index);
    ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, gfx::Point(),
                                 button->GetBoundsInScreen().origin(),
                                 ui::EventTimeForNow(), ui::EF_IS_DOUBLE_CLICK,
                                 0);
    test_api_->ButtonPressed(
        button, release_event,
        views::test::InkDropHostViewTestApi(button).GetInkDrop());
    shelf_view_->PointerReleasedOnButton(button, ShelfView::MOUSE, false);
  }

  void DoDrag(int dist_x,
              int dist_y,
              views::View* button,
              ShelfView::Pointer pointer,
              views::View* to) {
    ui::MouseEvent drag_event(ui::ET_MOUSE_DRAGGED, gfx::Point(dist_x, dist_y),
                              to->GetBoundsInScreen().origin(),
                              ui::EventTimeForNow(), 0, 0);
    shelf_view_->PointerDraggedOnButton(button, pointer, drag_event);
  }

  /*
   * Trigger ContinueDrag of the shelf
   * The argument progressively means whether to simulate the drag progress (a
   * series of changes of the posistion of dragged item), like the normal user
   * drag behavior.
   */
  void ContinueDrag(views::View* button,
                    ShelfView::Pointer pointer,
                    int from_index,
                    int to_index,
                    bool progressively) {
    views::View* to = test_api_->GetButton(to_index);
    views::View* from = test_api_->GetButton(from_index);
    int dist_x = to->x() - from->x();
    int dist_y = to->y() - from->y();
    if (progressively) {
      int sgn = dist_x > 0 ? 1 : -1;
      dist_x = abs(dist_x);
      for (; dist_x; dist_x -= std::min(10, dist_x))
        DoDrag(sgn * std::min(10, abs(dist_x)), 0, button, pointer, to);
    } else {
      DoDrag(dist_x, dist_y, button, pointer, to);
    }
  }

  /*
   * Simulate drag operation.
   * Argument progressively means whether to simulate the drag progress (a
   * series of changes of the posistion of dragged item) like the behavior of
   * user drags.
   */
  views::View* SimulateDrag(ShelfView::Pointer pointer,
                            int button_index,
                            int destination_index,
                            bool progressively) {
    views::View* button = SimulateButtonPressed(pointer, button_index);

    if (!progressively) {
      ContinueDrag(button, pointer, button_index, destination_index, false);
    } else if (button_index < destination_index) {
      for (int cur_index = button_index + 1; cur_index <= destination_index;
           cur_index++)
        ContinueDrag(button, pointer, cur_index - 1, cur_index, true);
    } else if (button_index > destination_index) {
      for (int cur_index = button_index - 1; cur_index >= destination_index;
           cur_index--)
        ContinueDrag(button, pointer, cur_index + 1, cur_index, true);
    }
    return button;
  }

  void DragAndVerify(
      int from,
      int to,
      ShelfView* shelf_view,
      const std::vector<std::pair<int, views::View*>>& expected_id_map) {
    views::View* dragged_button =
        SimulateDrag(ShelfView::MOUSE, from, to, true);
    shelf_view->PointerReleasedOnButton(dragged_button, ShelfView::MOUSE,
                                        false);
    test_api_->RunMessageLoopUntilAnimationsDone();
    ASSERT_NO_FATAL_FAILURE(CheckModelIDs(expected_id_map));
  }

  void SetupForDragTest(std::vector<std::pair<ShelfID, views::View*>>* id_map) {
    // Initialize |id_map| with the automatically-created shelf buttons.
    for (size_t i = 0; i < model_->items().size(); ++i) {
      ShelfButton* button = test_api_->GetButton(i);
      id_map->push_back(std::make_pair(model_->items()[i].id, button));
    }
    ASSERT_NO_FATAL_FAILURE(CheckModelIDs(*id_map));

    // Add 5 app shelf buttons for testing.
    for (int i = 0; i < 5; ++i) {
      ShelfID id = AddAppShortcut();
      // App Icon is located at index 0, and browser shortcut is located at
      // index 1. So we should start to add app shortcut at index 2.
      id_map->insert(id_map->begin() + (i + browser_index_ + 1),
                     std::make_pair(id, GetButtonByID(id)));
    }
    ASSERT_NO_FATAL_FAILURE(CheckModelIDs(*id_map));
  }

  void AddButtonsUntilOverflow() {
    int items_added = 0;
    while (!test_api_->IsOverflowButtonVisible()) {
      AddAppShortcut();
      ++items_added;
      ASSERT_LT(items_added, 10000);
    }
  }

  void TestDraggingAnItemFromOverflowToShelf(bool cancel) {
    test_api_->ShowOverflowBubble();
    ASSERT_TRUE(test_api_->IsShowingOverflowBubble());

    ShelfViewTestAPI test_api_for_overflow(
        test_api_->overflow_bubble()->shelf_view());

    int total_item_count = model_->item_count();

    int last_visible_item_id_in_shelf =
        GetItemId(test_api_->GetLastVisibleIndex());
    int second_last_visible_item_id_in_shelf =
        GetItemId(test_api_->GetLastVisibleIndex() - 1);
    int first_visible_item_id_in_overflow =
        GetItemId(test_api_for_overflow.GetFirstVisibleIndex());
    int second_last_visible_item_id_in_overflow =
        GetItemId(test_api_for_overflow.GetLastVisibleIndex() - 1);

    int drag_item_index = test_api_for_overflow.GetLastVisibleIndex();
    ShelfID drag_item_id = GetItemId(drag_item_index);
    ShelfButton* drag_button = test_api_for_overflow.GetButton(drag_item_index);
    gfx::Point center_point_of_drag_item =
        drag_button->GetBoundsInScreen().CenterPoint();

    ui::test::EventGenerator& generator = GetEventGenerator();
    generator.set_current_location(center_point_of_drag_item);
    // Rip an item off to OverflowBubble.
    generator.PressLeftButton();
    gfx::Point rip_off_point(center_point_of_drag_item.x(), 0);
    generator.MoveMouseTo(rip_off_point);
    test_api_for_overflow.RunMessageLoopUntilAnimationsDone();
    ASSERT_TRUE(test_api_for_overflow.IsRippedOffFromShelf());
    ASSERT_FALSE(test_api_for_overflow.DraggedItemFromOverflowToShelf());

    // Move a dragged item into Shelf at |drop_index|.
    int drop_index = 1;
    gfx::Point drop_point =
        test_api_->GetButton(drop_index)->GetBoundsInScreen().CenterPoint();
    int item_width = test_api_for_overflow.GetButtonSize();
    // To insert at |drop_index|, more smaller x-axis value of |drop_point|
    // should be used.
    gfx::Point modified_drop_point(drop_point.x() - item_width / 4,
                                   drop_point.y());
    generator.MoveMouseTo(modified_drop_point);
    test_api_for_overflow.RunMessageLoopUntilAnimationsDone();
    test_api_->RunMessageLoopUntilAnimationsDone();
    ASSERT_TRUE(test_api_for_overflow.IsRippedOffFromShelf());
    ASSERT_TRUE(test_api_for_overflow.DraggedItemFromOverflowToShelf());

    if (cancel)
      drag_button->OnMouseCaptureLost();
    else
      generator.ReleaseLeftButton();

    test_api_for_overflow.RunMessageLoopUntilAnimationsDone();
    test_api_->RunMessageLoopUntilAnimationsDone();
    ASSERT_FALSE(test_api_for_overflow.IsRippedOffFromShelf());
    ASSERT_FALSE(test_api_for_overflow.DraggedItemFromOverflowToShelf());

    // Compare pre-stored items' id with newly positioned items' after dragging
    // is canceled or finished.
    if (cancel) {
      EXPECT_EQ(last_visible_item_id_in_shelf,
                GetItemId(test_api_->GetLastVisibleIndex()));
      EXPECT_EQ(second_last_visible_item_id_in_shelf,
                GetItemId(test_api_->GetLastVisibleIndex() - 1));
      EXPECT_EQ(first_visible_item_id_in_overflow,
                GetItemId(test_api_for_overflow.GetFirstVisibleIndex()));
      EXPECT_EQ(second_last_visible_item_id_in_overflow,
                GetItemId(test_api_for_overflow.GetLastVisibleIndex() - 1));
    } else {
      EXPECT_EQ(drag_item_id, GetItemId(drop_index));
      EXPECT_EQ(total_item_count, model_->item_count());
      EXPECT_EQ(last_visible_item_id_in_shelf,
                GetItemId(test_api_for_overflow.GetFirstVisibleIndex()));
      EXPECT_EQ(second_last_visible_item_id_in_shelf,
                GetItemId(test_api_->GetLastVisibleIndex()));
      EXPECT_EQ(first_visible_item_id_in_overflow,
                GetItemId(test_api_for_overflow.GetFirstVisibleIndex() + 1));
      EXPECT_EQ(second_last_visible_item_id_in_overflow,
                GetItemId(test_api_for_overflow.GetLastVisibleIndex()));
    }
    test_api_->HideOverflowBubble();
  }

  // Returns the item's ShelfID at |index|.
  ShelfID GetItemId(int index) {
    DCHECK_GE(index, 0);
    return model_->items()[index].id;
  }

  void ReplaceShelfDelegate() {
    shelf_delegate_ = new TestShelfDelegateForShelfView();
    test_api_->SetShelfDelegate(shelf_delegate_);
    WmShell::Get()->SetShelfDelegateForTesting(
        base::WrapUnique(shelf_delegate_));
  }

  ShelfModel* model_;
  ShelfView* shelf_view_;
  int browser_index_;

  // Owned by ash::WmShell.
  TestShelfDelegateForShelfView* shelf_delegate_;

  std::unique_ptr<ShelfViewTestAPI> test_api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfViewTest);
};

const char*
    ShelfViewTest::kTimeBetweenWindowMinimizedAndActivatedActionsHistogramName =
        ShelfButtonPressedMetricTracker::
            kTimeBetweenWindowMinimizedAndActivatedActionsHistogramName;

class ScopedTextDirectionChange {
 public:
  explicit ScopedTextDirectionChange(bool is_rtl) : is_rtl_(is_rtl) {
    original_locale_ = base::i18n::GetConfiguredLocale();
    if (is_rtl_)
      base::i18n::SetICUDefaultLocale("he");
    CheckTextDirectionIsCorrect();
  }

  ~ScopedTextDirectionChange() {
    if (is_rtl_)
      base::i18n::SetICUDefaultLocale(original_locale_);
  }

 private:
  void CheckTextDirectionIsCorrect() {
    ASSERT_EQ(is_rtl_, base::i18n::IsRTL());
  }

  bool is_rtl_;
  std::string original_locale_;
};

class ShelfViewTextDirectionTest : public ShelfViewTest,
                                   public testing::WithParamInterface<bool> {
 public:
  ShelfViewTextDirectionTest() : text_direction_change_(GetParam()) {}
  virtual ~ShelfViewTextDirectionTest() {}

  void SetUp() override { ShelfViewTest::SetUp(); }

  void TearDown() override { ShelfViewTest::TearDown(); }

 private:
  ScopedTextDirectionChange text_direction_change_;

  DISALLOW_COPY_AND_ASSIGN(ShelfViewTextDirectionTest);
};

// Checks that the ideal item icon bounds match the view's bounds in the screen
// in both LTR and RTL.
TEST_P(ShelfViewTextDirectionTest, IdealBoundsOfItemIcon) {
  ShelfID id = AddApp();
  ShelfButton* button = GetButtonByID(id);
  gfx::Rect item_bounds = button->GetBoundsInScreen();
  gfx::Point icon_offset = button->GetIconBounds().origin();
  item_bounds.Offset(icon_offset.OffsetFromOrigin());
  gfx::Rect ideal_bounds = shelf_view_->GetIdealBoundsOfItemIcon(id);
  gfx::Point screen_origin;
  views::View::ConvertPointToScreen(shelf_view_, &screen_origin);
  ideal_bounds.Offset(screen_origin.x(), screen_origin.y());
  EXPECT_EQ(item_bounds.x(), ideal_bounds.x());
  EXPECT_EQ(item_bounds.y(), ideal_bounds.y());
}

// Check that items in the overflow area are returning the overflow button as
// ideal bounds.
TEST_F(ShelfViewTest, OverflowButtonBounds) {
  ShelfID first_id = AddApp();
  ShelfID overflow_id = AddApp();
  int items_added = 0;
  while (!test_api_->IsOverflowButtonVisible()) {
    // Added button is visible after animation while in this loop.
    EXPECT_TRUE(GetButtonByID(overflow_id)->visible());
    overflow_id = AddApp();
    ++items_added;
    ASSERT_LT(items_added, 10000);
  }
  ShelfID last_id = AddApp();

  gfx::Rect first_bounds = shelf_view_->GetIdealBoundsOfItemIcon(first_id);
  gfx::Rect overflow_bounds =
      shelf_view_->GetIdealBoundsOfItemIcon(overflow_id);
  gfx::Rect last_bounds = shelf_view_->GetIdealBoundsOfItemIcon(last_id);

  // Check that all items have the same size and that the overflow items are
  // identical whereas the first one does not match either of them.
  EXPECT_EQ(first_bounds.size().ToString(), last_bounds.size().ToString());
  EXPECT_NE(first_bounds.ToString(), last_bounds.ToString());
  EXPECT_EQ(overflow_bounds.ToString(), last_bounds.ToString());
}

// Checks that shelf view contents are considered in the correct drag group.
TEST_F(ShelfViewTest, EnforceDragType) {
  EXPECT_TRUE(test_api_->SameDragType(TYPE_APP, TYPE_APP));
  EXPECT_FALSE(test_api_->SameDragType(TYPE_APP, TYPE_APP_SHORTCUT));
  EXPECT_FALSE(test_api_->SameDragType(TYPE_APP, TYPE_BROWSER_SHORTCUT));
  EXPECT_FALSE(test_api_->SameDragType(TYPE_APP, TYPE_APP_LIST));
  EXPECT_FALSE(test_api_->SameDragType(TYPE_APP, TYPE_APP_PANEL));

  EXPECT_TRUE(test_api_->SameDragType(TYPE_APP_SHORTCUT, TYPE_APP_SHORTCUT));
  EXPECT_TRUE(
      test_api_->SameDragType(TYPE_APP_SHORTCUT, TYPE_BROWSER_SHORTCUT));
  EXPECT_FALSE(test_api_->SameDragType(TYPE_APP_SHORTCUT, TYPE_APP_LIST));
  EXPECT_FALSE(test_api_->SameDragType(TYPE_APP_SHORTCUT, TYPE_APP_PANEL));

  EXPECT_TRUE(
      test_api_->SameDragType(TYPE_BROWSER_SHORTCUT, TYPE_BROWSER_SHORTCUT));
  EXPECT_FALSE(test_api_->SameDragType(TYPE_BROWSER_SHORTCUT, TYPE_APP_LIST));
  EXPECT_FALSE(test_api_->SameDragType(TYPE_BROWSER_SHORTCUT, TYPE_APP_PANEL));

  EXPECT_TRUE(test_api_->SameDragType(TYPE_APP_LIST, TYPE_APP_LIST));
  EXPECT_FALSE(test_api_->SameDragType(TYPE_APP_LIST, TYPE_APP_PANEL));

  EXPECT_TRUE(test_api_->SameDragType(TYPE_APP_PANEL, TYPE_APP_PANEL));
}

// Adds platform app button until overflow and verifies that the last added
// platform app button is hidden.
TEST_F(ShelfViewTest, AddBrowserUntilOverflow) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetButtonCount(), test_api_->GetLastVisibleIndex() + 1);

  // Add platform app button until overflow.
  int items_added = 0;
  ShelfID last_added = AddApp();
  while (!test_api_->IsOverflowButtonVisible()) {
    // Added button is visible after animation while in this loop.
    EXPECT_TRUE(GetButtonByID(last_added)->visible());

    last_added = AddApp();
    ++items_added;
    ASSERT_LT(items_added, 10000);
  }

  // The last added button should be invisible.
  EXPECT_FALSE(GetButtonByID(last_added)->visible());
}

// Adds one platform app button then adds app shortcut until overflow. Verifies
// that the browser button gets hidden on overflow and last added app shortcut
// is still visible.
TEST_F(ShelfViewTest, AddAppShortcutWithBrowserButtonUntilOverflow) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetButtonCount(), test_api_->GetLastVisibleIndex() + 1);

  ShelfID browser_button_id = AddApp();

  // Add app shortcut until overflow.
  int items_added = 0;
  ShelfID last_added = AddAppShortcut();
  while (!test_api_->IsOverflowButtonVisible()) {
    // Added button is visible after animation while in this loop.
    EXPECT_TRUE(GetButtonByID(last_added)->visible());

    last_added = AddAppShortcut();
    ++items_added;
    ASSERT_LT(items_added, 10000);
  }

  // And the platform app button is invisible.
  EXPECT_FALSE(GetButtonByID(browser_button_id)->visible());
}

TEST_F(ShelfViewTest, AddPanelHidesPlatformAppButton) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetButtonCount(), test_api_->GetLastVisibleIndex() + 1);

  // Add platform app button until overflow, remember last visible platform app
  // button.
  int items_added = 0;
  ShelfID first_added = AddApp();
  EXPECT_TRUE(GetButtonByID(first_added)->visible());
  while (true) {
    ShelfID added = AddApp();
    if (test_api_->IsOverflowButtonVisible()) {
      EXPECT_FALSE(GetButtonByID(added)->visible());
      RemoveByID(added);
      break;
    }
    ++items_added;
    ASSERT_LT(items_added, 10000);
  }

  ShelfID panel = AddPanel();
  EXPECT_TRUE(test_api_->IsOverflowButtonVisible());

  RemoveByID(panel);
  EXPECT_FALSE(test_api_->IsOverflowButtonVisible());
}

// When there are more panels then platform app buttons we should hide panels
// rather than platform apps.
TEST_F(ShelfViewTest, PlatformAppHidesExcessPanels) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetButtonCount(), test_api_->GetLastVisibleIndex() + 1);

  // Add platform app button.
  ShelfID platform_app = AddApp();
  ShelfID first_panel = AddPanel();

  EXPECT_TRUE(GetButtonByID(platform_app)->visible());
  EXPECT_TRUE(GetButtonByID(first_panel)->visible());

  // Add panels until there is an overflow.
  ShelfID last_panel = first_panel;
  int items_added = 0;
  while (!test_api_->IsOverflowButtonVisible()) {
    last_panel = AddPanel();
    ++items_added;
    ASSERT_LT(items_added, 10000);
  }

  // The first panel should now be hidden by the new platform apps needing
  // space.
  EXPECT_FALSE(GetButtonByID(first_panel)->visible());
  EXPECT_TRUE(GetButtonByID(last_panel)->visible());
  EXPECT_TRUE(GetButtonByID(platform_app)->visible());

  // Adding platform apps should eventually begin to hide platform apps. We will
  // add platform apps until either the last panel or platform app is hidden.
  items_added = 0;
  while (GetButtonByID(platform_app)->visible() &&
         GetButtonByID(last_panel)->visible()) {
    platform_app = AddApp();
    ++items_added;
    ASSERT_LT(items_added, 10000);
  }
  EXPECT_TRUE(GetButtonByID(last_panel)->visible());
  EXPECT_FALSE(GetButtonByID(platform_app)->visible());
}

// Making sure that no buttons on the shelf will ever overlap after adding many
// of them.
TEST_F(ShelfViewTest, AssertNoButtonsOverlap) {
  std::vector<ShelfID> button_ids;
  // Add app icons until the overflow button is visible.
  while (!test_api_->IsOverflowButtonVisible()) {
    ShelfID id = AddApp();
    button_ids.push_back(id);
  }
  ASSERT_LT(button_ids.size(), 10000U);
  ASSERT_GT(button_ids.size(), 2U);

  // Remove 2 icons to make more room for panel icons, the overflow button
  // should go away.
  for (int i = 0; i < 2; ++i) {
    ShelfID id = button_ids.back();
    RemoveByID(id);
    button_ids.pop_back();
  }
  EXPECT_FALSE(test_api_->IsOverflowButtonVisible());
  EXPECT_TRUE(GetButtonByID(button_ids.back())->visible());

  // Add 20 panel icons, and expect to have overflow.
  for (int i = 0; i < 20; ++i) {
    ShelfID id = AddPanel();
    button_ids.push_back(id);
  }
  ASSERT_LT(button_ids.size(), 10000U);
  EXPECT_TRUE(test_api_->IsOverflowButtonVisible());

  // Test that any two successive visible icons never overlap in all shelf
  // alignment types.
  const ShelfAlignment kAlignments[] = {
      SHELF_ALIGNMENT_LEFT, SHELF_ALIGNMENT_RIGHT, SHELF_ALIGNMENT_BOTTOM,
      SHELF_ALIGNMENT_BOTTOM_LOCKED,
  };

  for (ShelfAlignment alignment : kAlignments) {
    shelf_view_->wm_shelf()->SetAlignment(alignment);
    // For every 2 successive visible icons, expect that their bounds don't
    // intersect.
    for (int i = 1; i < test_api_->GetButtonCount() - 1; ++i) {
      if (!(test_api_->GetButton(i)->visible() &&
            test_api_->GetButton(i + 1)->visible())) {
        continue;
      }

      const gfx::Rect& bounds1 = test_api_->GetBoundsByIndex(i);
      const gfx::Rect& bounds2 = test_api_->GetBoundsByIndex(i + 1);
      EXPECT_FALSE(bounds1.Intersects(bounds2));
    }
  }
}

// Making sure the overflow bubble arrow correctly tracks with shelf position.
TEST_P(ShelfViewTextDirectionTest, OverflowArrowForShelfPosition) {
  const ShelfAlignment kAlignments[] = {
      SHELF_ALIGNMENT_BOTTOM, SHELF_ALIGNMENT_LEFT, SHELF_ALIGNMENT_RIGHT,
      SHELF_ALIGNMENT_BOTTOM_LOCKED,
  };

  // These must match what is expected for each alignment above.
  const views::BubbleBorder::Arrow kArrows[] = {
      views::BubbleBorder::BOTTOM_LEFT, views::BubbleBorder::LEFT_TOP,
      views::BubbleBorder::RIGHT_TOP, views::BubbleBorder::BOTTOM_LEFT,
  };

  for (size_t i = 0; i < arraysize(kAlignments); i++) {
    shelf_view_->wm_shelf()->SetAlignment(kAlignments[i]);

    // Make sure there are enough icons to trigger the overflow in new
    // orientation.
    AddButtonsUntilOverflow();
    test_api_->ShowOverflowBubble();
    ASSERT_TRUE(test_api_->IsShowingOverflowBubble());

    EXPECT_EQ(kArrows[i], test_api_->overflow_bubble()->bubble_view()->arrow());
    OverflowBubbleViewTestAPI bubble_view_api(
        test_api_->overflow_bubble()->bubble_view());
    EXPECT_EQ(kArrows[i],
              bubble_view_api.GetBubbleFrameView()->bubble_border()->arrow());
  }
}

// Adds button until overflow then removes first added one. Verifies that
// the last added one changes from invisible to visible and overflow
// chevron is gone.
TEST_F(ShelfViewTest, RemoveButtonRevealsOverflowed) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetButtonCount(), test_api_->GetLastVisibleIndex() + 1);

  // Add platform app buttons until overflow.
  int items_added = 0;
  ShelfID first_added = AddApp();
  ShelfID last_added = first_added;
  while (!test_api_->IsOverflowButtonVisible()) {
    last_added = AddApp();
    ++items_added;
    ASSERT_LT(items_added, 10000);
  }

  // Expect add more than 1 button. First added is visible and last is not.
  EXPECT_NE(first_added, last_added);
  EXPECT_TRUE(GetButtonByID(first_added)->visible());
  EXPECT_FALSE(GetButtonByID(last_added)->visible());

  // Remove first added.
  RemoveByID(first_added);

  // Last added button becomes visible and overflow chevron is gone.
  EXPECT_TRUE(GetButtonByID(last_added)->visible());
  EXPECT_EQ(1.0f, GetButtonByID(last_added)->layer()->opacity());
  EXPECT_FALSE(test_api_->IsOverflowButtonVisible());
}

// Verifies that remove last overflowed button should hide overflow chevron.
TEST_F(ShelfViewTest, RemoveLastOverflowed) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetButtonCount(), test_api_->GetLastVisibleIndex() + 1);

  // Add platform app button until overflow.
  int items_added = 0;
  ShelfID last_added = AddApp();
  while (!test_api_->IsOverflowButtonVisible()) {
    last_added = AddApp();
    ++items_added;
    ASSERT_LT(items_added, 10000);
  }

  RemoveByID(last_added);
  EXPECT_FALSE(test_api_->IsOverflowButtonVisible());
}

// Adds platform app button without waiting for animation to finish and verifies
// that all added buttons are visible.
TEST_F(ShelfViewTest, AddButtonQuickly) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetButtonCount(), test_api_->GetLastVisibleIndex() + 1);

  // Add a few platform buttons quickly without wait for animation.
  int added_count = 0;
  while (!test_api_->IsOverflowButtonVisible()) {
    AddAppNoWait();
    ++added_count;
    ASSERT_LT(added_count, 10000);
  }

  // ShelfView should be big enough to hold at least 3 new buttons.
  ASSERT_GE(added_count, 3);

  // Wait for the last animation to finish.
  test_api_->RunMessageLoopUntilAnimationsDone();

  // Verifies non-overflow buttons are visible.
  for (int i = 0; i <= test_api_->GetLastVisibleIndex(); ++i) {
    ShelfButton* button = test_api_->GetButton(i);
    if (button) {
      EXPECT_TRUE(button->visible()) << "button index=" << i;
      EXPECT_EQ(1.0f, button->layer()->opacity()) << "button index=" << i;
    }
  }
}

// Check that model changes are handled correctly while a shelf icon is being
// dragged.
TEST_F(ShelfViewTest, ModelChangesWhileDragging) {
  std::vector<std::pair<ShelfID, views::View*>> id_map;
  SetupForDragTest(&id_map);

  // Dragging browser shortcut at index 1.
  EXPECT_TRUE(model_->items()[1].type == TYPE_BROWSER_SHORTCUT);
  views::View* dragged_button = SimulateDrag(ShelfView::MOUSE, 1, 3, false);
  std::rotate(id_map.begin() + 1, id_map.begin() + 2, id_map.begin() + 4);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  shelf_view_->PointerReleasedOnButton(dragged_button, ShelfView::MOUSE, false);
  EXPECT_TRUE(model_->items()[3].type == TYPE_BROWSER_SHORTCUT);

  // Dragging changes model order.
  dragged_button = SimulateDrag(ShelfView::MOUSE, 1, 3, false);
  std::rotate(id_map.begin() + 1, id_map.begin() + 2, id_map.begin() + 4);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // Cancelling the drag operation restores previous order.
  shelf_view_->PointerReleasedOnButton(dragged_button, ShelfView::MOUSE, true);
  std::rotate(id_map.begin() + 1, id_map.begin() + 3, id_map.begin() + 4);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // Deleting an item keeps the remaining intact.
  dragged_button = SimulateDrag(ShelfView::MOUSE, 1, 3, false);
  model_->RemoveItemAt(1);
  id_map.erase(id_map.begin() + 1);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  shelf_view_->PointerReleasedOnButton(dragged_button, ShelfView::MOUSE, false);

  // Adding a shelf item cancels the drag and respects the order.
  dragged_button = SimulateDrag(ShelfView::MOUSE, 1, 3, false);
  ShelfID new_id = AddAppShortcut();
  id_map.insert(id_map.begin() + 6,
                std::make_pair(new_id, GetButtonByID(new_id)));
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  shelf_view_->PointerReleasedOnButton(dragged_button, ShelfView::MOUSE, false);

  // Adding a shelf item at the end (i.e. a panel)  canels drag and respects
  // the order.
  dragged_button = SimulateDrag(ShelfView::MOUSE, 1, 3, false);
  new_id = AddPanel();
  id_map.insert(id_map.begin() + 7,
                std::make_pair(new_id, GetButtonByID(new_id)));
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  shelf_view_->PointerReleasedOnButton(dragged_button, ShelfView::MOUSE, false);
}

// Check that 2nd drag from the other pointer would be ignored.
TEST_F(ShelfViewTest, SimultaneousDrag) {
  std::vector<std::pair<ShelfID, views::View*>> id_map;
  SetupForDragTest(&id_map);

  // Start a mouse drag.
  views::View* dragged_button_mouse =
      SimulateDrag(ShelfView::MOUSE, 1, 3, false);
  std::rotate(id_map.begin() + 1, id_map.begin() + 2, id_map.begin() + 4);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  // Attempt a touch drag before the mouse drag finishes.
  views::View* dragged_button_touch =
      SimulateDrag(ShelfView::TOUCH, 4, 2, false);

  // Nothing changes since 2nd drag is ignored.
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // Finish the mouse drag.
  shelf_view_->PointerReleasedOnButton(dragged_button_mouse, ShelfView::MOUSE,
                                       false);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // Now start a touch drag.
  dragged_button_touch = SimulateDrag(ShelfView::TOUCH, 4, 2, false);
  std::rotate(id_map.begin() + 3, id_map.begin() + 4, id_map.begin() + 5);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // And attempt a mouse drag before the touch drag finishes.
  dragged_button_mouse = SimulateDrag(ShelfView::MOUSE, 1, 2, false);

  // Nothing changes since 2nd drag is ignored.
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  shelf_view_->PointerReleasedOnButton(dragged_button_touch, ShelfView::TOUCH,
                                       false);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
}

// Check that whether the ash behaves correctly if not draggable
// item are in front of the shelf.
TEST_F(ShelfViewTest, DragWithNotDraggableItemInFront) {
  std::vector<std::pair<ShelfID, views::View*>> id_map;
  SetupForDragTest(&id_map);

  (static_cast<TestShelfItemDelegate*>(
       model_->GetShelfItemDelegate(id_map[1].first)))
      ->set_is_draggable(false);
  (static_cast<TestShelfItemDelegate*>(
       model_->GetShelfItemDelegate(id_map[2].first)))
      ->set_is_draggable(false);

  ASSERT_NO_FATAL_FAILURE(DragAndVerify(3, 1, shelf_view_, id_map));
  ASSERT_NO_FATAL_FAILURE(DragAndVerify(3, 2, shelf_view_, id_map));

  std::rotate(id_map.begin() + 3, id_map.begin() + 4, id_map.begin() + 5);
  ASSERT_NO_FATAL_FAILURE(DragAndVerify(4, 1, shelf_view_, id_map));
  std::rotate(id_map.begin() + 3, id_map.begin() + 5, id_map.begin() + 6);
  ASSERT_NO_FATAL_FAILURE(DragAndVerify(5, 1, shelf_view_, id_map));
}

// Check that clicking first on one item and then dragging another works as
// expected.
TEST_F(ShelfViewTest, ClickOneDragAnother) {
  std::vector<std::pair<ShelfID, views::View*>> id_map;
  SetupForDragTest(&id_map);

  // A click on item 1 is simulated.
  SimulateClick(1);

  // Dragging browser index at 0 should change the model order correctly.
  EXPECT_TRUE(model_->items()[1].type == TYPE_BROWSER_SHORTCUT);
  views::View* dragged_button = SimulateDrag(ShelfView::MOUSE, 1, 3, false);
  std::rotate(id_map.begin() + 1, id_map.begin() + 2, id_map.begin() + 4);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  shelf_view_->PointerReleasedOnButton(dragged_button, ShelfView::MOUSE, false);
  EXPECT_TRUE(model_->items()[3].type == TYPE_BROWSER_SHORTCUT);
}

// Tests that double-clicking an item does not activate it twice.
TEST_F(ShelfViewTest, ClickingTwiceActivatesOnce) {
  // Watch for selection of the browser shortcut.
  ShelfID browser_shelf_id = model_->items()[browser_index_].id;
  ShelfItemSelectionTracker* selection_tracker = new ShelfItemSelectionTracker;
  model_->SetShelfItemDelegate(
      browser_shelf_id, std::unique_ptr<ShelfItemDelegate>(selection_tracker));

  // A single click selects the item.
  SimulateClick(browser_index_);
  EXPECT_TRUE(selection_tracker->WasSelected());

  // A double-click does not select the item.
  selection_tracker->Reset();
  SimulateDoubleClick(browser_index_);
  EXPECT_FALSE(selection_tracker->WasSelected());
}

// Check that clicking an item and jittering the mouse a bit still selects the
// item.
TEST_F(ShelfViewTest, ClickAndMoveSlightly) {
  std::vector<std::pair<ShelfID, views::View*>> id_map;
  SetupForDragTest(&id_map);

  ShelfID shelf_id = (id_map.begin() + 1)->first;
  views::View* button = (id_map.begin() + 1)->second;

  // Replace the ShelfItemDelegate for |shelf_id| with one which tracks whether
  // the shelf item gets selected.
  ShelfItemSelectionTracker* selection_tracker = new ShelfItemSelectionTracker;
  model_->SetShelfItemDelegate(
      shelf_id, std::unique_ptr<ShelfItemDelegate>(selection_tracker));

  gfx::Vector2d press_offset(5, 30);
  gfx::Point press_location = gfx::Point() + press_offset;
  gfx::Point press_location_in_screen =
      button->GetBoundsInScreen().origin() + press_offset;

  ui::MouseEvent click_event(ui::ET_MOUSE_PRESSED, press_location,
                             press_location_in_screen, ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMousePressed(click_event);

  ui::MouseEvent drag_event1(
      ui::ET_MOUSE_DRAGGED, press_location + gfx::Vector2d(0, 1),
      press_location_in_screen + gfx::Vector2d(0, 1), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseDragged(drag_event1);

  ui::MouseEvent drag_event2(
      ui::ET_MOUSE_DRAGGED, press_location + gfx::Vector2d(-1, 0),
      press_location_in_screen + gfx::Vector2d(-1, 0), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseDragged(drag_event2);

  ui::MouseEvent release_event(
      ui::ET_MOUSE_RELEASED, press_location + gfx::Vector2d(-1, 0),
      press_location_in_screen + gfx::Vector2d(-1, 0), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseReleased(release_event);

  EXPECT_TRUE(selection_tracker->WasSelected());
}

// Confirm that item status changes are reflected in the buttons.
TEST_F(ShelfViewTest, ShelfItemStatus) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetButtonCount(), test_api_->GetLastVisibleIndex() + 1);

  // Add platform app button.
  ShelfID last_added = AddApp();
  ShelfItem item = GetItemByID(last_added);
  int index = model_->ItemIndexByID(last_added);
  ShelfButton* button = GetButtonByID(last_added);
  ASSERT_EQ(ShelfButton::STATE_RUNNING, button->state());
  item.status = STATUS_ACTIVE;
  model_->Set(index, item);
  ASSERT_EQ(ShelfButton::STATE_ACTIVE, button->state());
  item.status = STATUS_ATTENTION;
  model_->Set(index, item);
  ASSERT_EQ(ShelfButton::STATE_ATTENTION, button->state());
}

// Confirm that item status changes are reflected in the buttons
// for platform apps.
TEST_F(ShelfViewTest, ShelfItemStatusPlatformApp) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetButtonCount(), test_api_->GetLastVisibleIndex() + 1);

  // Add platform app button.
  ShelfID last_added = AddApp();
  ShelfItem item = GetItemByID(last_added);
  int index = model_->ItemIndexByID(last_added);
  ShelfButton* button = GetButtonByID(last_added);
  ASSERT_EQ(ShelfButton::STATE_RUNNING, button->state());
  item.status = STATUS_ACTIVE;
  model_->Set(index, item);
  ASSERT_EQ(ShelfButton::STATE_ACTIVE, button->state());
  item.status = STATUS_ATTENTION;
  model_->Set(index, item);
  ASSERT_EQ(ShelfButton::STATE_ATTENTION, button->state());
}

// Confirm that shelf item bounds are correctly updated on shelf changes.
TEST_F(ShelfViewTest, ShelfItemBoundsCheck) {
  VerifyShelfItemBoundsAreValid();
  shelf_view_->wm_shelf()->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  test_api_->RunMessageLoopUntilAnimationsDone();
  VerifyShelfItemBoundsAreValid();
  shelf_view_->wm_shelf()->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  test_api_->RunMessageLoopUntilAnimationsDone();
  VerifyShelfItemBoundsAreValid();
}

TEST_F(ShelfViewTest, ShelfTooltipTest) {
  ASSERT_EQ(test_api_->GetLastVisibleIndex() + 1, test_api_->GetButtonCount());

  // Prepare some items to the shelf.
  ShelfID app_button_id = AddAppShortcut();
  ShelfID platform_button_id = AddApp();

  ShelfButton* app_button = GetButtonByID(app_button_id);
  ShelfButton* platform_button = GetButtonByID(platform_button_id);

  ShelfTooltipManager* tooltip_manager = test_api_->tooltip_manager();
  EXPECT_TRUE(test_api_->shelf_view()->GetWidget()->GetNativeWindow());
  ui::test::EventGenerator& generator = GetEventGenerator();

  generator.MoveMouseTo(app_button->GetBoundsInScreen().CenterPoint());
  // There's a delay to show the tooltip, so it's not visible yet.
  EXPECT_FALSE(tooltip_manager->IsVisible());
  EXPECT_EQ(nullptr, tooltip_manager->GetCurrentAnchorView());

  tooltip_manager->ShowTooltip(app_button);
  EXPECT_TRUE(tooltip_manager->IsVisible());
  EXPECT_EQ(app_button, tooltip_manager->GetCurrentAnchorView());

  // The tooltip will continue showing while the cursor moves between buttons.
  const gfx::Point midpoint =
      gfx::UnionRects(app_button->GetBoundsInScreen(),
                      platform_button->GetBoundsInScreen())
          .CenterPoint();
  generator.MoveMouseTo(midpoint);
  EXPECT_TRUE(tooltip_manager->IsVisible());
  EXPECT_EQ(app_button, tooltip_manager->GetCurrentAnchorView());

  // When the cursor moves over another item, its tooltip shows immediately.
  generator.MoveMouseTo(platform_button->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(tooltip_manager->IsVisible());
  EXPECT_EQ(platform_button, tooltip_manager->GetCurrentAnchorView());
  tooltip_manager->Close();

  // Now cursor over the app_button and move immediately to the platform_button.
  generator.MoveMouseTo(app_button->GetBoundsInScreen().CenterPoint());
  generator.MoveMouseTo(midpoint);
  generator.MoveMouseTo(platform_button->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(tooltip_manager->IsVisible());
  EXPECT_EQ(nullptr, tooltip_manager->GetCurrentAnchorView());
}

// Verify a fix for crash caused by a tooltip update for a deleted shelf
// button, see crbug.com/288838.
TEST_F(ShelfViewTest, RemovingItemClosesTooltip) {
  ShelfTooltipManager* tooltip_manager = test_api_->tooltip_manager();

  // Add an item to the shelf.
  ShelfID app_button_id = AddAppShortcut();
  ShelfButton* app_button = GetButtonByID(app_button_id);

  // Spawn a tooltip on that item.
  tooltip_manager->ShowTooltip(app_button);
  EXPECT_TRUE(tooltip_manager->IsVisible());

  // Remove the app shortcut while the tooltip is open. The tooltip should be
  // closed.
  RemoveByID(app_button_id);
  EXPECT_FALSE(tooltip_manager->IsVisible());

  // Change the shelf layout. This should not crash.
  GetPrimaryShelf()->SetAlignment(SHELF_ALIGNMENT_LEFT);
}

// Changing the shelf alignment closes any open tooltip.
TEST_F(ShelfViewTest, ShelfAlignmentClosesTooltip) {
  ShelfTooltipManager* tooltip_manager = test_api_->tooltip_manager();

  // Add an item to the shelf.
  ShelfID app_button_id = AddAppShortcut();
  ShelfButton* app_button = GetButtonByID(app_button_id);

  // Spawn a tooltip on the item.
  tooltip_manager->ShowTooltip(app_button);
  EXPECT_TRUE(tooltip_manager->IsVisible());

  // Changing shelf alignment hides the tooltip.
  GetPrimaryShelf()->SetAlignment(SHELF_ALIGNMENT_LEFT);
  EXPECT_FALSE(tooltip_manager->IsVisible());
}

TEST_F(ShelfViewTest, ShouldHideTooltipTest) {
  ShelfID app_button_id = AddAppShortcut();
  ShelfID platform_button_id = AddApp();

  // The tooltip shouldn't hide if the mouse is on normal buttons.
  for (int i = 0; i < test_api_->GetButtonCount(); i++) {
    ShelfButton* button = test_api_->GetButton(i);
    if (!button)
      continue;

    EXPECT_FALSE(shelf_view_->ShouldHideTooltip(
        button->GetMirroredBounds().CenterPoint()))
        << "ShelfView tries to hide on button " << i;
  }

  // The tooltip should not hide on the app-list button.
  AppListButton* app_list_button = shelf_view_->GetAppListButton();
  EXPECT_FALSE(shelf_view_->ShouldHideTooltip(
      app_list_button->GetMirroredBounds().CenterPoint()));

  // The tooltip shouldn't hide if the mouse is in the gap between two buttons.
  gfx::Rect app_button_rect = GetButtonByID(app_button_id)->GetMirroredBounds();
  gfx::Rect platform_button_rect =
      GetButtonByID(platform_button_id)->GetMirroredBounds();
  ASSERT_FALSE(app_button_rect.Intersects(platform_button_rect));
  EXPECT_FALSE(shelf_view_->ShouldHideTooltip(
      gfx::UnionRects(app_button_rect, platform_button_rect).CenterPoint()));

  // The tooltip should hide if it's outside of all buttons.
  gfx::Rect all_area;
  for (int i = 0; i < test_api_->GetButtonCount(); i++) {
    ShelfButton* button = test_api_->GetButton(i);
    if (!button)
      continue;

    all_area.Union(button->GetMirroredBounds());
  }
  all_area.Union(shelf_view_->GetAppListButton()->GetMirroredBounds());
  EXPECT_FALSE(shelf_view_->ShouldHideTooltip(all_area.origin()));
  EXPECT_FALSE(shelf_view_->ShouldHideTooltip(
      gfx::Point(all_area.right() - 1, all_area.bottom() - 1)));
  EXPECT_TRUE(shelf_view_->ShouldHideTooltip(
      gfx::Point(all_area.right(), all_area.y())));
  EXPECT_TRUE(shelf_view_->ShouldHideTooltip(
      gfx::Point(all_area.x() - 1, all_area.y())));
  EXPECT_TRUE(shelf_view_->ShouldHideTooltip(
      gfx::Point(all_area.x(), all_area.y() - 1)));
  EXPECT_TRUE(shelf_view_->ShouldHideTooltip(
      gfx::Point(all_area.x(), all_area.bottom())));
}

TEST_F(ShelfViewTest, ShouldHideTooltipWithAppListWindowTest) {
  WmShell::Get()->ShowAppList();
  ASSERT_TRUE(WmShell::Get()->GetAppListTargetVisibility());

  // The tooltip shouldn't hide if the mouse is on normal buttons.
  for (int i = 1; i < test_api_->GetButtonCount(); i++) {
    ShelfButton* button = test_api_->GetButton(i);
    if (!button)
      continue;

    EXPECT_FALSE(shelf_view_->ShouldHideTooltip(
        button->GetMirroredBounds().CenterPoint()))
        << "ShelfView tries to hide on button " << i;
  }

  // The tooltip should hide on the app-list button.
  AppListButton* app_list_button = shelf_view_->GetAppListButton();
  EXPECT_TRUE(shelf_view_->ShouldHideTooltip(
      app_list_button->GetMirroredBounds().CenterPoint()));
}

// Test that by moving the mouse cursor off the button onto the bubble it closes
// the bubble.
TEST_F(ShelfViewTest, ShouldHideTooltipWhenHoveringOnTooltip) {
  ShelfTooltipManager* tooltip_manager = test_api_->tooltip_manager();
  tooltip_manager->set_timer_delay_for_test(0);
  ui::test::EventGenerator& generator = GetEventGenerator();

  // Move the mouse off any item and check that no tooltip is shown.
  generator.MoveMouseTo(gfx::Point(0, 0));
  EXPECT_FALSE(tooltip_manager->IsVisible());

  // Move the mouse over the button and check that it is visible.
  AppListButton* app_list_button = shelf_view_->GetAppListButton();
  gfx::Rect bounds = app_list_button->GetBoundsInScreen();
  generator.MoveMouseTo(bounds.CenterPoint());
  // Wait for the timer to go off.
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(tooltip_manager->IsVisible());

  // Move the mouse cursor slightly to the right of the item. The tooltip should
  // stay open.
  generator.MoveMouseBy(bounds.width() / 2 + 5, 0);
  // Make sure there is no delayed close.
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(tooltip_manager->IsVisible());

  // Move back - it should still stay open.
  generator.MoveMouseBy(-(bounds.width() / 2 + 5), 0);
  // Make sure there is no delayed close.
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(tooltip_manager->IsVisible());

  // Now move the mouse cursor slightly above the item - so that it is over the
  // tooltip bubble. Now it should disappear.
  generator.MoveMouseBy(0, -(bounds.height() / 2 + 5));
  // Wait until the delayed close kicked in.
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(tooltip_manager->IsVisible());
}

// Resizing shelf view while an add animation without fade-in is running,
// which happens when overflow happens. App list button should end up in its
// new ideal bounds.
TEST_F(ShelfViewTest, ResizeDuringOverflowAddAnimation) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetButtonCount(), test_api_->GetLastVisibleIndex() + 1);

  // Add buttons until overflow. Let the non-overflow add animations finish but
  // leave the last running.
  int items_added = 0;
  AddAppNoWait();
  while (!test_api_->IsOverflowButtonVisible()) {
    test_api_->RunMessageLoopUntilAnimationsDone();
    AddAppNoWait();
    ++items_added;
    ASSERT_LT(items_added, 10000);
  }

  // Resize shelf view with that animation running and stay overflown.
  gfx::Rect bounds = shelf_view_->bounds();
  bounds.set_width(bounds.width() - GetShelfConstant(SHELF_SIZE));
  shelf_view_->SetBoundsRect(bounds);
  ASSERT_TRUE(test_api_->IsOverflowButtonVisible());

  // Finish the animation.
  test_api_->RunMessageLoopUntilAnimationsDone();

  // App list button should ends up in its new ideal bounds.
  const int app_list_button_index = test_api_->GetButtonCount() - 1;
  const gfx::Rect& app_list_ideal_bounds =
      test_api_->GetIdealBoundsByIndex(app_list_button_index);
  const gfx::Rect& app_list_bounds =
      test_api_->GetBoundsByIndex(app_list_button_index);
  EXPECT_EQ(app_list_ideal_bounds, app_list_bounds);
}

// Checks the overflow bubble size when an item is ripped off and re-inserted.
TEST_F(ShelfViewTest, OverflowBubbleSize) {
  shelf_delegate_->set_is_app_pinned(true);
  AddButtonsUntilOverflow();
  // Add one more button to prevent the overflow bubble to disappear upon
  // dragging an item out on windows (flakiness, see crbug.com/436131).
  AddAppShortcut();

  // Show overflow bubble.
  test_api_->ShowOverflowBubble();
  ASSERT_TRUE(test_api_->IsShowingOverflowBubble());

  ShelfViewTestAPI test_for_overflow_view(
      test_api_->overflow_bubble()->shelf_view());

  int ripped_index = test_for_overflow_view.GetLastVisibleIndex();
  gfx::Size bubble_size = test_for_overflow_view.GetPreferredSize();
  int item_width = test_for_overflow_view.GetButtonSize() +
                   test_for_overflow_view.GetButtonSpacing();

  ui::test::EventGenerator& generator = GetEventGenerator();
  ShelfButton* button = test_for_overflow_view.GetButton(ripped_index);
  // Rip off the last visible item.
  gfx::Point start_point = button->GetBoundsInScreen().CenterPoint();
  gfx::Point rip_off_point(start_point.x(), 0);
  generator.MoveMouseTo(start_point.x(), start_point.y());
  base::RunLoop().RunUntilIdle();
  generator.PressLeftButton();
  base::RunLoop().RunUntilIdle();
  generator.MoveMouseTo(rip_off_point.x(), rip_off_point.y());
  base::RunLoop().RunUntilIdle();
  test_for_overflow_view.RunMessageLoopUntilAnimationsDone();

  // Check the overflow bubble size when an item is ripped off.
  EXPECT_EQ(bubble_size.width() - item_width,
            test_for_overflow_view.GetPreferredSize().width());
  ASSERT_TRUE(test_api_->IsShowingOverflowBubble());

  // Re-insert an item into the overflow bubble.
  int first_index = test_for_overflow_view.GetFirstVisibleIndex();
  button = test_for_overflow_view.GetButton(first_index);

  // Check the bubble size after an item is re-inserted.
  generator.MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
  test_for_overflow_view.RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(bubble_size.width(),
            test_for_overflow_view.GetPreferredSize().width());

  generator.ReleaseLeftButton();
  test_for_overflow_view.RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(bubble_size.width(),
            test_for_overflow_view.GetPreferredSize().width());
}

// Check the drag insertion bounds of scrolled overflow bubble.
TEST_F(ShelfViewTest, CheckDragInsertBoundsOfScrolledOverflowBubble) {
  UpdateDisplay("400x300");

  EXPECT_EQ(2, model_->item_count());

  AddButtonsUntilOverflow();

  // Show overflow bubble.
  test_api_->ShowOverflowBubble();
  ASSERT_TRUE(test_api_->IsShowingOverflowBubble());

  int item_width = test_api_->GetButtonSize() + test_api_->GetButtonSpacing();
  OverflowBubbleView* bubble_view = test_api_->overflow_bubble()->bubble_view();
  OverflowBubbleViewTestAPI bubble_view_api(bubble_view);

  // Add more buttons until OverflowBubble is scrollable and it has 3 invisible
  // items.
  while (bubble_view_api.GetContentsSize().width() <
         (bubble_view->GetContentsBounds().width() + 3 * item_width)) {
    AddAppShortcut();
  }

  ASSERT_TRUE(test_api_->IsShowingOverflowBubble());

  ShelfViewTestAPI test_for_overflow_view(
      test_api_->overflow_bubble()->shelf_view());
  int first_index = test_for_overflow_view.GetFirstVisibleIndex();
  int last_index = test_for_overflow_view.GetLastVisibleIndex();

  ShelfButton* first_button = test_for_overflow_view.GetButton(first_index);
  ShelfButton* last_button = test_for_overflow_view.GetButton(last_index);
  gfx::Point first_point = first_button->GetBoundsInScreen().CenterPoint();
  gfx::Point last_point = last_button->GetBoundsInScreen().CenterPoint();
  gfx::Rect drag_reinsert_bounds =
      test_for_overflow_view.GetBoundsForDragInsertInScreen();
  EXPECT_TRUE(drag_reinsert_bounds.Contains(first_point));
  EXPECT_FALSE(drag_reinsert_bounds.Contains(last_point));

  // Scroll sufficiently to completely show last item.
  bubble_view_api.ScrollByXOffset(bubble_view_api.GetContentsSize().width() -
                                  bubble_view->GetContentsBounds().width());
  drag_reinsert_bounds =
      test_for_overflow_view.GetBoundsForDragInsertInScreen();
  first_point = first_button->GetBoundsInScreen().CenterPoint();
  last_point = last_button->GetBoundsInScreen().CenterPoint();
  EXPECT_FALSE(drag_reinsert_bounds.Contains(first_point));
  EXPECT_TRUE(drag_reinsert_bounds.Contains(last_point));
}

// Check the drag insertion bounds of shelf view in multi monitor environment.
TEST_F(ShelfViewTest, CheckDragInsertBoundsWithMultiMonitor) {
  // win8-aura doesn't support multiple display.
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("800x600,800x600");
  WmShelf* secondary_shelf =
      WmShelf::ForWindow(WmShell::Get()->GetAllRootWindows()[1]);
  ShelfView* shelf_view_for_secondary =
      secondary_shelf->GetShelfViewForTesting();

  // The bounds should be big enough for 4 buttons + overflow chevron.
  shelf_view_for_secondary->SetBounds(0, 0, 500, GetShelfConstant(SHELF_SIZE));

  ShelfViewTestAPI test_api_for_secondary(shelf_view_for_secondary);
  // Speeds up animation for test.
  test_api_for_secondary.SetAnimationDuration(1);

  AddButtonsUntilOverflow();

  // Test #1: Test drag insertion bounds of primary shelf.
  // Show overflow bubble.
  test_api_->ShowOverflowBubble();
  ASSERT_TRUE(test_api_->IsShowingOverflowBubble());

  ShelfViewTestAPI test_api_for_overflow_view(
      test_api_->overflow_bubble()->shelf_view());

  ShelfButton* button = test_api_for_overflow_view.GetButton(
      test_api_for_overflow_view.GetLastVisibleIndex());

  // Checks that a point in shelf is contained in drag insert bounds.
  gfx::Point point_in_shelf_view = button->GetBoundsInScreen().CenterPoint();
  gfx::Rect drag_reinsert_bounds =
      test_api_for_overflow_view.GetBoundsForDragInsertInScreen();
  EXPECT_TRUE(drag_reinsert_bounds.Contains(point_in_shelf_view));
  // Checks that a point out of shelf is not contained in drag insert bounds.
  EXPECT_FALSE(
      drag_reinsert_bounds.Contains(gfx::Point(point_in_shelf_view.x(), 0)));

  // Test #2: Test drag insertion bounds of secondary shelf.
  // Show overflow bubble.
  test_api_for_secondary.ShowOverflowBubble();
  ASSERT_TRUE(test_api_for_secondary.IsShowingOverflowBubble());

  ShelfViewTestAPI test_api_for_overflow_view_of_secondary(
      test_api_for_secondary.overflow_bubble()->shelf_view());

  ShelfButton* button_in_secondary =
      test_api_for_overflow_view_of_secondary.GetButton(
          test_api_for_overflow_view_of_secondary.GetLastVisibleIndex());

  // Checks that a point in shelf is contained in drag insert bounds.
  gfx::Point point_in_secondary_shelf_view =
      button_in_secondary->GetBoundsInScreen().CenterPoint();
  gfx::Rect drag_reinsert_bounds_in_secondary =
      test_api_for_overflow_view_of_secondary.GetBoundsForDragInsertInScreen();
  EXPECT_TRUE(drag_reinsert_bounds_in_secondary.Contains(
      point_in_secondary_shelf_view));
  // Checks that a point out of shelf is not contained in drag insert bounds.
  EXPECT_FALSE(drag_reinsert_bounds_in_secondary.Contains(
      gfx::Point(point_in_secondary_shelf_view.x(), 0)));
  // Checks that a point of overflow bubble in primary shelf should not be
  // contained by insert bounds of secondary shelf.
  EXPECT_FALSE(drag_reinsert_bounds_in_secondary.Contains(point_in_shelf_view));
}

// Checks the rip an item off from left aligned shelf in secondary monitor.
TEST_F(ShelfViewTest, CheckRipOffFromLeftShelfAlignmentWithMultiMonitor) {
  // win8-aura doesn't support multiple display.
  if (!SupportsMultipleDisplays())
    return;

  UpdateDisplay("800x600,800x600");
  ASSERT_EQ(2U, WmShell::Get()->GetAllRootWindows().size());

  WmWindow* second_root = WmShell::Get()->GetAllRootWindows()[1];
  WmShelf* secondary_shelf = second_root->GetRootWindowController()->GetShelf();

  secondary_shelf->SetAlignment(SHELF_ALIGNMENT_LEFT);
  ASSERT_EQ(SHELF_ALIGNMENT_LEFT, secondary_shelf->alignment());

  // Initially, app list and browser shortcut are added.
  EXPECT_EQ(2, model_->item_count());
  int browser_index = model_->GetItemIndexForType(TYPE_BROWSER_SHORTCUT);
  EXPECT_GT(browser_index, 0);

  ShelfView* shelf_view_for_secondary =
      secondary_shelf->GetShelfViewForTesting();

  ShelfViewTestAPI test_api_for_secondary_shelf_view(shelf_view_for_secondary);
  ShelfButton* button =
      test_api_for_secondary_shelf_view.GetButton(browser_index);

  // Fetch the start point of dragging.
  gfx::Point start_point = button->GetBoundsInScreen().CenterPoint();
  start_point = second_root->ConvertPointFromScreen(start_point);

  ui::test::EventGenerator generator(Shell::GetAllRootWindows()[1],
                                     start_point);

  // Rip off the browser item.
  generator.PressLeftButton();
  generator.MoveMouseTo(start_point.x() + 400, start_point.y());
  test_api_for_secondary_shelf_view.RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(test_api_for_secondary_shelf_view.IsRippedOffFromShelf());
}

// Checks various drag and drop operations from OverflowBubble to Shelf.
TEST_F(ShelfViewTest, CheckDragAndDropFromOverflowBubbleToShelf) {
  AddButtonsUntilOverflow();
  // Add one more button to prevent the overflow bubble to disappear upon
  // dragging an item out on windows (flakiness, see crbug.com/425097).
  AddAppShortcut();

  TestDraggingAnItemFromOverflowToShelf(false);
  TestDraggingAnItemFromOverflowToShelf(true);
}

// Checks creating app shortcut for an opened platform app in overflow bubble
// should be invisible to the shelf. See crbug.com/605793.
TEST_F(ShelfViewTest, CheckOverflowStatusPinOpenedAppToShelf) {
  AddButtonsUntilOverflow();

  // Add a running Platform app.
  ShelfID platform_app_id = AddApp();
  EXPECT_FALSE(GetButtonByID(platform_app_id)->visible());

  // Make the added running platform app to be an app shortcut.
  // This app shortcut should be a swapped view in overflow bubble, which is
  // invisible.
  SetShelfItemTypeToAppShortcut(platform_app_id);
  EXPECT_FALSE(GetButtonByID(platform_app_id)->visible());
}

// Tests that the AppListButton renders as active in response to touches.
TEST_F(ShelfViewTest, AppListButtonTouchFeedback) {
  // Touch feedback is not available in material mode.
  if (ash::MaterialDesignController::IsShelfMaterial())
    return;

  AppListButton* app_list_button = shelf_view_->GetAppListButton();
  EXPECT_FALSE(app_list_button->draw_background_as_active());

  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.set_current_location(
      app_list_button->GetBoundsInScreen().CenterPoint());
  generator.PressTouch();
  EXPECT_TRUE(app_list_button->draw_background_as_active());

  generator.ReleaseTouch();
  EXPECT_FALSE(app_list_button->draw_background_as_active());
  EXPECT_TRUE(WmShell::Get()->GetAppListTargetVisibility());
}

// Tests that a touch that slides out of the bounds of the AppListButton leads
// to the end of rendering an active state.
TEST_F(ShelfViewTest, AppListButtonTouchFeedbackCancellation) {
  // Touch feedback is not available in material mode.
  if (ash::MaterialDesignController::IsShelfMaterial())
    return;

  AppListButton* app_list_button = shelf_view_->GetAppListButton();
  EXPECT_FALSE(app_list_button->draw_background_as_active());

  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.set_current_location(
      app_list_button->GetBoundsInScreen().CenterPoint());
  generator.PressTouch();
  EXPECT_TRUE(app_list_button->draw_background_as_active());

  gfx::Point moved_point(
      app_list_button->GetBoundsInScreen().right() + 1,
      app_list_button->GetBoundsInScreen().CenterPoint().y());
  generator.MoveTouch(moved_point);
  EXPECT_FALSE(app_list_button->draw_background_as_active());

  generator.set_current_location(moved_point);
  generator.ReleaseTouch();
  EXPECT_FALSE(app_list_button->draw_background_as_active());
  EXPECT_FALSE(WmShell::Get()->GetAppListTargetVisibility());
}

// Verifies that Launcher_ButtonPressed_* UMA user actions are recorded when an
// item is selected.
TEST_F(ShelfViewTest,
       Launcher_ButtonPressedUserActionsRecordedWhenItemSelected) {
  base::UserActionTester user_action_tester;

  ShelfID browser_shelf_id = model_->items()[browser_index_].id;
  ShelfItemSelectionTracker* selection_tracker = new ShelfItemSelectionTracker;
  model_->SetShelfItemDelegate(
      browser_shelf_id, std::unique_ptr<ShelfItemDelegate>(selection_tracker));

  SimulateClick(browser_index_);
  EXPECT_EQ(1,
            user_action_tester.GetActionCount("Launcher_ButtonPressed_Mouse"));
}

// Verifies that Launcher_*Task UMA user actions are recorded when an item is
// selected.
TEST_F(ShelfViewTest, Launcher_TaskUserActionsRecordedWhenItemSelected) {
  base::UserActionTester user_action_tester;

  ShelfID browser_shelf_id = model_->items()[browser_index_].id;
  ShelfItemSelectionTracker* selection_tracker = new ShelfItemSelectionTracker;
  selection_tracker->set_item_selected_action(
      ShelfItemDelegate::kNewWindowCreated);
  model_->SetShelfItemDelegate(
      browser_shelf_id, std::unique_ptr<ShelfItemDelegate>(selection_tracker));

  SimulateClick(browser_index_);
  EXPECT_EQ(1, user_action_tester.GetActionCount("Launcher_LaunchTask"));
}

// Verifies that metrics are recorded when an item is minimized and subsequently
// activated.
TEST_F(ShelfViewTest,
       VerifyMetricsAreRecordedWhenAnItemIsMinimizedAndActivated) {
  base::HistogramTester histogram_tester;

  ShelfID browser_shelf_id = model_->items()[browser_index_].id;
  ShelfItemSelectionTracker* selection_tracker = new ShelfItemSelectionTracker;
  model_->SetShelfItemDelegate(
      browser_shelf_id, std::unique_ptr<ShelfItemDelegate>(selection_tracker));

  selection_tracker->set_item_selected_action(
      ShelfItemDelegate::kExistingWindowMinimized);
  SimulateClick(browser_index_);

  selection_tracker->set_item_selected_action(
      ShelfItemDelegate::kExistingWindowActivated);
  SimulateClick(browser_index_);

  histogram_tester.ExpectTotalCount(
      kTimeBetweenWindowMinimizedAndActivatedActionsHistogramName, 1);
}

class ShelfViewVisibleBoundsTest : public ShelfViewTest,
                                   public testing::WithParamInterface<bool> {
 public:
  ShelfViewVisibleBoundsTest() : text_direction_change_(GetParam()) {}

  void CheckAllItemsAreInBounds() {
    gfx::Rect visible_bounds = shelf_view_->GetVisibleItemsBoundsInScreen();
    gfx::Rect shelf_bounds = shelf_view_->GetBoundsInScreen();
    EXPECT_TRUE(shelf_bounds.Contains(visible_bounds));
    for (int i = 0; i < test_api_->GetButtonCount(); ++i)
      if (ShelfButton* button = test_api_->GetButton(i)) {
        if (button->visible())
          EXPECT_TRUE(visible_bounds.Contains(button->GetBoundsInScreen()));
      }
    CheckAppListButtonIsInBounds();
  }

  void CheckAppListButtonIsInBounds() {
    gfx::Rect visible_bounds = shelf_view_->GetVisibleItemsBoundsInScreen();
    gfx::Rect app_list_button_bounds =
        shelf_view_->GetAppListButton()->GetBoundsInScreen();
    EXPECT_TRUE(visible_bounds.Contains(app_list_button_bounds));
  }

 private:
  ScopedTextDirectionChange text_direction_change_;

  DISALLOW_COPY_AND_ASSIGN(ShelfViewVisibleBoundsTest);
};

TEST_P(ShelfViewVisibleBoundsTest, ItemsAreInBounds) {
  // Adding elements leaving some empty space.
  for (int i = 0; i < 3; i++) {
    AddAppShortcut();
  }
  test_api_->RunMessageLoopUntilAnimationsDone();
  EXPECT_FALSE(test_api_->IsOverflowButtonVisible());
  CheckAllItemsAreInBounds();
  // Same for overflow case.
  while (!test_api_->IsOverflowButtonVisible()) {
    AddAppShortcut();
  }
  test_api_->RunMessageLoopUntilAnimationsDone();
  CheckAllItemsAreInBounds();
}

INSTANTIATE_TEST_CASE_P(LtrRtl, ShelfViewTextDirectionTest, testing::Bool());
INSTANTIATE_TEST_CASE_P(VisibleBounds,
                        ShelfViewVisibleBoundsTest,
                        testing::Bool());

namespace {

// An InkDrop implementation that wraps another InkDrop instance to keep track
// of state changes requested on it. Note that this will only track transitions
// routed through AnimateToState() and not the ones performed directly on the
// ripple inside the contained |ink_drop|.
class InkDropSpy : public views::InkDrop {
 public:
  explicit InkDropSpy(std::unique_ptr<views::InkDrop> ink_drop)
      : ink_drop_(std::move(ink_drop)) {}
  ~InkDropSpy() override {}

  std::vector<views::InkDropState> GetAndResetRequestedStates() {
    std::vector<views::InkDropState> requested_states;
    requested_states.swap(requested_states_);
    return requested_states;
  }

  // views::InkDrop:
  void HostSizeChanged(const gfx::Size& new_size) override {
    ink_drop_->HostSizeChanged(new_size);
  }
  views::InkDropState GetTargetInkDropState() const override {
    return ink_drop_->GetTargetInkDropState();
  }
  void AnimateToState(views::InkDropState ink_drop_state) override {
    requested_states_.push_back(ink_drop_state);
    ink_drop_->AnimateToState(ink_drop_state);
  }
  void SnapToActivated() override { ink_drop_->SnapToActivated(); }
  void SetHovered(bool is_hovered) override {
    ink_drop_->SetHovered(is_hovered);
  }
  void SetFocused(bool is_focused) override {
    ink_drop_->SetFocused(is_focused);
  }

  std::unique_ptr<views::InkDrop> ink_drop_;
  std::vector<views::InkDropState> requested_states_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InkDropSpy);
};

// A menu model that contains minimum number of items needed for a menu to be
// shown on a shelf item.
class TestShelfMenuModel : public ShelfMenuModel,
                           public ui::SimpleMenuModel::Delegate {
 public:
  TestShelfMenuModel() : ShelfMenuModel(this) { Build(); }
  ~TestShelfMenuModel() override {}

 private:
  void Build() {
    // A menu is expected to have at least 6 items. Three spacing separators,
    // one title, and at least two more items.
    AddSeparator(ui::SPACING_SEPARATOR);
    AddItem(0, base::ASCIIToUTF16("Title"));
    AddSeparator(ui::SPACING_SEPARATOR);
    AddItem(1, base::ASCIIToUTF16("Item 1"));
    AddItem(2, base::ASCIIToUTF16("Item 2"));
    AddSeparator(ui::SPACING_SEPARATOR);
  }

  // ShelfMenuModel:
  bool IsCommandActive(int command_id) const override { return false; }

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override { return false; }
  bool IsCommandIdEnabled(int command_id) const override {
    return command_id != 0;
  }
  void ExecuteCommand(int command_id, int event_flags) override {}

  DISALLOW_COPY_AND_ASSIGN(TestShelfMenuModel);
};

// A ShelfItemDelegate that returns a menu for the shelf item.
class ListMenuShelfItemDelegate : public TestShelfItemDelegate {
 public:
  ListMenuShelfItemDelegate() : TestShelfItemDelegate(nullptr) {}
  ~ListMenuShelfItemDelegate() override {}

 private:
  // TestShelfItemDelegate:
  ShelfMenuModel* CreateApplicationMenu(int event_flags) override {
    return new TestShelfMenuModel;
  }

  DISALLOW_COPY_AND_ASSIGN(ListMenuShelfItemDelegate);
};

// A test implementation for AppListPresenter that does not change visibility
// state immediately to simulate an in-flight animation. Calling
// FinishVisibilityChange() will change the visibility to the requested one,
// simulating end of the animation. Similar to the actual AppListPresenter, this
// class toggles app list visibility based on the actual visibility rather than
// the target visibility (which might be different due to in-flight animation).
class TestAppListPresenter : public app_list::AppListPresenter {
 public:
  TestAppListPresenter() {}
  ~TestAppListPresenter() override {}

  void FinishVisibilityChange() { is_visible_ = target_visibility_; }

  // app_list::AppListPresenter:
  void Show(int64_t display_id) override { target_visibility_ = true; }
  void Dismiss() override { target_visibility_ = false; }
  void ToggleAppList(int64_t display_id) override {
    if (is_visible_)
      Dismiss();
    else
      Show(display_id);
  }
  bool IsVisible() const override { return is_visible_; }
  bool GetTargetVisibility() const override { return target_visibility_; }

 private:
  bool is_visible_ = false;
  bool target_visibility_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestAppListPresenter);
};

// A test ShellDelegate implementation that returns a TestAppListPresenter as
// the app list presenter.
class TestAppListShellDelegate : public TestShellDelegate {
 public:
  TestAppListShellDelegate()
      : app_list_presenter_(new TestAppListPresenter()) {}
  ~TestAppListShellDelegate() override {}

  TestAppListPresenter* app_list_presenter() const {
    return app_list_presenter_.get();
  }

  // TestShellDelegate:
  app_list::AppListPresenter* GetAppListPresenter() override {
    return app_list_presenter();
  }

 private:
  std::unique_ptr<TestAppListPresenter> app_list_presenter_;

  DISALLOW_COPY_AND_ASSIGN(TestAppListShellDelegate);
};

}  // namespace

// Test fixture that forces material design mode in order to test ink drop
// ripples on shelf.
class ShelfViewInkDropTest : public ShelfViewTest {
 public:
  ShelfViewInkDropTest() {}
  ~ShelfViewInkDropTest() override {}

  void SetUp() override {
    ash_test_helper()->set_test_shell_delegate(CreateTestShellDelegate());

    set_material_mode(ash::MaterialDesignController::MATERIAL_EXPERIMENTAL);

    ShelfViewTest::SetUp();
  }

 protected:
  // Gives subclasses a chance to return a custom test shell delegate to install
  // before calling base class's SetUp(). Shell will take ownership of the
  // returned object.
  virtual TestShellDelegate* CreateTestShellDelegate() {
    shell_delegate_ = new TestAppListShellDelegate;
    return shell_delegate_;
  }

  void InitAppListButtonInkDrop() {
    app_list_button_ = shelf_view_->GetAppListButton();

    auto app_list_button_ink_drop =
        base::MakeUnique<InkDropSpy>(base::MakeUnique<views::InkDropImpl>(
            app_list_button_, app_list_button_->size()));
    app_list_button_ink_drop_ = app_list_button_ink_drop.get();
    views::test::InkDropHostViewTestApi(app_list_button_)
        .SetInkDrop(std::move(app_list_button_ink_drop), false);
  }

  void InitBrowserButtonInkDrop() {
    browser_button_ = test_api_->GetButton(browser_index_);

    auto browser_button_ink_drop =
        base::MakeUnique<InkDropSpy>(base::MakeUnique<views::InkDropImpl>(
            browser_button_, browser_button_->size()));
    browser_button_ink_drop_ = browser_button_ink_drop.get();
    views::test::InkDropHostViewTestApi(browser_button_)
        .SetInkDrop(std::move(browser_button_ink_drop));
  }

  void ShowAppList() {
    DCHECK(shelf_delegate_);
    shell_delegate_->app_list_presenter()->Show(0);
    // Similar to real AppListPresenter, notify button that the app list is
    // shown.
    app_list_button_->OnAppListShown();
  }

  void DismissAppList() {
    DCHECK(shelf_delegate_);
    shell_delegate_->app_list_presenter()->Dismiss();
    // Similar to real AppListPresenter, notify button that the app list is
    // dismissed.
    app_list_button_->OnAppListDismissed();
  }

  void FinishAppListVisibilityChange() {
    DCHECK(shelf_delegate_);
    shell_delegate_->app_list_presenter()->FinishVisibilityChange();
  }

  TestAppListShellDelegate* shell_delegate_ = nullptr;  // Owned by Shell.

  AppListButton* app_list_button_ = nullptr;
  InkDropSpy* app_list_button_ink_drop_ = nullptr;
  ShelfButton* browser_button_ = nullptr;
  InkDropSpy* browser_button_ink_drop_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfViewInkDropTest);
};

// Tests that changing visibility of the app list transitions app list button's
// ink drop states correctly.
TEST_F(ShelfViewInkDropTest, AppListButtonWhenVisibilityChanges) {
  InitAppListButtonInkDrop();

  ShowAppList();
  FinishAppListVisibilityChange();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTIVATED));

  DismissAppList();
  FinishAppListVisibilityChange();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::DEACTIVATED));
}

// Tests that when the app list is hidden, mouse press on the app list button,
// which shows the app list, transitions ink drop states correctly. Also, tests
// that mouse drag and mouse release does not affect the ink drop state.
TEST_F(ShelfViewInkDropTest, AppListButtonMouseEventsWhenHidden) {
  InitAppListButtonInkDrop();

  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.MoveMouseTo(app_list_button_->GetBoundsInScreen().CenterPoint());

  // Mouse press on the button, which shows the app list, should end up in the
  // activated state.
  generator.PressLeftButton();
  // Similar to real AppListPresenter, notify button that the app list is shown.
  app_list_button_->OnAppListShown();
  FinishAppListVisibilityChange();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING,
                          views::InkDropState::ACTIVATED));

  // Dragging mouse out and back and releasing the button should not change the
  // ink drop state.
  generator.MoveMouseBy(app_list_button_->width(), 0);
  generator.MoveMouseBy(-app_list_button_->width(), 0);
  generator.ReleaseLeftButton();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());
}

// Tests that when the app list is visible, mouse press on the app list button,
// which dismisses the app list, transitions ink drop states correctly. Also,
// tests that mouse drag and mouse release does not affect the ink drop state.
TEST_F(ShelfViewInkDropTest, AppListButtonMouseEventsWhenVisible) {
  InitAppListButtonInkDrop();

  // Show the app list.
  ShowAppList();
  FinishAppListVisibilityChange();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTIVATED));

  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.MoveMouseTo(app_list_button_->GetBoundsInScreen().CenterPoint());

  // Mouse press on the button, which dismisses the app list, should end up in
  // the hidden state.
  // Dismiss app list similar to pre-target handler in real AppListPresenter.
  DismissAppList();
  generator.PressLeftButton();
  FinishAppListVisibilityChange();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::DEACTIVATED));

  // Dragging mouse out and back and releasing the button should not change the
  // ink drop state.
  generator.MoveMouseBy(app_list_button_->width(), 0);
  generator.MoveMouseBy(-app_list_button_->width(), 0);
  generator.ReleaseLeftButton();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());
}

// There is no ink drop effect for gesture events on Windows.
#if !defined(OS_WIN)
// Tests that when the app list is hidden, tapping on the app list button
// transitions ink drop states correctly.
TEST_F(ShelfViewInkDropTest, AppListButtonGestureTapWhenHidden) {
  InitAppListButtonInkDrop();

  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.MoveMouseTo(app_list_button_->GetBoundsInScreen().CenterPoint());

  // Touch press on the button should end up in the pending state.
  generator.PressTouch();
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  // Touch release on the button, which shows the app list, should end up in the
  // activated state.
  generator.ReleaseTouch();
  // Similar to real AppListPresenter, notify button that the app list is shown.
  app_list_button_->OnAppListShown();
  FinishAppListVisibilityChange();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTIVATED));
}

// Tests that when the app list is visible, tapping on the app list button
// transitions ink drop states correctly.
TEST_F(ShelfViewInkDropTest, AppListButtonGestureTapWhenVisible) {
  InitAppListButtonInkDrop();

  // Show the app list.
  ShowAppList();
  FinishAppListVisibilityChange();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTIVATED));

  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.MoveMouseTo(app_list_button_->GetBoundsInScreen().CenterPoint());

  // Touch press on the button, which dismisses the app list, should end up in
  // the hidden state.
  // Dismiss app list similar to pre-target handler in real AppListPresenter.
  DismissAppList();
  generator.PressTouch();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::DEACTIVATED));

  // Touch release on the button should not change the ink drop state.
  generator.ReleaseTouch();
  FinishAppListVisibilityChange();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());
}

// Tests that when the app list is hidden, tapping down on the app list button
// and dragging the touch point transitions ink drop states correctly.
TEST_F(ShelfViewInkDropTest, AppListButtonGestureTapDragWhenHidden) {
  InitAppListButtonInkDrop();

  ui::test::EventGenerator& generator = GetEventGenerator();
  gfx::Point touch_location =
      app_list_button_->GetBoundsInScreen().CenterPoint();
  generator.MoveMouseTo(touch_location);

  // Touch press on the button should end up in the pending state.
  generator.PressTouch();
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  // Dragging the touch point should hide the pending ink drop.
  touch_location.Offset(app_list_button_->width(), 0);
  generator.MoveTouch(touch_location);
  EXPECT_EQ(views::InkDropState::HIDDEN,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::HIDDEN));

  // Touch release should not change the ink drop state.
  generator.ReleaseTouch();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());
}

// Tests that when the app list is visible, tapping down on the app list button
// and dragging the touch point transitions ink drop states correctly.
TEST_F(ShelfViewInkDropTest, AppListButtonGestureTapDragWhenVisible) {
  InitAppListButtonInkDrop();

  // Show the app list.
  ShowAppList();
  FinishAppListVisibilityChange();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTIVATED));

  ui::test::EventGenerator& generator = GetEventGenerator();
  gfx::Point touch_location =
      app_list_button_->GetBoundsInScreen().CenterPoint();
  generator.MoveMouseTo(touch_location);

  // Touch press on the button, which dismisses the app list, should end up in
  // the hidden state.
  // Dismiss app list similar to pre-target handler in real AppListPresenter.
  DismissAppList();
  generator.PressTouch();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::DEACTIVATED));

  // Dragging the touch point and releasing should not change the ink drop
  // state.
  touch_location.Offset(app_list_button_->width(), 0);
  generator.MoveTouch(touch_location);
  generator.ReleaseTouch();
  FinishAppListVisibilityChange();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            app_list_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(app_list_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::HIDDEN));
}
#endif  // !defined(OS_WIN)

// Tests that clicking on a shelf item that does not show a menu transitions ink
// drop states correctly.
TEST_F(ShelfViewInkDropTest, ShelfButtonWithoutMenuPressRelease) {
  InitBrowserButtonInkDrop();

  views::CustomButton* button = browser_button_;
  gfx::Point mouse_location = button->GetLocalBounds().CenterPoint();

  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, mouse_location,
                             mouse_location, ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMousePressed(press_event);
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, mouse_location,
                               mouse_location, ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseReleased(release_event);
  EXPECT_EQ(views::InkDropState::HIDDEN,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_TRIGGERED));
}

// Tests that dragging outside of a shelf item transitions ink drop states
// correctly.
TEST_F(ShelfViewInkDropTest, ShelfButtonWithoutMenuPressDragReleaseOutside) {
  InitBrowserButtonInkDrop();

  views::CustomButton* button = browser_button_;
  gfx::Point mouse_location = button->GetLocalBounds().CenterPoint();

  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, mouse_location,
                             mouse_location, ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMousePressed(press_event);
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  mouse_location.Offset(test_api_->GetMinimumDragDistance() / 2, 0);
  ui::MouseEvent drag_event_small(ui::ET_MOUSE_DRAGGED, mouse_location,
                                  mouse_location, ui::EventTimeForNow(),
                                  ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseDragged(drag_event_small);
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  mouse_location.Offset(test_api_->GetMinimumDragDistance(), 0);
  ui::MouseEvent drag_event_large(ui::ET_MOUSE_DRAGGED, mouse_location,
                                  mouse_location, ui::EventTimeForNow(),
                                  ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseDragged(drag_event_large);
  EXPECT_EQ(views::InkDropState::HIDDEN,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::HIDDEN));

  ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, mouse_location,
                               mouse_location, ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseReleased(release_event);
  EXPECT_EQ(views::InkDropState::HIDDEN,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());
}

// Tests that dragging outside of a shelf item and back transitions ink drop
// states correctly.
TEST_F(ShelfViewInkDropTest, ShelfButtonWithoutMenuPressDragReleaseInside) {
  InitBrowserButtonInkDrop();

  views::CustomButton* button = browser_button_;
  gfx::Point mouse_location = button->GetLocalBounds().CenterPoint();

  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, mouse_location,
                             mouse_location, ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMousePressed(press_event);
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  mouse_location.Offset(test_api_->GetMinimumDragDistance() * 2, 0);
  ui::MouseEvent drag_event_outside(ui::ET_MOUSE_DRAGGED, mouse_location,
                                    mouse_location, ui::EventTimeForNow(),
                                    ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseDragged(drag_event_outside);
  EXPECT_EQ(views::InkDropState::HIDDEN,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::HIDDEN));

  mouse_location.Offset(-test_api_->GetMinimumDragDistance() * 2, 0);
  ui::MouseEvent drag_event_inside(ui::ET_MOUSE_DRAGGED, mouse_location,
                                   mouse_location, ui::EventTimeForNow(),
                                   ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseDragged(drag_event_inside);
  EXPECT_EQ(views::InkDropState::HIDDEN,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, mouse_location,
                               mouse_location, ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseReleased(release_event);
  EXPECT_EQ(views::InkDropState::HIDDEN,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());
}

// Tests that clicking on a shelf item that shows an app list menu transitions
// ink drop state correctly.
TEST_F(ShelfViewInkDropTest, ShelfButtonWithMenuPressRelease) {
  InitBrowserButtonInkDrop();

  // Set a delegate for the shelf item that returns an app list menu.
  ShelfID browser_shelf_id = model_->items()[browser_index_].id;
  ListMenuShelfItemDelegate* list_menu_delegate = new ListMenuShelfItemDelegate;
  model_->SetShelfItemDelegate(browser_shelf_id,
                               base::WrapUnique(list_menu_delegate));

  views::CustomButton* button = browser_button_;
  gfx::Point mouse_location = button->GetLocalBounds().CenterPoint();

  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, mouse_location,
                             mouse_location, ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMousePressed(press_event);
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  // Mouse release will spawn a menu which we will then close.
  ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, mouse_location,
                               mouse_location, ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseReleased(release_event);
  test_api_->CloseMenu();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTIVATED,
                          views::InkDropState::DEACTIVATED));
}

namespace {

// An empty menu model for shell context menu just to have a menu.
class TestShellMenuModel : public ui::SimpleMenuModel,
                           public ui::SimpleMenuModel::Delegate {
 public:
  TestShellMenuModel() : ui::SimpleMenuModel(this) {}
  ~TestShellMenuModel() override {}

 private:
  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override { return false; }
  bool IsCommandIdEnabled(int command_id) const override { return true; }
  void ExecuteCommand(int command_id, int event_flags) override {}

  DISALLOW_COPY_AND_ASSIGN(TestShellMenuModel);
};

// A test ShellDelegate implementation for overflow button tests that returns a
// TestShelfMenuModel for the shell context menu.
class TestOverflowButtonShellDelegate : public TestShellDelegate {
 public:
  TestOverflowButtonShellDelegate() {}
  ~TestOverflowButtonShellDelegate() override {}

  // TestShellDelegate:
  ui::MenuModel* CreateContextMenu(WmShelf* wm_shelf,
                                   const ShelfItem* item) override {
    // Caller takes ownership of the returned object.
    return new TestShellMenuModel;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestOverflowButtonShellDelegate);
};

}  // namespace

// Test fixture for testing material design ink drop on overflow button.
class OverflowButtonInkDropTest : public ShelfViewInkDropTest {
 public:
  OverflowButtonInkDropTest() {}
  ~OverflowButtonInkDropTest() override {}

  void SetUp() override {
    ShelfViewInkDropTest::SetUp();

    overflow_button_ = test_api_->overflow_button();

    auto overflow_button_ink_drop =
        base::MakeUnique<InkDropSpy>(base::MakeUnique<views::InkDropImpl>(
            overflow_button_, overflow_button_->size()));
    overflow_button_ink_drop_ = overflow_button_ink_drop.get();
    views::test::InkDropHostViewTestApi(overflow_button_)
        .SetInkDrop(std::move(overflow_button_ink_drop));

    AddButtonsUntilOverflow();
    EXPECT_TRUE(test_api_->IsOverflowButtonVisible());
    EXPECT_FALSE(test_api_->IsShowingOverflowBubble());
  }

 protected:
  gfx::Point GetScreenPointInsideOverflowButton() const {
    return overflow_button_->GetBoundsInScreen().CenterPoint();
  }

  gfx::Point GetScreenPointOutsideOverflowButton() const {
    gfx::Point point = GetScreenPointInsideOverflowButton();
    point.Offset(overflow_button_->width(), 0);
    return point;
  }

  // Overridden from ShelfViewInkDropTest:
  TestShellDelegate* CreateTestShellDelegate() override {
    return new TestOverflowButtonShellDelegate;
  }

  OverflowButton* overflow_button_ = nullptr;
  InkDropSpy* overflow_button_ink_drop_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(OverflowButtonInkDropTest);
};

// Tests ink drop state transitions for the overflow button when the overflow
// bubble is shown or hidden.
TEST_F(OverflowButtonInkDropTest, OnOverflowBubbleShowHide) {
  test_api_->ShowOverflowBubble();
  ASSERT_TRUE(test_api_->IsShowingOverflowBubble());
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTIVATED));

  test_api_->HideOverflowBubble();
  EXPECT_FALSE(test_api_->IsShowingOverflowBubble());
  EXPECT_EQ(views::InkDropState::HIDDEN,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::DEACTIVATED));
}

// Tests ink drop state transitions for the overflow button when the user clicks
// on it.
TEST_F(OverflowButtonInkDropTest, MouseActivate) {
  ui::test::EventGenerator& generator = GetEventGenerator();
  gfx::Point mouse_location = GetScreenPointInsideOverflowButton();
  generator.MoveMouseTo(mouse_location);

  generator.PressLeftButton();
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  generator.ReleaseLeftButton();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTIVATED));

  ASSERT_TRUE(test_api_->IsShowingOverflowBubble());
}

// Tests ink drop state transitions for the overflow button when the user
// presses left mouse button on it and drags it out of the button bounds.
TEST_F(OverflowButtonInkDropTest, MouseDragOut) {
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.MoveMouseTo(GetScreenPointInsideOverflowButton());

  generator.PressLeftButton();
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  generator.MoveMouseTo(GetScreenPointOutsideOverflowButton());
  EXPECT_EQ(views::InkDropState::HIDDEN,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::HIDDEN));

  generator.ReleaseLeftButton();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  EXPECT_FALSE(test_api_->IsShowingOverflowBubble());
}

// Tests ink drop state transitions for the overflow button when the user
// presses left mouse button on it and drags it out of the button bounds and
// back.
TEST_F(OverflowButtonInkDropTest, MouseDragOutAndBack) {
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.MoveMouseTo(GetScreenPointInsideOverflowButton());

  generator.PressLeftButton();
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  generator.MoveMouseTo(GetScreenPointOutsideOverflowButton());
  EXPECT_EQ(views::InkDropState::HIDDEN,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::HIDDEN));

  generator.MoveMouseTo(GetScreenPointInsideOverflowButton());
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  generator.ReleaseLeftButton();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTIVATED));

  ASSERT_TRUE(test_api_->IsShowingOverflowBubble());
}

// Tests ink drop state transitions for the overflow button when the user right
// clicks on the button to show the context menu.
TEST_F(OverflowButtonInkDropTest, MouseContextMenu) {
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.MoveMouseTo(GetScreenPointInsideOverflowButton());

  generator.PressRightButton();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  generator.ReleaseRightButton();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  EXPECT_FALSE(test_api_->IsShowingOverflowBubble());
}

// There is no ink drop effect for gesture events on Windows.
#if !defined(OS_WIN)
// Tests ink drop state transitions for the overflow button when the user taps
// on it.
TEST_F(OverflowButtonInkDropTest, TouchActivate) {
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.set_current_location(GetScreenPointInsideOverflowButton());

  generator.PressTouch();
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  generator.ReleaseTouch();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTIVATED));

  ASSERT_TRUE(test_api_->IsShowingOverflowBubble());
}

// Tests ink drop state transitions for the overflow button when the user taps
// down on it and drags it out of the button bounds.
TEST_F(OverflowButtonInkDropTest, TouchDragOut) {
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.set_current_location(GetScreenPointInsideOverflowButton());

  generator.PressTouch();
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  generator.MoveTouch(GetScreenPointOutsideOverflowButton());
  EXPECT_EQ(views::InkDropState::HIDDEN,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::HIDDEN));

  generator.ReleaseTouch();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  EXPECT_FALSE(test_api_->IsShowingOverflowBubble());
}

// Tests ink drop state transitions for the overflow button when the user taps
// down on it and drags it out of the button bounds and back.
TEST_F(OverflowButtonInkDropTest, TouchDragOutAndBack) {
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.set_current_location(GetScreenPointInsideOverflowButton());

  generator.PressTouch();
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  generator.MoveTouch(GetScreenPointOutsideOverflowButton());
  EXPECT_EQ(views::InkDropState::HIDDEN,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::HIDDEN));

  generator.MoveTouch(GetScreenPointInsideOverflowButton());
  EXPECT_EQ(views::InkDropState::HIDDEN,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  generator.ReleaseTouch();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  EXPECT_FALSE(test_api_->IsShowingOverflowBubble());
}

// Tests ink drop state transitions for the overflow button when the user long
// presses on the button to show the context menu.
TEST_F(OverflowButtonInkDropTest, TouchContextMenu) {
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.set_current_location(GetScreenPointInsideOverflowButton());
  base::ScopedMockTimeMessageLoopTaskRunner mock_task_runner;

  generator.PressTouch();
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  mock_task_runner->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ALTERNATE_ACTION_PENDING,
                          views::InkDropState::HIDDEN));

  generator.ReleaseTouch();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  EXPECT_FALSE(test_api_->IsShowingOverflowBubble());
}

#endif  // !defined(OS_WIN)

// Test fixture for testing material design ink drop on overflow button when it
// is active.
class OverflowButtonActiveInkDropTest : public OverflowButtonInkDropTest {
 public:
  OverflowButtonActiveInkDropTest() {}
  ~OverflowButtonActiveInkDropTest() override {}

  void SetUp() override {
    OverflowButtonInkDropTest::SetUp();

    test_api_->ShowOverflowBubble();
    ASSERT_TRUE(test_api_->IsShowingOverflowBubble());
    EXPECT_EQ(views::InkDropState::ACTIVATED,
              overflow_button_ink_drop_->GetTargetInkDropState());
    EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
                ElementsAre(views::InkDropState::ACTIVATED));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(OverflowButtonActiveInkDropTest);
};

// Tests ink drop state transitions for the overflow button when it is active
// and the user clicks on it.
TEST_F(OverflowButtonActiveInkDropTest, MouseDeactivate) {
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.MoveMouseTo(GetScreenPointInsideOverflowButton());

  generator.PressLeftButton();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  generator.ReleaseLeftButton();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::DEACTIVATED));

  EXPECT_FALSE(test_api_->IsShowingOverflowBubble());
}

// Tests ink drop state transitions for the overflow button when it is active
// and the user presses left mouse button on it and drags it out of the button
// bounds.
TEST_F(OverflowButtonActiveInkDropTest, MouseDragOut) {
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.MoveMouseTo(GetScreenPointInsideOverflowButton());

  generator.PressLeftButton();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  generator.MoveMouseTo(GetScreenPointOutsideOverflowButton());
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  generator.ReleaseLeftButton();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  ASSERT_TRUE(test_api_->IsShowingOverflowBubble());
}

// Tests ink drop state transitions for the overflow button when it is active
// and the user presses left mouse button on it and drags it out of the button
// bounds and back.
TEST_F(OverflowButtonActiveInkDropTest, MouseDragOutAndBack) {
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.MoveMouseTo(GetScreenPointInsideOverflowButton());

  generator.PressLeftButton();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  generator.MoveMouseTo(GetScreenPointOutsideOverflowButton());
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  generator.MoveMouseTo(GetScreenPointInsideOverflowButton());
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  generator.ReleaseLeftButton();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::DEACTIVATED));

  EXPECT_FALSE(test_api_->IsShowingOverflowBubble());
}

// Tests ink drop state transitions for the overflow button when it is active
// and the user right clicks on the button to show the context menu.
TEST_F(OverflowButtonActiveInkDropTest, MouseContextMenu) {
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.MoveMouseTo(GetScreenPointInsideOverflowButton());

  generator.PressRightButton();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  generator.ReleaseRightButton();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  ASSERT_TRUE(test_api_->IsShowingOverflowBubble());
}

// There is no ink drop effect for gesture events on Windows.
#if !defined(OS_WIN)
// Tests ink drop state transitions for the overflow button when it is active
// and the user taps on it.
TEST_F(OverflowButtonActiveInkDropTest, TouchDeactivate) {
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.set_current_location(GetScreenPointInsideOverflowButton());

  generator.PressTouch();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  generator.ReleaseTouch();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::DEACTIVATED,
                          views::InkDropState::HIDDEN));

  EXPECT_FALSE(test_api_->IsShowingOverflowBubble());
}

// Tests ink drop state transitions for the overflow button when it is active
// and the user taps down on it and drags it out of the button bounds.
TEST_F(OverflowButtonActiveInkDropTest, TouchDragOut) {
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.set_current_location(GetScreenPointInsideOverflowButton());

  generator.PressTouch();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  generator.MoveTouch(GetScreenPointOutsideOverflowButton());
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  generator.ReleaseTouch();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  ASSERT_TRUE(test_api_->IsShowingOverflowBubble());
}

// Tests ink drop state transitions for the overflow button when it is active
// and the user taps down on it and drags it out of the button bounds and back.
TEST_F(OverflowButtonActiveInkDropTest, TouchDragOutAndBack) {
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.set_current_location(GetScreenPointInsideOverflowButton());

  generator.PressTouch();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  generator.MoveTouch(GetScreenPointOutsideOverflowButton());
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  generator.MoveTouch(GetScreenPointInsideOverflowButton());
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  generator.ReleaseTouch();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  ASSERT_TRUE(test_api_->IsShowingOverflowBubble());
}

// Tests ink drop state transitions for the overflow button when it is active
// and the user long presses on the button to show the context menu.
TEST_F(OverflowButtonActiveInkDropTest, TouchContextMenu) {
  ui::test::EventGenerator& generator = GetEventGenerator();
  generator.set_current_location(GetScreenPointInsideOverflowButton());
  base::ScopedMockTimeMessageLoopTaskRunner mock_task_runner;

  generator.PressTouch();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  mock_task_runner->FastForwardUntilNoTasksRemain();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  generator.ReleaseTouch();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            overflow_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(overflow_button_ink_drop_->GetAndResetRequestedStates(),
              IsEmpty());

  ASSERT_TRUE(test_api_->IsShowingOverflowBubble());
}

#endif  // !defined(OS_WIN)

}  // namespace test
}  // namespace ash
