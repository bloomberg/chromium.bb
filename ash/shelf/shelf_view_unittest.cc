// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_view.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/focus_cycler.h"
#include "ash/ime/ime_controller_impl.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/back_button.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_focus_cycler.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_observer.h"
#include "ash/shelf/shelf_test_util.h"
#include "ash/shelf/shelf_tooltip_manager.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/ui_controls_factory_ash.h"
#include "ash/test_screenshot_delegate.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wallpaper/wallpaper_controller_test_api.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/icu_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/prefs/pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/test/ui_controls.h"
#include "ui/base/ui_base_features.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/animation/test/ink_drop_host_view_test_api.h"
#include "ui/views/animation/test/ink_drop_impl_test_api.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/view_model.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_util.h"

using testing::ElementsAre;
using testing::IsEmpty;

namespace ash {
namespace {

int64_t GetPrimaryDisplayId() {
  return display::Screen::GetScreen()->GetPrimaryDisplay().id();
}

void ExpectFocused(views::View* view) {
  EXPECT_TRUE(view->GetWidget()->IsActive());
  EXPECT_TRUE(view->Contains(view->GetFocusManager()->GetFocusedView()));
}

void ExpectNotFocused(views::View* view) {
  EXPECT_FALSE(view->GetWidget()->IsActive());
  EXPECT_FALSE(view->Contains(view->GetFocusManager()->GetFocusedView()));
}

class TestShelfObserver : public ShelfObserver {
 public:
  explicit TestShelfObserver(Shelf* shelf) : shelf_(shelf) {
    shelf_->AddObserver(this);
  }

  ~TestShelfObserver() override { shelf_->RemoveObserver(this); }

  // ShelfObserver implementation.
  void OnShelfIconPositionsChanged() override {
    icon_positions_changed_ = true;

    icon_positions_animation_duration_ =
        ShelfViewTestAPI(shelf_->GetShelfViewForTesting())
            .GetAnimationDuration();
  }

  bool icon_positions_changed() const { return icon_positions_changed_; }
  void Reset() {
    icon_positions_changed_ = false;
    icon_positions_animation_duration_ = base::TimeDelta();
  }
  base::TimeDelta icon_positions_animation_duration() const {
    return icon_positions_animation_duration_;
  }

 private:
  Shelf* shelf_;
  bool icon_positions_changed_ = false;
  base::TimeDelta icon_positions_animation_duration_;

  DISALLOW_COPY_AND_ASSIGN(TestShelfObserver);
};

// A ShelfItemDelegate that tracks the last context menu request, and exposes a
// method tests can use to finish the tracked request.
class AsyncContextMenuShelfItemDelegate : public ShelfItemDelegate {
 public:
  AsyncContextMenuShelfItemDelegate() : ShelfItemDelegate(ShelfID()) {}
  ~AsyncContextMenuShelfItemDelegate() override = default;

  bool RunPendingContextMenuCallback(
      std::unique_ptr<ui::SimpleMenuModel> model) {
    if (pending_context_menu_callback_.is_null())
      return false;
    std::move(pending_context_menu_callback_).Run(std::move(model));
    return true;
  }
  bool HasPendingContextMenuCallback() const {
    return !pending_context_menu_callback_.is_null();
  }

 private:
  // ShelfItemDelegate:
  void GetContextMenu(int64_t display_id,
                      GetContextMenuCallback callback) override {
    ASSERT_TRUE(pending_context_menu_callback_.is_null());
    pending_context_menu_callback_ = std::move(callback);
  }
  void ExecuteCommand(bool, int64_t, int32_t, int64_t) override {}
  void Close() override {}

  GetContextMenuCallback pending_context_menu_callback_;
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ShelfObserver::OnShelfIconPositionsChanged tests.

class ShelfObserverIconTest : public AshTestBase {
 public:
  ShelfObserverIconTest() = default;
  ~ShelfObserverIconTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    observer_.reset(new TestShelfObserver(GetPrimaryShelf()));
    shelf_view_test_.reset(
        new ShelfViewTestAPI(GetPrimaryShelf()->GetShelfViewForTesting()));
    shelf_view_test_->SetAnimationDuration(
        base::TimeDelta::FromMilliseconds(1));
  }

  void TearDown() override {
    observer_.reset();
    AshTestBase::TearDown();
  }

  TestShelfObserver* observer() { return observer_.get(); }

  ShelfViewTestAPI* shelf_view_test() { return shelf_view_test_.get(); }

 private:
  std::unique_ptr<TestShelfObserver> observer_;
  std::unique_ptr<ShelfViewTestAPI> shelf_view_test_;

  DISALLOW_COPY_AND_ASSIGN(ShelfObserverIconTest);
};

// A ShelfItemDelegate that tracks selections and reports a custom action.
class ShelfItemSelectionTracker : public ShelfItemDelegate {
 public:
  ShelfItemSelectionTracker() : ShelfItemDelegate(ShelfID()) {}
  ~ShelfItemSelectionTracker() override = default;

  size_t item_selected_count() const { return item_selected_count_; }
  void set_item_selected_action(ShelfAction item_selected_action) {
    item_selected_action_ = item_selected_action;
  }

  // ShelfItemDelegate:
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ShelfLaunchSource source,
                    ItemSelectedCallback callback) override {
    item_selected_count_++;
    std::move(callback).Run(item_selected_action_, {});
  }
  void ExecuteCommand(bool, int64_t, int32_t, int64_t) override {}
  void Close() override {}

 private:
  size_t item_selected_count_ = 0;
  ShelfAction item_selected_action_ = SHELF_ACTION_NONE;

  DISALLOW_COPY_AND_ASSIGN(ShelfItemSelectionTracker);
};

TEST_F(ShelfObserverIconTest, AddRemove) {
  ShelfItem item;
  item.id = ShelfID("foo");
  item.type = TYPE_APP;
  EXPECT_FALSE(observer()->icon_positions_changed());
  const int shelf_item_index = ShelfModel::Get()->Add(item);
  shelf_view_test()->RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(observer()->icon_positions_changed());
  observer()->Reset();

  EXPECT_FALSE(observer()->icon_positions_changed());
  ShelfModel::Get()->RemoveItemAt(shelf_item_index);
  shelf_view_test()->RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(observer()->icon_positions_changed());
  observer()->Reset();
}

// Make sure creating/deleting an window on one displays notifies a
// shelf on external display as well as one on primary.
TEST_F(ShelfObserverIconTest, AddRemoveWithMultipleDisplays) {
  UpdateDisplay("400x400,400x400");
  observer()->Reset();

  Shelf* second_shelf = Shelf::ForWindow(Shell::GetAllRootWindows()[1]);
  TestShelfObserver second_observer(second_shelf);
  ShelfViewTestAPI second_shelf_test_api(
      second_shelf->GetShelfViewForTesting());

  ShelfItem item;
  item.id = ShelfID("foo");
  item.type = TYPE_APP;
  EXPECT_FALSE(observer()->icon_positions_changed());
  EXPECT_FALSE(second_observer.icon_positions_changed());

  // Add item and wait for all animations to finish.
  const int shelf_item_index = ShelfModel::Get()->Add(item);
  shelf_view_test()->RunMessageLoopUntilAnimationsDone();
  second_shelf_test_api.RunMessageLoopUntilAnimationsDone();

  EXPECT_TRUE(observer()->icon_positions_changed());
  EXPECT_TRUE(second_observer.icon_positions_changed());

  // Reset observer so they can track the next set of animations.
  observer()->Reset();
  ASSERT_FALSE(observer()->icon_positions_changed());
  second_observer.Reset();
  ASSERT_FALSE(second_observer.icon_positions_changed());

  // Remove the item, and wait for all the animations to complete.
  ShelfModel::Get()->RemoveItemAt(shelf_item_index);
  shelf_view_test()->RunMessageLoopUntilAnimationsDone();
  second_shelf_test_api.RunMessageLoopUntilAnimationsDone();

  EXPECT_TRUE(observer()->icon_positions_changed());
  EXPECT_TRUE(second_observer.icon_positions_changed());

  observer()->Reset();
  second_observer.Reset();
}

////////////////////////////////////////////////////////////////////////////////
// ShelfView tests.

class ShelfViewTest : public AshTestBase {
 public:
  static const char*
      kTimeBetweenWindowMinimizedAndActivatedActionsHistogramName;

  ShelfViewTest() = default;
  ~ShelfViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    model_ = ShelfModel::Get();
    shelf_view_ = GetPrimaryShelf()->GetShelfViewForTesting();
    navigation_view_ = GetPrimaryShelf()
                           ->shelf_widget()
                           ->navigation_widget()
                           ->GetContentsView();
    gfx::NativeWindow window = shelf_view_->shelf_widget()->GetNativeWindow();
    status_area_ = RootWindowController::ForWindow(window)
                       ->GetStatusAreaWidget()
                       ->GetContentsView();

    // The bounds should be big enough for 4 buttons.
    ASSERT_GE(GetPrimaryShelf()
                  ->shelf_widget()
                  ->hotseat_widget()
                  ->GetWindowBoundsInScreen()
                  .width(),
              500);

    test_api_.reset(new ShelfViewTestAPI(shelf_view_));
    test_api_->SetAnimationDuration(base::TimeDelta::FromMilliseconds(1));

    // Add a browser shortcut shelf item, as chrome does, for testing.
    AddItem(TYPE_BROWSER_SHORTCUT, true);
  }

  void TearDown() override {
    test_api_.reset();
    AshTestBase::TearDown();
  }

  std::string GetNextAppId() { return base::NumberToString(id_); }

 protected:
  // Add shelf items of various types, and optionally wait for animations.
  ShelfID AddItem(ShelfItemType type, bool wait_for_animations) {
    ShelfItem item =
        ShelfTestUtil::AddAppShortcut(base::NumberToString(id_++), type);
    // Set a delegate; some tests require one to select the item.
    model_->SetShelfItemDelegate(item.id,
                                 std::make_unique<ShelfItemSelectionTracker>());
    if (wait_for_animations)
      test_api_->RunMessageLoopUntilAnimationsDone();
    return item.id;
  }
  ShelfID AddAppShortcut() { return AddItem(TYPE_PINNED_APP, true); }
  ShelfID AddAppNoWait() { return AddItem(TYPE_APP, false); }
  ShelfID AddApp() { return AddItem(TYPE_APP, true); }

  void SetShelfItemTypeToAppShortcut(const ShelfID& id) {
    int index = model_->ItemIndexByID(id);
    DCHECK_GE(index, 0);

    ShelfItem item = model_->items()[index];

    if (item.type == TYPE_APP) {
      item.type = TYPE_PINNED_APP;
      model_->Set(index, item);
    }
    test_api_->RunMessageLoopUntilAnimationsDone();
  }

  void RemoveByID(const ShelfID& id) {
    model_->RemoveItemAt(model_->ItemIndexByID(id));
    test_api_->RunMessageLoopUntilAnimationsDone();
  }

  ShelfAppButton* GetButtonByID(const ShelfID& id) {
    return test_api_->GetButton(model_->ItemIndexByID(id));
  }

  ShelfItem GetItemByID(const ShelfID& id) { return *model_->ItemByID(id); }

  void PinAppWithID(const ShelfID& id) { model_->PinAppWithID(id.app_id); }

  bool IsAppPinned(const ShelfID& id) { return model_->IsAppPinned(id.app_id); }

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
    for (int i = 0; i <= shelf_view_->last_visible_index(); ++i) {
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

  // Simulate a mouse press event on the shelf's view at |view_index|.
  views::View* SimulateViewPressed(ShelfView::Pointer pointer, int view_index) {
    views::View* view = test_api_->GetViewAt(view_index);
    ui::MouseEvent pressed_event(ui::ET_MOUSE_PRESSED, gfx::Point(),
                                 view->GetBoundsInScreen().origin(),
                                 ui::EventTimeForNow(), 0, 0);
    shelf_view_->PointerPressedOnButton(view, pointer, pressed_event);
    return view;
  }

  // Similar to SimulateViewPressed, but the index must not be for the app list,
  // since the home button is not a ShelfAppButton.
  ShelfAppButton* SimulateButtonPressed(ShelfView::Pointer pointer,
                                        int button_index) {
    ShelfAppButton* button = test_api_->GetButton(button_index);
    EXPECT_EQ(button, SimulateViewPressed(pointer, button_index));
    return button;
  }

  // Simulates a single mouse click.
  void SimulateClick(int button_index) {
    ShelfAppButton* button =
        SimulateButtonPressed(ShelfView::MOUSE, button_index);
    ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, gfx::Point(),
                                 button->GetBoundsInScreen().origin(),
                                 ui::EventTimeForNow(), 0, 0);
    button->NotifyClick(release_event);
    button->OnMouseReleased(release_event);
  }

  // Simulates the second click of a double click.
  void SimulateDoubleClick(int button_index) {
    ShelfAppButton* button =
        SimulateButtonPressed(ShelfView::MOUSE, button_index);
    ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED, gfx::Point(),
                                 button->GetBoundsInScreen().origin(),
                                 ui::EventTimeForNow(), ui::EF_IS_DOUBLE_CLICK,
                                 0);
    button->NotifyClick(release_event);
    button->OnMouseReleased(release_event);
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
    views::View* to = test_api_->GetViewAt(to_index);
    views::View* from = test_api_->GetViewAt(from_index);
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
    views::View* button = SimulateViewPressed(pointer, button_index);

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
      const std::vector<std::pair<ShelfID, views::View*>>& expected_id_map) {
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
      ShelfAppButton* button = test_api_->GetButton(i);
      id_map->push_back(std::make_pair(model_->items()[i].id, button));
    }
    ASSERT_NO_FATAL_FAILURE(CheckModelIDs(*id_map));

    // Add 5 app shelf buttons for testing.
    for (int i = 0; i < 5; ++i) {
      ShelfID id = AddAppShortcut();
      // The browser shortcut is located at index 0. So we should start to add
      // app shortcuts at index 1.
      id_map->insert(id_map->begin() + i + 1,
                     std::make_pair(id, GetButtonByID(id)));
    }
    ASSERT_NO_FATAL_FAILURE(CheckModelIDs(*id_map));
  }

  // Returns the item's ShelfID at |index|.
  ShelfID GetItemId(int index) {
    DCHECK_GE(index, 0);
    return model_->items()[index].id;
  }

  // Returns the center point of a button. Helper function for event generators.
  gfx::Point GetButtonCenter(const ShelfID& button_id) {
    return GetButtonCenter(GetButtonByID(button_id));
  }

  gfx::Point GetButtonCenter(ShelfAppButton* button) {
    return button->GetBoundsInScreen().CenterPoint();
  }

  ShelfModel* model_ = nullptr;
  ShelfView* shelf_view_ = nullptr;
  views::View* navigation_view_ = nullptr;
  views::View* status_area_ = nullptr;

  int id_ = 0;

  std::unique_ptr<ShelfViewTestAPI> test_api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfViewTest);
};

// TODO(https://crbug.com/1009638): remove this class and all its descendants
// when scrollable shelf is launched.
class ShelfViewNotScrollableTest : public ShelfViewTest {
 public:
  ShelfViewNotScrollableTest() = default;
  ~ShelfViewNotScrollableTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({},
                                          {chromeos::features::kShelfHotseat});
    ShelfViewTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ShelfViewNotScrollableTest);
};

const char*
    ShelfViewTest::kTimeBetweenWindowMinimizedAndActivatedActionsHistogramName =
        ShelfButtonPressedMetricTracker::
            kTimeBetweenWindowMinimizedAndActivatedActionsHistogramName;

class ShelfViewTextDirectionTest : public ShelfViewTest,
                                   public testing::WithParamInterface<bool> {
 public:
  ShelfViewTextDirectionTest() : scoped_locale_(GetParam() ? "he" : "") {}
  virtual ~ShelfViewTextDirectionTest() = default;

 private:
  // Restores locale to the default when destructor is called.
  base::test::ScopedRestoreICUDefaultLocale scoped_locale_;

  DISALLOW_COPY_AND_ASSIGN(ShelfViewTextDirectionTest);
};

// Checks that shelf view contents are considered in the correct drag group.
TEST_F(ShelfViewTest, EnforceDragType) {
  EXPECT_TRUE(test_api_->SameDragType(TYPE_APP, TYPE_APP));
  EXPECT_FALSE(test_api_->SameDragType(TYPE_APP, TYPE_PINNED_APP));
  EXPECT_FALSE(test_api_->SameDragType(TYPE_APP, TYPE_BROWSER_SHORTCUT));

  EXPECT_TRUE(test_api_->SameDragType(TYPE_PINNED_APP, TYPE_PINNED_APP));
  EXPECT_TRUE(test_api_->SameDragType(TYPE_PINNED_APP, TYPE_BROWSER_SHORTCUT));

  EXPECT_TRUE(
      test_api_->SameDragType(TYPE_BROWSER_SHORTCUT, TYPE_BROWSER_SHORTCUT));
}

// Check that model changes are handled correctly while a shelf icon is being
// dragged.
TEST_F(ShelfViewTest, ModelChangesWhileDragging) {
  std::vector<std::pair<ShelfID, views::View*>> id_map;
  SetupForDragTest(&id_map);

  // Dragging browser shortcut at index 1.
  EXPECT_TRUE(model_->items()[0].type == TYPE_BROWSER_SHORTCUT);
  views::View* dragged_button = SimulateDrag(ShelfView::MOUSE, 0, 2, false);
  std::rotate(id_map.begin(), id_map.begin() + 1, id_map.begin() + 3);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  shelf_view_->PointerReleasedOnButton(dragged_button, ShelfView::MOUSE, false);
  EXPECT_TRUE(model_->items()[2].type == TYPE_BROWSER_SHORTCUT);

  // Dragging changes model order.
  dragged_button = SimulateDrag(ShelfView::MOUSE, 0, 2, false);
  std::rotate(id_map.begin(), id_map.begin() + 1, id_map.begin() + 3);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // Cancelling the drag operation restores previous order.
  shelf_view_->PointerReleasedOnButton(dragged_button, ShelfView::MOUSE, true);
  std::rotate(id_map.begin(), id_map.begin() + 2, id_map.begin() + 3);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // Deleting an item keeps the remaining intact.
  dragged_button = SimulateDrag(ShelfView::MOUSE, 0, 2, false);
  model_->RemoveItemAt(0);
  id_map.erase(id_map.begin());
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  shelf_view_->PointerReleasedOnButton(dragged_button, ShelfView::MOUSE, false);

  // Waits until app removal animation finishes.
  test_api_->RunMessageLoopUntilAnimationsDone();

  // Adding a shelf item cancels the drag and respects the order.
  dragged_button = SimulateDrag(ShelfView::MOUSE, 0, 2, false);
  ShelfID new_id = AddAppShortcut();
  id_map.insert(id_map.begin() + 5,
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
      SimulateDrag(ShelfView::MOUSE, 2, 4, false);
  std::rotate(id_map.begin() + 2, id_map.begin() + 3, id_map.begin() + 5);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  // Attempt a touch drag before the mouse drag finishes.
  views::View* dragged_button_touch =
      SimulateDrag(ShelfView::TOUCH, 5, 3, false);

  // Nothing changes since 2nd drag is ignored.
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // Finish the mouse drag.
  shelf_view_->PointerReleasedOnButton(dragged_button_mouse, ShelfView::MOUSE,
                                       false);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // Now start a touch drag.
  dragged_button_touch = SimulateDrag(ShelfView::TOUCH, 5, 3, false);
  std::rotate(id_map.begin() + 4, id_map.begin() + 5, id_map.begin() + 6);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // And attempt a mouse drag before the touch drag finishes.
  dragged_button_mouse = SimulateDrag(ShelfView::MOUSE, 2, 3, false);

  // Nothing changes since 2nd drag is ignored.
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  shelf_view_->PointerReleasedOnButton(dragged_button_touch, ShelfView::TOUCH,
                                       false);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
}

// Ensure that clicking on one item and then dragging another works as expected.
TEST_F(ShelfViewTest, ClickOneDragAnother) {
  std::vector<std::pair<ShelfID, views::View*>> id_map;
  SetupForDragTest(&id_map);

  // A click on the item at index 1 is simulated.
  SimulateClick(1);

  // Dragging the browser item at index 0 should change the model order.
  EXPECT_TRUE(model_->items()[0].type == TYPE_BROWSER_SHORTCUT);
  views::View* dragged_button = SimulateDrag(ShelfView::MOUSE, 0, 2, false);
  std::rotate(id_map.begin(), id_map.begin() + 1, id_map.begin() + 3);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  shelf_view_->PointerReleasedOnButton(dragged_button, ShelfView::MOUSE, false);
  EXPECT_TRUE(model_->items()[2].type == TYPE_BROWSER_SHORTCUT);
}

// Tests that double-clicking an item does not activate it twice.
TEST_F(ShelfViewTest, ClickingTwiceActivatesOnce) {
  // Watch for selection of the browser shortcut.
  ShelfItemSelectionTracker* selection_tracker = new ShelfItemSelectionTracker;
  model_->SetShelfItemDelegate(model_->items()[0].id,
                               base::WrapUnique(selection_tracker));

  // A single click selects the item, but a double-click does not.
  EXPECT_EQ(0u, selection_tracker->item_selected_count());
  SimulateClick(0);
  EXPECT_EQ(1u, selection_tracker->item_selected_count());
  SimulateDoubleClick(0);
  EXPECT_EQ(1u, selection_tracker->item_selected_count());
}

// Check that very small mouse drags do not prevent shelf item selection.
TEST_F(ShelfViewTest, ClickAndMoveSlightly) {
  std::vector<std::pair<ShelfID, views::View*>> id_map;
  SetupForDragTest(&id_map);

  ShelfID shelf_id = (id_map.begin() + 2)->first;
  views::View* button = (id_map.begin() + 2)->second;

  // Install a ShelfItemDelegate that tracks when the shelf item is selected.
  ShelfItemSelectionTracker* selection_tracker = new ShelfItemSelectionTracker;
  model_->SetShelfItemDelegate(
      shelf_id, base::WrapUnique<ShelfItemSelectionTracker>(selection_tracker));

  gfx::Vector2d press_offset(5, 30);
  gfx::Point press_location = gfx::Point() + press_offset;
  gfx::Point press_location_in_screen =
      button->GetBoundsInScreen().origin() + press_offset;

  ui::MouseEvent click_event(ui::ET_MOUSE_PRESSED, press_location,
                             press_location_in_screen, ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMousePressed(click_event);
  EXPECT_EQ(0u, selection_tracker->item_selected_count());

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
  EXPECT_EQ(0u, selection_tracker->item_selected_count());

  ui::MouseEvent release_event(
      ui::ET_MOUSE_RELEASED, press_location + gfx::Vector2d(-1, 0),
      press_location_in_screen + gfx::Vector2d(-1, 0), ui::EventTimeForNow(),
      ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseReleased(release_event);
  EXPECT_EQ(1u, selection_tracker->item_selected_count());
}

// Confirm that item status changes are reflected in the buttons.
TEST_F(ShelfViewTest, ShelfItemStatus) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetButtonCount(), shelf_view_->last_visible_index() + 1);

  // Add platform app button.
  ShelfID last_added = AddApp();
  ShelfItem item = GetItemByID(last_added);
  int index = model_->ItemIndexByID(last_added);
  ShelfAppButton* button = GetButtonByID(last_added);
  ASSERT_EQ(ShelfAppButton::STATE_RUNNING, button->state());
  item.status = STATUS_ATTENTION;
  model_->Set(index, item);
  ASSERT_EQ(ShelfAppButton::STATE_ATTENTION, button->state());
}

// Test what drag movements will rip an item off the shelf.
TEST_F(ShelfViewTest, ShelfRipOff) {
  ui::test::EventGenerator* generator = GetEventGenerator();

  // The test makes some assumptions that the shelf is bottom aligned.
  ASSERT_EQ(shelf_view_->shelf()->alignment(), ShelfAlignment::kBottom);

  // The rip off threshold. Taken from |kRipOffDistance| in shelf_view.cc.
  const int kRipOffDistance = 48;

  // Add two apps on the main shelf.
  ShelfID first_app_id = AddAppShortcut();
  ShelfID second_app_id = AddAppShortcut();

  // Verify that dragging an app off the shelf will trigger the app getting
  // ripped off, unless the distance is less than |kRipOffDistance|.
  gfx::Point first_app_location = GetButtonCenter(GetButtonByID(first_app_id));
  generator->set_current_screen_location(first_app_location);
  generator->PressLeftButton();
  // Drag the mouse to just off the shelf.
  generator->MoveMouseBy(0, -ShelfConfig::Get()->shelf_size() / 2 - 1);
  EXPECT_FALSE(test_api_->IsRippedOffFromShelf());
  // Drag the mouse past the rip off threshold.
  generator->MoveMouseBy(0, -kRipOffDistance);
  EXPECT_TRUE(test_api_->IsRippedOffFromShelf());
  // Drag the mouse back to the original position, so that the app does not get
  // deleted.
  generator->MoveMouseTo(first_app_location);
  generator->ReleaseLeftButton();
  EXPECT_FALSE(test_api_->IsRippedOffFromShelf());
}

// Tests that drag and drop a pinned running app will unpin it.
TEST_F(ShelfViewTest, DragAndDropPinnedRunningApp) {
  ui::test::EventGenerator* generator = GetEventGenerator();

  // The test makes some assumptions that the shelf is bottom aligned.
  ASSERT_EQ(shelf_view_->shelf()->alignment(), ShelfAlignment::kBottom);

  // The rip off threshold. Taken from |kRipOffDistance| in shelf_view.cc.
  constexpr int kRipOffDistance = 48;

  const ShelfID id = AddApp();
  // Added only one app here, the index of the app will not change after drag
  // and drop.
  int index = model_->ItemIndexByID(id);
  ShelfItem item = GetItemByID(id);
  EXPECT_EQ(STATUS_RUNNING, item.status);
  PinAppWithID(id);
  EXPECT_TRUE(IsAppPinned(GetItemId(index)));

  gfx::Point app_location = GetButtonCenter(GetButtonByID(id));
  generator->set_current_screen_location(app_location);
  generator->PressLeftButton();
  generator->MoveMouseBy(0, -ShelfConfig::Get()->shelf_size() / 2 - 1);
  EXPECT_FALSE(test_api_->IsRippedOffFromShelf());
  generator->MoveMouseBy(0, -kRipOffDistance);
  EXPECT_TRUE(test_api_->IsRippedOffFromShelf());
  generator->ReleaseLeftButton();
  EXPECT_FALSE(IsAppPinned(GetItemId(index)));
}

// Confirm that item status changes are reflected in the buttons
// for platform apps.
TEST_F(ShelfViewTest, ShelfItemStatusPlatformApp) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetButtonCount(), shelf_view_->last_visible_index() + 1);

  // Add platform app button.
  ShelfID last_added = AddApp();
  ShelfItem item = GetItemByID(last_added);
  int index = model_->ItemIndexByID(last_added);
  ShelfAppButton* button = GetButtonByID(last_added);
  ASSERT_EQ(ShelfAppButton::STATE_RUNNING, button->state());
  item.status = STATUS_ATTENTION;
  model_->Set(index, item);
  ASSERT_EQ(ShelfAppButton::STATE_ATTENTION, button->state());
}

// Confirm that shelf item bounds are correctly updated on shelf changes.
TEST_F(ShelfViewTest, ShelfItemBoundsCheck) {
  VerifyShelfItemBoundsAreValid();
  shelf_view_->shelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  test_api_->RunMessageLoopUntilAnimationsDone();
  VerifyShelfItemBoundsAreValid();
  shelf_view_->shelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);
  test_api_->RunMessageLoopUntilAnimationsDone();
  VerifyShelfItemBoundsAreValid();
}

TEST_F(ShelfViewTest, ShelfTooltipTest) {
  ASSERT_EQ(shelf_view_->last_visible_index() + 1, test_api_->GetButtonCount());

  // Prepare some items to the shelf.
  ShelfID app_button_id = AddAppShortcut();
  ShelfID platform_button_id = AddApp();

  ShelfAppButton* app_button = GetButtonByID(app_button_id);
  ShelfAppButton* platform_button = GetButtonByID(platform_button_id);

  ShelfTooltipManager* tooltip_manager = test_api_->tooltip_manager();
  EXPECT_TRUE(shelf_view_->GetWidget()->GetNativeWindow());
  ui::test::EventGenerator* generator = GetEventGenerator();

  generator->MoveMouseTo(app_button->GetBoundsInScreen().CenterPoint());
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
  generator->MoveMouseTo(midpoint);
  EXPECT_TRUE(tooltip_manager->IsVisible());
  EXPECT_EQ(app_button, tooltip_manager->GetCurrentAnchorView());

  // When the cursor moves over another item, its tooltip shows immediately.
  generator->MoveMouseTo(platform_button->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(tooltip_manager->IsVisible());
  EXPECT_EQ(platform_button, tooltip_manager->GetCurrentAnchorView());
  tooltip_manager->Close();

  // Now cursor over the app_button and move immediately to the platform_button.
  generator->MoveMouseTo(app_button->GetBoundsInScreen().CenterPoint());
  generator->MoveMouseTo(midpoint);
  generator->MoveMouseTo(platform_button->GetBoundsInScreen().CenterPoint());
  EXPECT_FALSE(tooltip_manager->IsVisible());
  EXPECT_EQ(nullptr, tooltip_manager->GetCurrentAnchorView());
}

TEST_F(ShelfViewNotScrollableTest, ButtonTitlesTest) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  test_api_->RunMessageLoopUntilAnimationsDone();

  EXPECT_EQ(base::UTF8ToUTF16("Launcher"), shelf_view_->shelf_widget()
                                               ->navigation_widget()
                                               ->GetHomeButton()
                                               ->GetAccessibleName());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_SHELF_BACK_BUTTON_TITLE),
            shelf_view_->shelf_widget()
                ->navigation_widget()
                ->GetBackButton()
                ->GetAccessibleName());

  for (int i = 0; i < test_api_->GetButtonCount(); i++) {
    ShelfAppButton* button = test_api_->GetButton(i);
    if (button) {
      EXPECT_EQ(shelf_view_->GetTitleForView(button),
                button->GetAccessibleName())
          << "Each button's tooltip text should read the same as its "
          << "accessible name";
    }
  }
}

// Verify a fix for crash caused by a tooltip update for a deleted shelf
// button, see crbug.com/288838.
TEST_F(ShelfViewTest, RemovingItemClosesTooltip) {
  ShelfTooltipManager* tooltip_manager = test_api_->tooltip_manager();

  // Add an item to the shelf.
  ShelfID app_button_id = AddAppShortcut();
  ShelfAppButton* app_button = GetButtonByID(app_button_id);

  // Spawn a tooltip on that item.
  tooltip_manager->ShowTooltip(app_button);
  EXPECT_TRUE(tooltip_manager->IsVisible());

  // Remove the app shortcut while the tooltip is open. The tooltip should be
  // closed.
  RemoveByID(app_button_id);
  EXPECT_FALSE(tooltip_manager->IsVisible());

  // Change the shelf layout. This should not crash.
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kLeft);
}

// Changing the shelf alignment closes any open tooltip.
TEST_F(ShelfViewTest, ShelfAlignmentClosesTooltip) {
  ShelfTooltipManager* tooltip_manager = test_api_->tooltip_manager();

  // Add an item to the shelf.
  ShelfID app_button_id = AddAppShortcut();
  ShelfAppButton* app_button = GetButtonByID(app_button_id);

  // Spawn a tooltip on the item.
  tooltip_manager->ShowTooltip(app_button);
  EXPECT_TRUE(tooltip_manager->IsVisible());

  // Changing shelf alignment hides the tooltip.
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kLeft);
  EXPECT_FALSE(tooltip_manager->IsVisible());
}

// Verifies that the time of button press is recorded correctly in clamshell.
TEST_F(ShelfViewTest, HomeButtonMetricsInClamshell) {
  const HomeButton* home_button =
      GetPrimaryShelf()->navigation_widget()->GetHomeButton();

  // Make sure we're not showing the app list.
  EXPECT_FALSE(home_button->IsShowingAppList());

  base::UserActionTester user_action_tester;
  ASSERT_EQ(0, user_action_tester.GetActionCount(
                   "AppList_HomeButtonPressedClamshell"));

  GetEventGenerator()->GestureTapAt(
      home_button->GetBoundsInScreen().CenterPoint());
  ASSERT_EQ(1, user_action_tester.GetActionCount(
                   "AppList_HomeButtonPressedClamshell"));
  EXPECT_TRUE(home_button->IsShowingAppList());
}

// Verifies that the time of button press is recorded correctly in tablet.
TEST_F(ShelfViewTest, HomeButtonMetricsInTablet) {
  // Enable accessibility feature that forces home button to be shown even with
  // kHideShelfControlsInTabletMode enabled.
  Shell::Get()
      ->accessibility_controller()
      ->SetTabletModeShelfNavigationButtonsEnabled(true);
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  const HomeButton* home_button =
      GetPrimaryShelf()->navigation_widget()->GetHomeButton();

  // Make sure we're not showing the app list.
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  wm::ActivateWindow(window.get());
  EXPECT_FALSE(home_button->IsShowingAppList());

  base::UserActionTester user_action_tester;
  ASSERT_EQ(
      0, user_action_tester.GetActionCount("AppList_HomeButtonPressedTablet"));

  GetEventGenerator()->GestureTapAt(
      home_button->GetBoundsInScreen().CenterPoint());
  ASSERT_EQ(
      1, user_action_tester.GetActionCount("AppList_HomeButtonPressedTablet"));
  EXPECT_TRUE(home_button->IsShowingAppList());
}

class HotseatShelfViewTest : public ShelfViewTest,
                             public testing::WithParamInterface<bool> {
 public:
  HotseatShelfViewTest() = default;
  ~HotseatShelfViewTest() override = default;

  // AshTestBase:
  void SetUp() override {
    if (GetParam()) {
      feature_list_.InitAndEnableFeature(chromeos::features::kShelfHotseat);
    } else {
      feature_list_.InitAndDisableFeature(chromeos::features::kShelfHotseat);
    }
    ShelfViewTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  DISALLOW_COPY_AND_ASSIGN(HotseatShelfViewTest);
};

// Tests with both hotseat enabled and disabled.
INSTANTIATE_TEST_SUITE_P(All, HotseatShelfViewTest, testing::Bool());

TEST_P(HotseatShelfViewTest, ShouldHideTooltipTest) {

  ShelfID app_button_id = AddAppShortcut();
  ShelfID platform_button_id = AddApp();
  // TODO(manucornet): It should not be necessary to call this manually. The
  // |AddItem| call seems to sometimes be missing some re-layout steps. We
  // should find out what's going on there.
  shelf_view_->UpdateVisibleShelfItemBoundsUnion();
  const HomeButton* home_button =
      GetPrimaryShelf()->navigation_widget()->GetHomeButton();

  // Make sure we're not showing the app list.
  EXPECT_FALSE(home_button->IsShowingAppList())
      << "We should not be showing the app list";

  // The tooltip shouldn't hide if the mouse is on normal buttons.
  for (int i = 0; i < test_api_->GetButtonCount(); i++) {
    ShelfAppButton* button = test_api_->GetButton(i);
    if (!button)
      continue;
    EXPECT_FALSE(shelf_view_->ShouldHideTooltip(
        button->GetMirroredBounds().CenterPoint()))
        << "ShelfView tries to hide on button " << i;
  }

  // The tooltip should hide if placed in between the home button and the
  // first shelf button.
  const int left = home_button->GetBoundsInScreen().right();
  // Find the first shelf button that's to the right of the home button.
  int right = 0;
  for (int i = 0; i < test_api_->GetButtonCount(); ++i) {
    ShelfAppButton* button = test_api_->GetButton(i);
    if (!button)
      continue;
    right = button->GetBoundsInScreen().x();
    if (right > left)
      break;
  }

  gfx::Point test_point(left + (right - left) / 2,
                        home_button->GetBoundsInScreen().y());
  views::View::ConvertPointFromScreen(shelf_view_, &test_point);
  EXPECT_TRUE(shelf_view_->ShouldHideTooltip(gfx::Point(
      shelf_view_->GetMirroredXInView(test_point.x()), test_point.y())))
      << "Tooltip should hide between home button and first shelf item";

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
    ShelfAppButton* button = test_api_->GetButton(i);
    if (!button)
      continue;

    all_area.Union(button->GetMirroredBounds());
  }
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

// Test that shelf button tooltips show (except app list) with an open app list.
TEST_F(ShelfViewTest, ShouldHideTooltipWithAppListWindowTest) {
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());

  // The tooltip shouldn't hide if the mouse is on normal buttons.
  for (int i = 2; i < test_api_->GetButtonCount(); i++) {
    ShelfAppButton* button = test_api_->GetButton(i);
    if (!button)
      continue;

    EXPECT_FALSE(shelf_view_->ShouldHideTooltip(
        button->GetMirroredBounds().CenterPoint()))
        << "ShelfView tries to hide on button " << i;
  }

  // The tooltip should hide on the home button if the app list is visible.
  HomeButton* home_button =
      GetPrimaryShelf()->navigation_widget()->GetHomeButton();
  gfx::Point center_point = home_button->GetBoundsInScreen().CenterPoint();
  views::View::ConvertPointFromScreen(shelf_view_, &center_point);
  EXPECT_TRUE(shelf_view_->ShouldHideTooltip(gfx::Point(
      shelf_view_->GetMirroredXInView(center_point.x()), center_point.y())));
}

// Test that by moving the mouse cursor off the button onto the bubble it closes
// the bubble.
TEST_P(HotseatShelfViewTest, ShouldHideTooltipWhenHoveringOnTooltip) {
  ShelfTooltipManager* tooltip_manager = test_api_->tooltip_manager();
  tooltip_manager->set_timer_delay_for_test(0);
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Move the mouse off any item and check that no tooltip is shown.
  generator->MoveMouseTo(gfx::Point(0, 0));
  EXPECT_FALSE(tooltip_manager->IsVisible());

  // Move the mouse over the button and check that it is visible.
  views::View* button = shelf_view_->first_visible_button_for_testing();
  gfx::Rect bounds = button->GetBoundsInScreen();
  generator->MoveMouseTo(bounds.CenterPoint());
  // Wait for the timer to go off.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(tooltip_manager->IsVisible());

  // Move the mouse cursor slightly to the right of the item. The tooltip should
  // now close.
  generator->MoveMouseBy(bounds.width() / 2 + 5, 0);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tooltip_manager->IsVisible());

  // Move back - it should appear again.
  generator->MoveMouseBy(-(bounds.width() / 2 + 5), 0);
  // Make sure there is no delayed close.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(tooltip_manager->IsVisible());

  // Now move the mouse cursor slightly above the item - so that it is over the
  // tooltip bubble. Now it should disappear.
  generator->MoveMouseBy(0, -(bounds.height() / 2 + 5));
  // Wait until the delayed close kicked in.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(tooltip_manager->IsVisible());
}

// Checks the rip an item off from left aligned shelf in secondary monitor.
TEST_F(ShelfViewTest, CheckRipOffFromLeftShelfAlignmentWithMultiMonitor) {
  UpdateDisplay("800x600,800x600");
  ASSERT_EQ(2U, Shell::GetAllRootWindows().size());

  aura::Window* root_window = Shell::GetAllRootWindows()[1];
  Shelf* secondary_shelf = Shelf::ForWindow(root_window);

  secondary_shelf->SetAlignment(ShelfAlignment::kLeft);
  ASSERT_EQ(ShelfAlignment::kLeft, secondary_shelf->alignment());

  ShelfView* shelf_view_for_secondary =
      secondary_shelf->GetShelfViewForTesting();

  AddAppShortcut();
  ShelfViewTestAPI test_api_for_secondary_shelf_view(shelf_view_for_secondary);
  ShelfAppButton* button = test_api_for_secondary_shelf_view.GetButton(0);

  // Fetch the start point of dragging.
  gfx::Point start_point = button->GetBoundsInScreen().CenterPoint();
  gfx::Point end_point = start_point + gfx::Vector2d(400, 0);
  ::wm::ConvertPointFromScreen(root_window, &start_point);
  ui::test::EventGenerator generator(root_window, start_point);

  // Rip off the browser item.
  generator.PressLeftButton();
  generator.MoveMouseTo(end_point);
  test_api_for_secondary_shelf_view.RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(test_api_for_secondary_shelf_view.IsRippedOffFromShelf());

  // Release the button to prevent crash in test destructor (releasing the
  // button triggers animating shelf to ideal bounds during shell destruction).
  generator.ReleaseLeftButton();
}

// Verifies that Launcher_ButtonPressed_* UMA user actions are recorded when an
// item is selected.
TEST_F(ShelfViewTest,
       Launcher_ButtonPressedUserActionsRecordedWhenItemSelected) {
  base::UserActionTester user_action_tester;

  ShelfItemSelectionTracker* selection_tracker = new ShelfItemSelectionTracker;
  model_->SetShelfItemDelegate(
      model_->items()[0].id,
      base::WrapUnique<ShelfItemSelectionTracker>(selection_tracker));

  SimulateClick(0);
  EXPECT_EQ(1,
            user_action_tester.GetActionCount("Launcher_ButtonPressed_Mouse"));
}

// Verifies that Launcher_*Task UMA user actions are recorded when an item is
// selected.
TEST_F(ShelfViewTest, Launcher_TaskUserActionsRecordedWhenItemSelected) {
  base::UserActionTester user_action_tester;

  ShelfItemSelectionTracker* selection_tracker = new ShelfItemSelectionTracker;
  selection_tracker->set_item_selected_action(SHELF_ACTION_NEW_WINDOW_CREATED);
  model_->SetShelfItemDelegate(
      model_->items()[0].id,
      base::WrapUnique<ShelfItemSelectionTracker>(selection_tracker));

  SimulateClick(0);
  EXPECT_EQ(1, user_action_tester.GetActionCount("Launcher_LaunchTask"));
}

// Verifies that metrics are recorded when an item is minimized and subsequently
// activated.
TEST_F(ShelfViewTest,
       VerifyMetricsAreRecordedWhenAnItemIsMinimizedAndActivated) {
  base::HistogramTester histogram_tester;

  ShelfItemSelectionTracker* selection_tracker = new ShelfItemSelectionTracker;
  model_->SetShelfItemDelegate(
      model_->items()[0].id,
      base::WrapUnique<ShelfItemSelectionTracker>(selection_tracker));

  selection_tracker->set_item_selected_action(SHELF_ACTION_WINDOW_MINIMIZED);
  SimulateClick(0);

  selection_tracker->set_item_selected_action(SHELF_ACTION_WINDOW_ACTIVATED);
  SimulateClick(0);

  histogram_tester.ExpectTotalCount(
      kTimeBetweenWindowMinimizedAndActivatedActionsHistogramName, 1);
}

// Verify the animations of the shelf items are as long as expected.
TEST_F(ShelfViewTest, TestShelfItemsAnimations) {
  TestShelfObserver observer(shelf_view_->shelf());
  ui::test::EventGenerator* generator = GetEventGenerator();
  ShelfID first_app_id = AddAppShortcut();
  ShelfID second_app_id = AddAppShortcut();

  // Set the animation duration for shelf items.
  test_api_->SetAnimationDuration(base::TimeDelta::FromMilliseconds(100));

  // The shelf items should animate if they are moved within the shelf, either
  // by swapping or if the items need to be rearranged due to an item getting
  // ripped off.
  generator->set_current_screen_location(GetButtonCenter(first_app_id));
  generator->DragMouseTo(GetButtonCenter(second_app_id));
  generator->DragMouseBy(0, 50);
  test_api_->RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(100, observer.icon_positions_animation_duration().InMilliseconds());

  // The shelf items should not animate when the whole shelf and its contents
  // have to move.
  observer.Reset();
  shelf_view_->shelf()->SetAlignment(ShelfAlignment::kLeft);
  test_api_->RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(1, observer.icon_positions_animation_duration().InMilliseconds());

  // The shelf items should animate if we are entering or exiting tablet mode,
  // and the shelf alignment is bottom aligned, and scrollable shelf is not
  // enabled.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  const int64_t id = GetPrimaryDisplay().id();
  shelf_view_->shelf()->SetAlignment(ShelfAlignment::kBottom);
  SetShelfAlignmentPref(prefs, id, ShelfAlignment::kBottom);
  observer.Reset();
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  test_api_->RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(1, observer.icon_positions_animation_duration().InMilliseconds());

  observer.Reset();
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  test_api_->RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(1, observer.icon_positions_animation_duration().InMilliseconds());

  // The shelf items should not animate if we are entering or exiting tablet
  // mode, and the shelf alignment is not bottom aligned.
  shelf_view_->shelf()->SetAlignment(ShelfAlignment::kLeft);
  SetShelfAlignmentPref(prefs, id, ShelfAlignment::kLeft);
  observer.Reset();
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  test_api_->RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(1, observer.icon_positions_animation_duration().InMilliseconds());

  observer.Reset();
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  test_api_->RunMessageLoopUntilAnimationsDone();
  EXPECT_EQ(1, observer.icon_positions_animation_duration().InMilliseconds());
}

// Tests that the blank shelf view area shows a context menu on right click.
TEST_F(ShelfViewTest, ShelfViewShowsContextMenu) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(shelf_view_->GetBoundsInScreen().CenterPoint());
  generator->PressRightButton();
  generator->ReleaseRightButton();

  EXPECT_TRUE(test_api_->CloseMenu());
}

TEST_F(ShelfViewTest, TabletModeStartAndEndClosesContextMenu) {
  // Show a context menu on the shelf
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(shelf_view_->GetBoundsInScreen().CenterPoint());
  generator->PressRightButton();

  // Start tablet mode, which should close the menu.
  shelf_view_->OnTabletModeStarted();

  // Attempt to close the menu, which should already be closed.
  EXPECT_FALSE(test_api_->CloseMenu());

  // Show another context menu on the shelf.
  generator->MoveMouseTo(shelf_view_->GetBoundsInScreen().CenterPoint());
  generator->PressRightButton();

  // End tablet mode, which should close the menu.
  shelf_view_->OnTabletModeEnded();

  // Attempt to close the menu, which should already be closed.
  EXPECT_FALSE(test_api_->CloseMenu());
}

// Tests that ShelfWindowWatcher buttons show a context menu on right click.
TEST_F(ShelfViewTest, ShelfWindowWatcherButtonShowsContextMenu) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  widget->Show();
  aura::Window* window = widget->GetNativeWindow();
  ShelfID shelf_id("123");
  window->SetProperty(kShelfIDKey, shelf_id.Serialize());
  window->SetProperty(kShelfItemTypeKey, static_cast<int32_t>(TYPE_DIALOG));
  ShelfAppButton* button = GetButtonByID(shelf_id);
  ASSERT_TRUE(button);
  generator->MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
  generator->PressRightButton();
  EXPECT_TRUE(test_api_->CloseMenu());
}

// Tests that the drag view is set on left click and not set on right click.
TEST_F(ShelfViewTest, ShelfDragViewAndContextMenu) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  widget->Show();
  aura::Window* window = widget->GetNativeWindow();
  ShelfID shelf_id("123");
  window->SetProperty(kShelfIDKey, shelf_id.Serialize());
  window->SetProperty(kShelfItemTypeKey, static_cast<int32_t>(TYPE_DIALOG));

  // Waits for the bounds animation triggered by window property setting to
  // finish.
  test_api_->RunMessageLoopUntilAnimationsDone();

  ShelfAppButton* button = GetButtonByID(shelf_id);
  ASSERT_TRUE(button);

  // Context menu is shown on right button press and no drag view is set.
  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  EXPECT_FALSE(shelf_view_->drag_view());
  generator->MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
  generator->PressRightButton();
  EXPECT_TRUE(shelf_view_->IsShowingMenu());
  EXPECT_FALSE(shelf_view_->drag_view());

  // Press left button. Menu should close.
  generator->PressLeftButton();
  generator->ReleaseLeftButton();
  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  // Press left button. Drag view is set to |button|.
  generator->PressLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(shelf_view_->drag_view(), button);
  generator->ReleaseLeftButton();
  EXPECT_FALSE(shelf_view_->drag_view());
}

// Tests that context menu show is cancelled if item drag starts during context
// menu show (while constructing the item menu model).
TEST_F(ShelfViewTest, InProgressItemDragPreventsContextMenuShow) {
  const ShelfID app_id = AddAppShortcut();
  auto item_delegate_owned =
      std::make_unique<AsyncContextMenuShelfItemDelegate>();
  AsyncContextMenuShelfItemDelegate* item_delegate = item_delegate_owned.get();
  model_->SetShelfItemDelegate(app_id, std::move(item_delegate_owned));

  ShelfAppButton* button = GetButtonByID(app_id);
  ASSERT_TRUE(button);

  const gfx::Point location = button->GetBoundsInScreen().CenterPoint();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(location);
  generator->PressTouch();

  // Generate long press, which should start context menu request.
  ui::GestureEventDetails event_details(ui::ET_GESTURE_LONG_PRESS);
  ui::GestureEvent long_press(location.x(), location.y(), 0,
                              ui::EventTimeForNow(), event_details);
  generator->Dispatch(&long_press);

  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  EXPECT_FALSE(shelf_view_->drag_view());
  EXPECT_TRUE(item_delegate->HasPendingContextMenuCallback());

  // Drag the app icon while context menu callback is pending..
  ASSERT_TRUE(button->FireDragTimerForTest());
  generator->MoveTouchBy(0, -10);

  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  EXPECT_TRUE(shelf_view_->drag_view());

  // Return the context menu model.
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(nullptr);
  menu_model->AddItem(203, base::ASCIIToUTF16("item"));
  ASSERT_TRUE(
      item_delegate->RunPendingContextMenuCallback(std::move(menu_model)));

  // The context menu show is expected to be canceled by the item drag.
  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  EXPECT_TRUE(shelf_view_->drag_view());

  // Drag state should be cleared when the drag ends.
  generator->ReleaseTouch();
  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  EXPECT_FALSE(shelf_view_->drag_view());

  // Another long press starts context menu request.
  generator->set_current_screen_location(location);
  generator->PressTouch();
  ui::GestureEventDetails second_press_event_details(ui::ET_GESTURE_LONG_PRESS);
  ui::GestureEvent second_long_press(location.x(), location.y(), 0,
                                     ui::EventTimeForNow(),
                                     second_press_event_details);
  generator->Dispatch(&second_long_press);
  EXPECT_TRUE(item_delegate->HasPendingContextMenuCallback());
}

// Tests that context menu show is cancelled if item drag starts and ends during
// context menu show (while constructing the item menu model).
TEST_F(ShelfViewTest, CompletedItemDragPreventsContextMenuShow) {
  const ShelfID app_id = AddAppShortcut();
  auto item_delegate_owned =
      std::make_unique<AsyncContextMenuShelfItemDelegate>();
  AsyncContextMenuShelfItemDelegate* item_delegate = item_delegate_owned.get();
  model_->SetShelfItemDelegate(app_id, std::move(item_delegate_owned));

  ShelfAppButton* button = GetButtonByID(app_id);
  ASSERT_TRUE(button);

  const gfx::Point location = button->GetBoundsInScreen().CenterPoint();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(location);
  generator->PressTouch();

  // Generate long press, which should start context menu request.
  ui::GestureEventDetails event_details(ui::ET_GESTURE_LONG_PRESS);
  ui::GestureEvent long_press(location.x(), location.y(), 0,
                              ui::EventTimeForNow(), event_details);
  generator->Dispatch(&long_press);

  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  EXPECT_FALSE(shelf_view_->drag_view());
  EXPECT_TRUE(item_delegate->HasPendingContextMenuCallback());

  // Drag the app icon while context menu callback is pending.
  ASSERT_TRUE(button->FireDragTimerForTest());
  generator->MoveTouchBy(0, -10);

  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  EXPECT_TRUE(shelf_view_->drag_view());

  // Drag state should be cleared when the drag ends.
  generator->ReleaseTouch();
  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  EXPECT_FALSE(shelf_view_->drag_view());

  // Return the context menu model.
  auto menu_model = std::make_unique<ui::SimpleMenuModel>(nullptr);
  menu_model->AddItem(203, base::ASCIIToUTF16("item"));
  ASSERT_TRUE(
      item_delegate->RunPendingContextMenuCallback(std::move(menu_model)));

  // The context menu show is expected to be canceled by the item drag, so it
  // should not be shown even though there is no in-progress drag when the
  // context menu model is received.
  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  EXPECT_FALSE(shelf_view_->drag_view());

  // Another long press starts context menu request.
  generator->set_current_screen_location(location);
  generator->PressTouch();
  ui::GestureEventDetails second_press_event_details(ui::ET_GESTURE_LONG_PRESS);
  ui::GestureEvent second_long_press(location.x(), location.y(), 0,
                                     ui::EventTimeForNow(),
                                     second_press_event_details);
  generator->Dispatch(&second_long_press);
  EXPECT_TRUE(item_delegate->HasPendingContextMenuCallback());
}

// Tests that shelf items in always shown shelf can be dragged through gesture
// events after context menu is shown.
TEST_F(ShelfViewTest, DragAppAfterContextMenuIsShownInAlwaysShownShelf) {
  ASSERT_EQ(SHELF_VISIBLE, GetPrimaryShelf()->GetVisibilityState());
  ui::test::EventGenerator* generator = GetEventGenerator();
  const ShelfID first_app_id = AddAppShortcut();
  const ShelfID second_app_id = AddAppShortcut();
  const int last_index = model_->items().size() - 1;
  ASSERT_TRUE(last_index >= 0);

  const gfx::Point start = GetButtonCenter(first_app_id);
  // Drag the app long enough to ensure the drag can be triggered.
  const gfx::Point end(start.x() + 100, start.y());
  generator->set_current_screen_location(start);

  // Add |STATE_DRAGGING| state to emulate the gesture drag after context menu
  // is shown.
  GetButtonByID(first_app_id)->AddState(ShelfAppButton::STATE_DRAGGING);
  generator->GestureScrollSequence(start, end,
                                   base::TimeDelta::FromMilliseconds(100), 3);

  // |first_add_id| has been moved to the end of the items in the shelf.
  EXPECT_EQ(first_app_id, model_->items()[last_index].id);
}

// Tests that shelf items in AUTO_HIDE_SHOWN shelf can be dragged through
// gesture events after context menu is shown.
TEST_F(ShelfViewTest, DragAppAfterContextMenuIsShownInAutoHideShelf) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  const ShelfID first_app_id = AddAppShortcut();
  const ShelfID second_app_id = AddAppShortcut();
  const int last_index = model_->items().size() - 1;

  Shelf* shelf = GetPrimaryShelf();
  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  widget->Show();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  shelf->shelf_widget()->GetFocusCycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  const gfx::Point start = GetButtonCenter(first_app_id);
  // Drag the app long enough to ensure the drag can be triggered.
  const gfx::Point end = gfx::Point(start.x() + 100, start.y());
  generator->set_current_screen_location(start);

  // Add |STATE_DRAGGING| state to emulate the gesture drag after context menu
  // is shown.
  GetButtonByID(first_app_id)->AddState(ShelfAppButton::STATE_DRAGGING);
  generator->GestureScrollSequence(start, end,
                                   base::TimeDelta::FromMilliseconds(100), 3);

  // |first_add_id| has been moved to the end of the items in the shelf.
  EXPECT_EQ(first_app_id, model_->items()[last_index].id);
}

// Tests that the home button does shows a context menu on right click.
TEST_F(ShelfViewTest, HomeButtonDoesShowContextMenu) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  const HomeButton* home_button =
      GetPrimaryShelf()->navigation_widget()->GetHomeButton();
  generator->MoveMouseTo(home_button->GetBoundsInScreen().CenterPoint());
  generator->PressRightButton();
  EXPECT_TRUE(test_api_->CloseMenu());
}

void ExpectWithinOnePixel(int a, int b) {
  EXPECT_TRUE(abs(a - b) <= 1) << "Values " << a << " and " << b
                               << " should have a difference no greater than 1";
}

TEST_F(ShelfViewTest, IconCenteringTest) {
  const display::Display display =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  const int screen_width = display.bounds().width();
  const int screen_center = screen_width / 2;

  // Show the IME panel, to introduce for asymettry with a larger status area.
  Shell::Get()->ime_controller()->ShowImeMenuOnShelf(true);

  // At the start, we have exactly one app icon for the browser. That should
  // be centered on the screen.
  const ShelfAppButton* button1 = GetButtonByID(model_->items()[0].id);
  ExpectWithinOnePixel(screen_center,
                       button1->GetBoundsInScreen().CenterPoint().x());
  // Also check that the distance between the icon edge and the screen edge is
  // the same on both sides.
  ExpectWithinOnePixel(button1->GetBoundsInScreen().x(),
                       screen_width - button1->GetBoundsInScreen().right());

  const int apps_that_can_easily_fit_at_center_of_screen = 3;
  std::vector<ShelfAppButton*> app_buttons;
  // Start with just the browser app button.
  app_buttons.push_back(GetButtonByID(model_->items()[0].id));
  int n_buttons = 1;

  // Now repeat the same process by adding apps until they can't fit at the
  // center of the screen.
  for (int i = 1; i < apps_that_can_easily_fit_at_center_of_screen; ++i) {
    // Add a new app and add its button to our list.
    app_buttons.push_back(GetButtonByID(AddApp()));
    n_buttons = app_buttons.size();
    if (n_buttons % 2 == 1) {
      // Odd number of apps. Check that the middle app is exactly at the center
      // of the screen.
      ExpectWithinOnePixel(
          screen_center,
          app_buttons[n_buttons / 2]->GetBoundsInScreen().CenterPoint().x());
    }
    // Also check that the first icon is at the same distance from the left
    // screen edge as the last icon is from the right screen edge.
    ExpectWithinOnePixel(
        app_buttons[0]->GetBoundsInScreen().x(),
        screen_width - app_buttons[n_buttons - 1]->GetBoundsInScreen().right());
  }
}

TEST_F(ShelfViewTest, FirstAndLastVisibleIndex) {
  // At the start, the only visible app on the shelf is the browser app button
  // (index 0).
  EXPECT_EQ(0, shelf_view_->first_visible_index());
  EXPECT_EQ(0, shelf_view_->last_visible_index());
  // By enabling tablet mode, the back button (index 0) should become visible,
  // but that does not change the first and last visible indices.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  EXPECT_EQ(0, shelf_view_->first_visible_index());
  EXPECT_EQ(0, shelf_view_->last_visible_index());
  // Turn tablet mode off again.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_EQ(0, shelf_view_->first_visible_index());
  EXPECT_EQ(0, shelf_view_->last_visible_index());
}

TEST_F(ShelfViewTest, ReplacingDelegateCancelsContextMenu) {
  ui::test::EventGenerator* generator = GetEventGenerator();

  ShelfID app_button_id = AddAppShortcut();
  generator->MoveMouseTo(GetButtonCenter(GetButtonByID(app_button_id)));

  // Right click should open the context menu.
  generator->PressRightButton();
  generator->ReleaseRightButton();
  EXPECT_TRUE(shelf_view_->IsShowingMenu());

  // Replacing the item delegate should close the context menu.
  model_->SetShelfItemDelegate(app_button_id,
                               std::make_unique<ShelfItemSelectionTracker>());
  EXPECT_FALSE(shelf_view_->IsShowingMenu());
}

// Test class that tests both context and application menus.
class ShelfViewMenuTest : public ShelfViewTest,
                          public testing::WithParamInterface<bool> {
 public:
  ShelfViewMenuTest() = default;
  ~ShelfViewMenuTest() override = default;

  DISALLOW_COPY_AND_ASSIGN(ShelfViewMenuTest);
};

INSTANTIATE_TEST_SUITE_P(All, ShelfViewMenuTest, testing::Bool());

// Tests that menu anchor points are aligned with the shelf button bounds.
TEST_P(ShelfViewMenuTest, ShelfViewMenuAnchorPoint) {
  const ShelfAppButton* shelf_button = GetButtonByID(AddApp());
  const bool context_menu = GetParam();
  EXPECT_EQ(ShelfAlignment::kBottom, GetPrimaryShelf()->alignment());

  // Test for bottom shelf.
  EXPECT_EQ(
      shelf_button->GetBoundsInScreen().y(),
      test_api_->GetMenuAnchorRect(*shelf_button, gfx::Point(), context_menu)
          .y());

  // Test for left shelf.
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kLeft);

  EXPECT_EQ(
      shelf_button->GetBoundsInScreen().x(),
      test_api_->GetMenuAnchorRect(*shelf_button, gfx::Point(), context_menu)
          .x());

  // Test for right shelf.
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kRight);

  EXPECT_EQ(
      shelf_button->GetBoundsInScreen().x(),
      test_api_->GetMenuAnchorRect(*shelf_button, gfx::Point(), context_menu)
          .x());
}

// Test class that enables notification indicators.
class NotificationIndicatorTest : public ShelfViewTest {
 public:
  NotificationIndicatorTest() = default;
  ~NotificationIndicatorTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({::features::kNotificationIndicator},
                                          {});
    ShelfViewTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(NotificationIndicatorTest);
};

// Tests that an item has a notification indicator when it recieves a
// notification.
TEST_F(NotificationIndicatorTest, AddedItemHasNotificationIndicator) {
  const ShelfID id_0 = AddApp();
  const std::string notification_id_0("notification_id_0");
  const ShelfAppButton* button_0 = GetButtonByID(id_0);

  EXPECT_FALSE(GetItemByID(id_0).has_notification);
  EXPECT_FALSE(button_0->state() & ShelfAppButton::STATE_NOTIFICATION);

  // Post a test notification after the item was added.
  model_->AddNotificationRecord(id_0.app_id, notification_id_0);

  EXPECT_TRUE(GetItemByID(id_0).has_notification);
  EXPECT_TRUE(button_0->state() & ShelfAppButton::STATE_NOTIFICATION);

  // Post another notification for a non existing item.
  const std::string next_app_id(GetNextAppId());
  const std::string notification_id_1("notification_id_1");
  model_->AddNotificationRecord(next_app_id, notification_id_1);

  // Add an item with matching app id.
  const ShelfID id_1 = AddApp();

  // Ensure that the app id assigned to |id_1| is the same as |next_app_id|.
  EXPECT_EQ(next_app_id, id_1.app_id);
  const ShelfAppButton* button_1 = GetButtonByID(id_1);
  EXPECT_TRUE(GetItemByID(id_1).has_notification);
  EXPECT_TRUE(button_1->state() & ShelfAppButton::STATE_NOTIFICATION);

  // Remove all notifications.
  model_->RemoveNotificationRecord(notification_id_0);
  model_->RemoveNotificationRecord(notification_id_1);

  EXPECT_FALSE(GetItemByID(id_0).has_notification);
  EXPECT_FALSE(button_0->state() & ShelfAppButton::STATE_NOTIFICATION);
  EXPECT_FALSE(GetItemByID(id_1).has_notification);
  EXPECT_FALSE(button_1->state() & ShelfAppButton::STATE_NOTIFICATION);
}

// Tests that the notification indicator is active until all notifications have
// been removed.
TEST_F(NotificationIndicatorTest,
       NotificationIndicatorStaysActiveUntilNotificationsAreGone) {
  const ShelfID app = AddApp();
  const ShelfAppButton* button = GetButtonByID(app);

  // Add two notifications for the same app.
  const std::string notification_id_0("notification_id_0");
  model_->AddNotificationRecord(app.app_id, notification_id_0);
  const std::string notification_id_1("notification_id_1");
  model_->AddNotificationRecord(app.app_id, notification_id_1);

  EXPECT_TRUE(GetItemByID(app).has_notification);
  EXPECT_TRUE(button->state() & ShelfAppButton::STATE_NOTIFICATION);

  // Remove one notification, indicator should stay active.
  model_->RemoveNotificationRecord(notification_id_0);

  EXPECT_TRUE(GetItemByID(app).has_notification);
  EXPECT_TRUE(button->state() & ShelfAppButton::STATE_NOTIFICATION);

  // Remove the last notification, indicator should not be active.
  model_->RemoveNotificationRecord(notification_id_1);

  EXPECT_FALSE(GetItemByID(app).has_notification);
  EXPECT_FALSE(button->state() & ShelfAppButton::STATE_NOTIFICATION);
}

class ShelfViewVisibleBoundsTest : public ShelfViewTest,
                                   public testing::WithParamInterface<bool> {
 public:
  ShelfViewVisibleBoundsTest() : scoped_locale_(GetParam() ? "he" : "") {}

  void CheckAllItemsAreInBounds() {
    gfx::Rect visible_bounds = shelf_view_->GetVisibleItemsBoundsInScreen();
    gfx::Rect shelf_bounds = shelf_view_->GetBoundsInScreen();
    EXPECT_TRUE(shelf_bounds.Contains(visible_bounds));
    for (int i = 0; i < test_api_->GetButtonCount(); ++i)
      if (ShelfAppButton* button = test_api_->GetButton(i)) {
        if (button->GetVisible())
          EXPECT_TRUE(visible_bounds.Contains(button->GetBoundsInScreen()));
      }
  }

 private:
  // Restores locale to the default when destructor is called.
  base::test::ScopedRestoreICUDefaultLocale scoped_locale_;

  DISALLOW_COPY_AND_ASSIGN(ShelfViewVisibleBoundsTest);
};

TEST_P(ShelfViewVisibleBoundsTest, ItemsAreInBounds) {
  // Adding elements leaving some empty space.
  for (int i = 0; i < 3; i++) {
    AddAppShortcut();
  }
  test_api_->RunMessageLoopUntilAnimationsDone();
  CheckAllItemsAreInBounds();
}

INSTANTIATE_TEST_SUITE_P(LtrRtl, ShelfViewTextDirectionTest, testing::Bool());
INSTANTIATE_TEST_SUITE_P(VisibleBounds,
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
  ~InkDropSpy() override = default;

  std::vector<views::InkDropState> GetAndResetRequestedStates() {
    std::vector<views::InkDropState> requested_states;
    requested_states.swap(requested_states_);
    return requested_states;
  }

  // views::InkDrop:
  void HostSizeChanged(const gfx::Size& new_size) override {
    ink_drop_->HostSizeChanged(new_size);
  }
  void HostTransformChanged(const gfx::Transform& new_transform) override {
    ink_drop_->HostTransformChanged(new_transform);
  }
  views::InkDropState GetTargetInkDropState() const override {
    return ink_drop_->GetTargetInkDropState();
  }
  void AnimateToState(views::InkDropState ink_drop_state) override {
    requested_states_.push_back(ink_drop_state);
    ink_drop_->AnimateToState(ink_drop_state);
  }

  void SetHoverHighlightFadeDuration(base::TimeDelta duration) override {
    ink_drop_->SetHoverHighlightFadeDuration(duration);
  }

  void UseDefaultHoverHighlightFadeDuration() override {
    ink_drop_->UseDefaultHoverHighlightFadeDuration();
  }

  void SnapToActivated() override { ink_drop_->SnapToActivated(); }
  void SnapToHidden() override { ink_drop_->SnapToHidden(); }
  void SetHovered(bool is_hovered) override {
    ink_drop_->SetHovered(is_hovered);
  }
  void SetFocused(bool is_focused) override {
    ink_drop_->SetFocused(is_focused);
  }

  bool IsHighlightFadingInOrVisible() const override {
    return ink_drop_->IsHighlightFadingInOrVisible();
  }

  void SetShowHighlightOnHover(bool show_highlight_on_hover) override {
    ink_drop_->SetShowHighlightOnHover(show_highlight_on_hover);
  }

  void SetShowHighlightOnFocus(bool show_highlight_on_focus) override {
    ink_drop_->SetShowHighlightOnFocus(show_highlight_on_focus);
  }

  std::unique_ptr<views::InkDrop> ink_drop_;
  std::vector<views::InkDropState> requested_states_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InkDropSpy);
};

// A ShelfItemDelegate that returns a menu for the shelf item.
class ListMenuShelfItemDelegate : public ShelfItemDelegate {
 public:
  ListMenuShelfItemDelegate() : ShelfItemDelegate(ShelfID()) {}
  ~ListMenuShelfItemDelegate() override = default;

 private:
  // ShelfItemDelegate:
  void ItemSelected(std::unique_ptr<ui::Event> event,
                    int64_t display_id,
                    ShelfLaunchSource source,
                    ItemSelectedCallback callback) override {
    // Two items are needed to show a menu; the data in the items is not tested.
    std::move(callback).Run(SHELF_ACTION_NONE, {{}, {}});
  }
  void ExecuteCommand(bool, int64_t, int32_t, int64_t) override {}
  void Close() override {}

  DISALLOW_COPY_AND_ASSIGN(ListMenuShelfItemDelegate);
};

}  // namespace

// Test fixture for testing material design ink drop ripples on shelf.
class ShelfViewInkDropTest : public ShelfViewTest {
 public:
  ShelfViewInkDropTest() = default;
  ShelfViewInkDropTest(const ShelfViewInkDropTest&) = delete;
  ShelfViewInkDropTest& operator=(const ShelfViewInkDropTest&) = delete;
  ~ShelfViewInkDropTest() override = default;

 protected:
  void InitHomeButtonInkDrop() {
    home_button_ = GetPrimaryShelf()->navigation_widget()->GetHomeButton();

    auto home_button_ink_drop =
        std::make_unique<InkDropSpy>(std::make_unique<views::InkDropImpl>(
            home_button_, home_button_->size()));
    home_button_ink_drop_ = home_button_ink_drop.get();
    views::test::InkDropHostViewTestApi(home_button_)
        .SetInkDrop(std::move(home_button_ink_drop), false);
  }

  void InitBrowserButtonInkDrop() {
    browser_button_ = test_api_->GetButton(0);

    auto ink_drop_impl = std::make_unique<views::InkDropImpl>(
        browser_button_, browser_button_->size());
    browser_button_ink_drop_impl_ = ink_drop_impl.get();

    auto browser_button_ink_drop =
        std::make_unique<InkDropSpy>(std::move(ink_drop_impl));
    browser_button_ink_drop_ = browser_button_ink_drop.get();
    views::test::InkDropHostViewTestApi(browser_button_)
        .SetInkDrop(std::move(browser_button_ink_drop));
  }

  HomeButton* home_button_ = nullptr;
  InkDropSpy* home_button_ink_drop_ = nullptr;
  ShelfAppButton* browser_button_ = nullptr;
  InkDropSpy* browser_button_ink_drop_ = nullptr;
  views::InkDropImpl* browser_button_ink_drop_impl_ = nullptr;
};

// Tests that changing visibility of the app list transitions home button's
// ink drop states correctly.
TEST_F(ShelfViewInkDropTest, HomeButtonWhenVisibilityChanges) {
  InitHomeButtonInkDrop();

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());

  EXPECT_EQ(views::InkDropState::ACTIVATED,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTIVATED));

  GetAppListTestHelper()->DismissAndRunLoop();

  EXPECT_EQ(views::InkDropState::HIDDEN,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::DEACTIVATED));
}

// Tests that when the app list is hidden, mouse press on the home button,
// which shows the app list, transitions ink drop states correctly. Also, tests
// that mouse drag and mouse release does not affect the ink drop state.
TEST_F(ShelfViewInkDropTest, HomeButtonMouseEventsWhenHidden) {
  InitHomeButtonInkDrop();

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(home_button_->GetBoundsInScreen().CenterPoint());

  // Mouse press on the button, which shows the app list, should end up in the
  // activated state.
  generator->PressLeftButton();

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING,
                          views::InkDropState::ACTIVATED));

  // Dragging mouse out and back and releasing the button should not change the
  // ink drop state.
  generator->MoveMouseBy(home_button_->width(), 0);
  generator->MoveMouseBy(-home_button_->width(), 0);
  generator->ReleaseLeftButton();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(), IsEmpty());
}

// Tests that when the app list is visible, mouse press on the home button,
// which dismisses the app list, transitions ink drop states correctly. Also,
// tests that mouse drag and mouse release does not affect the ink drop state.
TEST_F(ShelfViewInkDropTest, HomeButtonMouseEventsWhenVisible) {
  InitHomeButtonInkDrop();

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTIVATED));

  // Mouse press on the button, which dismisses the app list, should end up in
  // the hidden state.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(home_button_->GetBoundsInScreen().CenterPoint());
  generator->PressLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING,
                          views::InkDropState::DEACTIVATED));

  // Dragging mouse out and back and releasing the button should not change the
  // ink drop state.
  generator->MoveMouseBy(home_button_->width(), 0);
  generator->MoveMouseBy(-home_button_->width(), 0);
  generator->ReleaseLeftButton();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(), IsEmpty());
}

// Tests that when the app list is hidden, tapping on the home button
// transitions ink drop states correctly.
TEST_F(ShelfViewInkDropTest, HomeButtonGestureTapWhenHidden) {
  InitHomeButtonInkDrop();

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(home_button_->GetBoundsInScreen().CenterPoint());

  // Touch press on the button should end up in the pending state.
  generator->PressTouch();
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  // Touch release on the button, which shows the app list, should end up in the
  // activated state.
  generator->ReleaseTouch();

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_TRIGGERED,
                          views::InkDropState::ACTIVATED));
}

// Tests that when the app list is visible, tapping on the home button
// transitions ink drop states correctly.
TEST_F(ShelfViewInkDropTest, HomeButtonGestureTapWhenVisible) {
  InitHomeButtonInkDrop();

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());

  EXPECT_EQ(views::InkDropState::ACTIVATED,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTIVATED));

  // Touch press and release on the button, which dismisses the app list, should
  // end up in the hidden state.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(home_button_->GetBoundsInScreen().CenterPoint());
  generator->PressTouch();
  generator->ReleaseTouch();
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::DEACTIVATED));
}

// Tests that when the app list is hidden, tapping down on the home button
// and dragging the touch point transitions ink drop states correctly.
TEST_F(ShelfViewInkDropTest, HomeButtonGestureTapDragWhenHidden) {
  InitHomeButtonInkDrop();

  ui::test::EventGenerator* generator = GetEventGenerator();
  gfx::Point touch_location = home_button_->GetBoundsInScreen().CenterPoint();
  generator->MoveMouseTo(touch_location);

  // Touch press on the button should end up in the pending state.
  generator->PressTouch();
  EXPECT_EQ(views::InkDropState::ACTION_PENDING,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_PENDING));

  // Dragging the touch point should hide the pending ink drop.
  touch_location.Offset(home_button_->width(), 0);
  generator->MoveTouch(touch_location);
  EXPECT_EQ(views::InkDropState::HIDDEN,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTION_TRIGGERED));

  // Touch release should not change the ink drop state.
  generator->ReleaseTouch();
  EXPECT_EQ(views::InkDropState::HIDDEN,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(), IsEmpty());
}

// Tests that when the app list is visible, tapping down on the home button
// and dragging the touch point will not change ink drop states.
TEST_F(ShelfViewInkDropTest, HomeButtonGestureTapDragWhenVisible) {
  InitHomeButtonInkDrop();

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());

  EXPECT_EQ(views::InkDropState::ACTIVATED,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTIVATED));

  // Touch press on the button, dragging the touch point, and releasing, which
  // will not dismisses the app list, should end up in the |ACTIVATED| state.
  ui::test::EventGenerator* generator = GetEventGenerator();
  gfx::Point touch_location = home_button_->GetBoundsInScreen().CenterPoint();
  generator->MoveMouseTo(touch_location);

  // Touch press on the button should not change the ink drop state.
  generator->PressTouch();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(), IsEmpty());

  // Dragging the touch point should not hide the pending ink drop.
  touch_location.Offset(home_button_->width(), 0);
  generator->MoveTouch(touch_location);
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(), IsEmpty());

  // Touch release should not change the ink drop state.
  generator->ReleaseTouch();
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            home_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(home_button_ink_drop_->GetAndResetRequestedStates(), IsEmpty());
}

// Tests that clicking on a shelf item that does not show a menu transitions ink
// drop states correctly.
TEST_F(ShelfViewInkDropTest, ShelfButtonWithoutMenuPressRelease) {
  InitBrowserButtonInkDrop();

  views::Button* button = browser_button_;
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

  views::Button* button = browser_button_;
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

  views::Button* button = browser_button_;
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
  model_->SetShelfItemDelegate(model_->items()[0].id,
                               std::make_unique<ListMenuShelfItemDelegate>());

  views::Button* button = browser_button_;
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
  EXPECT_EQ(views::InkDropState::ACTIVATED,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_TRUE(test_api_->CloseMenu());

  EXPECT_EQ(views::InkDropState::HIDDEN,
            browser_button_ink_drop_->GetTargetInkDropState());
  EXPECT_THAT(browser_button_ink_drop_->GetAndResetRequestedStates(),
              ElementsAre(views::InkDropState::ACTIVATED,
                          views::InkDropState::DEACTIVATED));
}

TEST_F(ShelfViewInkDropTest, DismissingMenuWithDoubleClickDoesntShowInkDrop) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  InitBrowserButtonInkDrop();

  views::Button* button = browser_button_;

  // Show a context menu on the home button.
  generator->MoveMouseTo(shelf_view_->shelf_widget()
                             ->navigation_widget()
                             ->GetHomeButton()
                             ->GetBoundsInScreen()
                             .CenterPoint());
  generator->PressRightButton();
  generator->ReleaseRightButton();
  EXPECT_TRUE(shelf_view_->IsShowingMenu());

  // Now check that double-clicking on the browser button dismisses the context
  // menu, and does not show an ink drop.
  EXPECT_EQ(views::InkDropState::HIDDEN,
            browser_button_ink_drop_->GetTargetInkDropState());
  generator->MoveMouseTo(button->GetBoundsInScreen().CenterPoint());
  generator->DoubleClickLeftButton();
  EXPECT_FALSE(shelf_view_->IsShowingMenu());
  EXPECT_EQ(views::InkDropState::HIDDEN,
            browser_button_ink_drop_->GetTargetInkDropState());
}

// Tests that the shelf ink drop transforms when the host transforms. Regression
// test for https://crbug.com/1097044.
TEST_F(ShelfViewInkDropTest, ShelfButtonTransformed) {
  InitBrowserButtonInkDrop();
  ASSERT_TRUE(browser_button_ink_drop_impl_);
  auto ink_drop_impl_test_api =
      std::make_unique<views::test::InkDropImplTestApi>(
          browser_button_ink_drop_impl_);

  views::Button* button = browser_button_;
  gfx::Transform transform;
  transform.Translate(10, 0);
  button->SetTransform(transform);
  EXPECT_EQ(transform, ink_drop_impl_test_api->GetRootLayer()->transform());

  button->SetTransform(gfx::Transform());
  EXPECT_EQ(gfx::Transform(),
            ink_drop_impl_test_api->GetRootLayer()->transform());
}

class ShelfViewFocusTest : public ShelfViewTest {
 public:
  ShelfViewFocusTest() = default;
  ~ShelfViewFocusTest() override = default;

  // AshTestBase:
  void SetUp() override {
    ShelfViewTest::SetUp();

    // Add two app shortcuts for testing.
    AddAppShortcut();
    AddAppShortcut();

    // Focus the home button.
    Shelf* shelf = Shelf::ForWindow(Shell::GetPrimaryRootWindow());
    shelf->shelf_focus_cycler()->FocusNavigation(false /* last_element */);
  }

  void DoTab() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_TAB, ui::EventFlags::EF_NONE);
  }

  void DoShiftTab() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_TAB,
                       ui::EventFlags::EF_SHIFT_DOWN);
  }

  void DoEnter() {
    ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());
    generator.PressKey(ui::KeyboardCode::VKEY_RETURN, ui::EventFlags::EF_NONE);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfViewFocusTest);
};

// Tests that the number of buttons is as expected and the shelf's widget
// intially has focus.
TEST_F(ShelfViewFocusTest, Basic) {
  // There are five buttons, including 3 app buttons. The back button and
  // launcher are always there, the browser shortcut is added in
  // ShelfViewTest and the two test apps added in ShelfViewFocusTest.
  EXPECT_EQ(3, test_api_->GetButtonCount());
  EXPECT_TRUE(GetPrimaryShelf()->navigation_widget()->IsActive());

  // The home button is focused initially because the back button is only
  // visible in tablet mode.
  EXPECT_TRUE(shelf_view_->shelf_widget()
                  ->navigation_widget()
                  ->GetHomeButton()
                  ->HasFocus());
}

// Tests that the expected views have focus when cycling through shelf items
// with tab.
TEST_F(ShelfViewFocusTest, ForwardCycling) {
  // Pressing tab once should advance focus to the next element after the
  // home button, which is the first app.
  DoTab();
  EXPECT_TRUE(test_api_->GetViewAt(0)->HasFocus());

  DoTab();
  DoTab();
  EXPECT_TRUE(test_api_->GetViewAt(2)->HasFocus());
}

// Tests that the expected views have focus when cycling backwards through shelf
// items with shift tab.
TEST_F(ShelfViewFocusTest, BackwardCycling) {
  // The first element is currently focused. Let's advance to the last element
  // first.
  EXPECT_TRUE(shelf_view_->shelf_widget()
                  ->navigation_widget()
                  ->GetHomeButton()
                  ->HasFocus());
  DoTab();
  DoTab();
  DoTab();
  EXPECT_TRUE(test_api_->GetViewAt(2)->HasFocus());

  // Pressing shift tab once should advance focus to the previous element.
  DoShiftTab();
  EXPECT_TRUE(test_api_->GetViewAt(1)->HasFocus());
}

// Verifies that focus moves as expected between the shelf and the status area.
TEST_F(ShelfViewFocusTest, FocusCyclingBetweenShelfAndStatusWidget) {
  // The first element of the shelf (the home button) is focused at start.
  EXPECT_TRUE(shelf_view_->shelf_widget()
                  ->navigation_widget()
                  ->GetHomeButton()
                  ->HasFocus());

  // Focus the next few elements.
  DoTab();
  EXPECT_TRUE(test_api_->GetViewAt(0)->HasFocus());
  DoTab();
  EXPECT_TRUE(test_api_->GetViewAt(1)->HasFocus());
  DoTab();
  EXPECT_TRUE(test_api_->GetViewAt(2)->HasFocus());

  // This is the last element. Tabbing once more should go into the status
  // area.
  DoTab();
  ExpectNotFocused(shelf_view_);
  ExpectFocused(status_area_);

  // Shift-tab: we should be back at the last element in the shelf.
  DoShiftTab();
  EXPECT_TRUE(test_api_->GetViewAt(2)->HasFocus());
  ExpectNotFocused(status_area_);

  // Go into the status area again.
  DoTab();
  ExpectNotFocused(shelf_view_);
  ExpectFocused(status_area_);

  // And keep going forward, now we should be cycling back to the first shelf
  // element.
  DoTab();
  EXPECT_TRUE(shelf_view_->shelf_widget()
                  ->navigation_widget()
                  ->GetHomeButton()
                  ->HasFocus());
  ExpectNotFocused(status_area_);
}

// Verifies that hitting the Esc key can consistently unfocus the shelf.
TEST_F(ShelfViewFocusTest, UnfocusWithEsc) {
  // The home button is focused at start.
  EXPECT_TRUE(shelf_view_->shelf_widget()
                  ->navigation_widget()
                  ->GetHomeButton()
                  ->HasFocus());

  // Focus the status area.
  DoShiftTab();
  ExpectNotFocused(shelf_view_);
  ExpectFocused(status_area_);

  // Advance backwards to the last element of the shelf.
  DoShiftTab();
  ExpectNotFocused(status_area_);
  ExpectFocused(shelf_view_);

  // Press Escape. Nothing should be focused.
  ui::KeyEvent key_event(ui::ET_KEY_PRESSED, ui::VKEY_ESCAPE, ui::EF_NONE);
  shelf_view_->GetWidget()->OnKeyEvent(&key_event);
  ExpectNotFocused(status_area_);
  ExpectNotFocused(shelf_view_);
}

class ShelfViewFocusWithNoShelfNavigationTest : public ShelfViewFocusTest {
 public:
  ShelfViewFocusWithNoShelfNavigationTest() {
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kShelfHotseat,
         features::kHideShelfControlsInTabletMode},
        {});
  }
  ~ShelfViewFocusWithNoShelfNavigationTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ShelfViewFocusWithNoShelfNavigationTest,
       ShelfWithoutNavigationControls) {
  // The home button is focused at start.
  ASSERT_TRUE(GetPrimaryShelf()->navigation_widget()->GetHomeButton());
  EXPECT_TRUE(shelf_view_->shelf_widget()
                  ->navigation_widget()
                  ->GetHomeButton()
                  ->HasFocus());

  // Switch to tablet mode, which should hide navigation buttons.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  test_api_->RunMessageLoopUntilAnimationsDone();

  ASSERT_FALSE(GetPrimaryShelf()->navigation_widget()->GetHomeButton());
  ExpectFocused(status_area_);

  // Verify focus cycling skips the navigation widget.
  DoTab();
  ExpectFocused(status_area_);
  DoTab();
  ExpectFocused(shelf_view_);
  DoShiftTab();
  ExpectFocused(status_area_);
}

}  // namespace ash
