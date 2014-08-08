// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_view.h"

#include <algorithm>
#include <vector>

#include "ash/ash_switches.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/overflow_bubble.h"
#include "ash/shelf/overflow_bubble_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_button.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_icon_observer.h"
#include "ash/shelf/shelf_item_delegate_manager.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_model.h"
#include "ash/shelf/shelf_tooltip_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/overflow_bubble_view_test_api.h"
#include "ash/test/shelf_test_api.h"
#include "ash/test/shelf_view_test_api.h"
#include "ash/test/shell_test_api.h"
#include "ash/test/test_shelf_delegate.h"
#include "ash/test/test_shelf_item_delegate.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "grit/ash_resources.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/view_model.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace test {

////////////////////////////////////////////////////////////////////////////////
// ShelfIconObserver tests.

class TestShelfIconObserver : public ShelfIconObserver {
 public:
  explicit TestShelfIconObserver(Shelf* shelf)
      : shelf_(shelf),
        change_notified_(false) {
    if (shelf_)
      shelf_->AddIconObserver(this);
  }

  virtual ~TestShelfIconObserver() {
    if (shelf_)
      shelf_->RemoveIconObserver(this);
  }

  // ShelfIconObserver implementation.
  virtual void OnShelfIconPositionsChanged() OVERRIDE {
    change_notified_ = true;
  }

  int change_notified() const { return change_notified_; }
  void Reset() { change_notified_ = false; }

 private:
  Shelf* shelf_;
  bool change_notified_;

  DISALLOW_COPY_AND_ASSIGN(TestShelfIconObserver);
};

class ShelfViewIconObserverTest : public AshTestBase {
 public:
  ShelfViewIconObserverTest() {}
  virtual ~ShelfViewIconObserverTest() {}

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();
    Shelf* shelf = Shelf::ForPrimaryDisplay();
    observer_.reset(new TestShelfIconObserver(shelf));

    shelf_view_test_.reset(
        new ShelfViewTestAPI(ShelfTestAPI(shelf).shelf_view()));
    shelf_view_test_->SetAnimationDuration(1);
  }

  virtual void TearDown() OVERRIDE {
    observer_.reset();
    AshTestBase::TearDown();
  }

  TestShelfIconObserver* observer() { return observer_.get(); }

  ShelfViewTestAPI* shelf_view_test() {
    return shelf_view_test_.get();
  }

  Shelf* ShelfForSecondaryDisplay() {
    return Shelf::ForWindow(Shell::GetAllRootWindows()[1]);
  }

 private:
  scoped_ptr<TestShelfIconObserver> observer_;
  scoped_ptr<ShelfViewTestAPI> shelf_view_test_;

  DISALLOW_COPY_AND_ASSIGN(ShelfViewIconObserverTest);
};

// TestShelfItemDelegate which tracks whether it gets selected.
class ShelfItemSelectionTracker : public TestShelfItemDelegate {
 public:
  ShelfItemSelectionTracker() : TestShelfItemDelegate(NULL), selected_(false) {
  }

  virtual ~ShelfItemSelectionTracker() {
  }

  // Returns true if the delegate was selected.
  bool WasSelected() {
    return selected_;
  }

  // TestShelfItemDelegate:
  virtual bool ItemSelected(const ui::Event& event) OVERRIDE {
    selected_ = true;
    return false;
  }

 private:
  bool selected_;

  DISALLOW_COPY_AND_ASSIGN(ShelfItemSelectionTracker);
};

TEST_F(ShelfViewIconObserverTest, AddRemove) {
  TestShelfDelegate* shelf_delegate = TestShelfDelegate::instance();
  ASSERT_TRUE(shelf_delegate);

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = CurrentContext();

  scoped_ptr<views::Widget> widget(new views::Widget());
  widget->Init(params);
  shelf_delegate->AddShelfItem(widget->GetNativeWindow());
  shelf_view_test()->RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(observer()->change_notified());
  observer()->Reset();

  widget->Show();
  widget->GetNativeWindow()->parent()->RemoveChild(widget->GetNativeWindow());
  shelf_view_test()->RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(observer()->change_notified());
  observer()->Reset();
}

// Sometimes fails on trybots on win7_aura. http://crbug.com/177135
#if defined(OS_WIN)
#define MAYBE_AddRemoveWithMultipleDisplays \
    DISABLED_AddRemoveWithMultipleDisplays
#else
#define MAYBE_AddRemoveWithMultipleDisplays \
    AddRemoveWithMultipleDisplays
#endif
// Make sure creating/deleting an window on one displays notifies a
// shelf on external display as well as one on primary.
TEST_F(ShelfViewIconObserverTest, MAYBE_AddRemoveWithMultipleDisplays) {
  UpdateDisplay("400x400,400x400");
  TestShelfIconObserver second_observer(ShelfForSecondaryDisplay());

  TestShelfDelegate* shelf_delegate = TestShelfDelegate::instance();
  ASSERT_TRUE(shelf_delegate);

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = CurrentContext();

  scoped_ptr<views::Widget> widget(new views::Widget());
  widget->Init(params);
  shelf_delegate->AddShelfItem(widget->GetNativeWindow());
  shelf_view_test()->RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(observer()->change_notified());
  EXPECT_TRUE(second_observer.change_notified());
  observer()->Reset();
  second_observer.Reset();

  widget->GetNativeWindow()->parent()->RemoveChild(widget->GetNativeWindow());
  shelf_view_test()->RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(observer()->change_notified());
  EXPECT_TRUE(second_observer.change_notified());

  observer()->Reset();
  second_observer.Reset();
}

TEST_F(ShelfViewIconObserverTest, BoundsChanged) {
  ShelfWidget* widget = Shell::GetPrimaryRootWindowController()->shelf();
  Shelf* shelf = Shelf::ForPrimaryDisplay();
  gfx::Size shelf_size = widget->GetWindowBoundsInScreen().size();
  shelf_size.set_width(shelf_size.width() / 2);
  ASSERT_GT(shelf_size.width(), 0);
  shelf->SetShelfViewBounds(gfx::Rect(shelf_size));
  // No animation happens for ShelfView bounds change.
  EXPECT_TRUE(observer()->change_notified());
  observer()->Reset();
}

////////////////////////////////////////////////////////////////////////////////
// ShelfView tests.

// Simple ShelfDelegate implmentation for ShelfViewTest.OverflowBubbleSize
// and CheckDragAndDropFromOverflowBubbleToShelf
class TestShelfDelegateForShelfView : public ShelfDelegate {
 public:
  explicit TestShelfDelegateForShelfView(ShelfModel* model)
      : model_(model) {}
  virtual ~TestShelfDelegateForShelfView() {}

  // ShelfDelegate overrides:
  virtual void OnShelfCreated(Shelf* shelf) OVERRIDE {}

  virtual void OnShelfDestroyed(Shelf* shelf) OVERRIDE {}

  virtual ShelfID GetShelfIDForAppID(const std::string& app_id) OVERRIDE {
    ShelfID id = 0;
    EXPECT_TRUE(base::StringToInt(app_id, &id));
    return id;
  }

  virtual const std::string& GetAppIDForShelfID(ShelfID id) OVERRIDE {
    // Use |app_id_| member variable because returning a reference to local
    // variable is not allowed.
    app_id_ = base::IntToString(id);
    return app_id_;
  }

  virtual void PinAppWithID(const std::string& app_id) OVERRIDE {
  }

  virtual bool IsAppPinned(const std::string& app_id) OVERRIDE {
    // Returns true for ShelfViewTest.OverflowBubbleSize. To test ripping off in
    // that test, an item is already pinned state.
    return true;
  }

  virtual bool CanPin() const OVERRIDE {
    return true;
  }

  virtual void UnpinAppWithID(const std::string& app_id) OVERRIDE {
    ShelfID id = 0;
    EXPECT_TRUE(base::StringToInt(app_id, &id));
    ASSERT_GT(id, 0);
    int index = model_->ItemIndexByID(id);
    ASSERT_GE(index, 0);

    model_->RemoveItemAt(index);
  }

 private:
  ShelfModel* model_;

  // Temp member variable for returning a value. See the comment in the
  // GetAppIDForShelfID().
  std::string app_id_;

  DISALLOW_COPY_AND_ASSIGN(TestShelfDelegateForShelfView);
};

class ShelfViewTest : public AshTestBase {
 public:
  ShelfViewTest() : model_(NULL), shelf_view_(NULL), browser_index_(1) {}
  virtual ~ShelfViewTest() {}

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();
    test::ShellTestApi test_api(Shell::GetInstance());
    model_ = test_api.shelf_model();
    Shelf* shelf = Shelf::ForPrimaryDisplay();
    shelf_view_ = ShelfTestAPI(shelf).shelf_view();

    // The bounds should be big enough for 4 buttons + overflow chevron.
    shelf_view_->SetBounds(0, 0, 500, kShelfSize);

    test_api_.reset(new ShelfViewTestAPI(shelf_view_));
    test_api_->SetAnimationDuration(1);  // Speeds up animation for test.

    item_manager_ = Shell::GetInstance()->shelf_item_delegate_manager();
    DCHECK(item_manager_);

    // Add browser shortcut shelf item at index 0 for test.
    AddBrowserShortcut();
  }

  virtual void TearDown() OVERRIDE {
    test_api_.reset();
    AshTestBase::TearDown();
  }

 protected:
  void CreateAndSetShelfItemDelegateForID(ShelfID id) {
    scoped_ptr<ShelfItemDelegate> delegate(new TestShelfItemDelegate(NULL));
    item_manager_->SetShelfItemDelegate(id, delegate.Pass());
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

  ShelfID AddPlatformAppNoWait() {
    ShelfItem item;
    item.type = TYPE_PLATFORM_APP;
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

  ShelfID AddPlatformApp() {
    ShelfID id = AddPlatformAppNoWait();
    test_api_->RunMessageLoopUntilAnimationsDone();
    return id;
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
      const std::vector<std::pair<ShelfID, views::View*> >& id_map) {
    size_t map_index = 0;
    for (size_t model_index = 0;
         model_index < model_->items().size();
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
    for (int i=0;i <= test_api_->GetLastVisibleIndex(); ++i) {
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

  views::View* SimulateButtonPressed(ShelfButtonHost::Pointer pointer,
                                     int button_index) {
    ShelfButtonHost* button_host = shelf_view_;
    views::View* button = test_api_->GetButton(button_index);
    ui::MouseEvent click_event(ui::ET_MOUSE_PRESSED,
                               gfx::Point(),
                               button->GetBoundsInScreen().origin(), 0, 0);
    button_host->PointerPressedOnButton(button, pointer, click_event);
    return button;
  }

  views::View* SimulateClick(ShelfButtonHost::Pointer pointer,
                             int button_index) {
    ShelfButtonHost* button_host = shelf_view_;
    views::View* button = SimulateButtonPressed(pointer, button_index);
    button_host->PointerReleasedOnButton(button, ShelfButtonHost::MOUSE, false);
    return button;
  }

  views::View* SimulateDrag(ShelfButtonHost::Pointer pointer,
                            int button_index,
                            int destination_index) {
    ShelfButtonHost* button_host = shelf_view_;
    views::View* button = SimulateButtonPressed(pointer, button_index);

    // Drag.
    views::View* destination = test_api_->GetButton(destination_index);
    ui::MouseEvent drag_event(ui::ET_MOUSE_DRAGGED,
                              gfx::Point(destination->x() - button->x(),
                                         destination->y() - button->y()),
                              destination->GetBoundsInScreen().origin(), 0, 0);
    button_host->PointerDraggedOnButton(button, pointer, drag_event);
    return button;
  }

  void SetupForDragTest(
      std::vector<std::pair<ShelfID, views::View*> >* id_map) {
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

  views::View* GetTooltipAnchorView() {
    return shelf_view_->tooltip_manager()->anchor_;
  }

  void AddButtonsUntilOverflow() {
    int items_added = 0;
    while (!test_api_->IsOverflowButtonVisible()) {
      AddAppShortcut();
      ++items_added;
      ASSERT_LT(items_added, 10000);
    }
  }

  void ShowTooltip() {
    shelf_view_->tooltip_manager()->ShowInternal();
  }

  void TestDraggingAnItemFromOverflowToShelf(bool cancel) {
    test_api_->ShowOverflowBubble();
    ASSERT_TRUE(test_api_->overflow_bubble() &&
                test_api_->overflow_bubble()->IsShowing());

    ash::test::ShelfViewTestAPI test_api_for_overflow(
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

    int drag_item_index =
        test_api_for_overflow.GetLastVisibleIndex();
    ShelfID drag_item_id = GetItemId(drag_item_index);
    ShelfButton* drag_button = test_api_for_overflow.GetButton(drag_item_index);
    gfx::Point center_point_of_drag_item =
        drag_button->GetBoundsInScreen().CenterPoint();

    ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow(),
                                       center_point_of_drag_item);
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
  }

  // Returns the item's ShelfID at |index|.
  ShelfID GetItemId(int index) {
    DCHECK_GE(index, 0);
    return model_->items()[index].id;
  }

  void ReplaceShelfDelegateForRipOffTest() {
    // Replace ShelfDelegate.
    test::ShellTestApi test_api(Shell::GetInstance());
    test_api.SetShelfDelegate(NULL);
    ShelfDelegate* delegate = new TestShelfDelegateForShelfView(model_);
    test_api.SetShelfDelegate(delegate);
    test::ShelfTestAPI(Shelf::ForPrimaryDisplay()).SetShelfDelegate(delegate);
    test_api_->SetShelfDelegate(delegate);
  }

  ShelfModel* model_;
  ShelfView* shelf_view_;
  int browser_index_;
  ShelfItemDelegateManager* item_manager_;

  scoped_ptr<ShelfViewTestAPI> test_api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfViewTest);
};

class ScopedTextDirectionChange {
 public:
  ScopedTextDirectionChange(bool is_rtl)
      : is_rtl_(is_rtl) {
    original_locale_ = l10n_util::GetApplicationLocale(std::string());
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

class ShelfViewTextDirectionTest
    : public ShelfViewTest,
      public testing::WithParamInterface<bool> {
 public:
  ShelfViewTextDirectionTest() : text_direction_change_(GetParam()) {}
  virtual ~ShelfViewTextDirectionTest() {}

  virtual void SetUp() OVERRIDE {
    ShelfViewTest::SetUp();
  }

  virtual void TearDown() OVERRIDE {
    ShelfViewTest::TearDown();
  }

 private:
  ScopedTextDirectionChange text_direction_change_;

  DISALLOW_COPY_AND_ASSIGN(ShelfViewTextDirectionTest);
};

// Checks that the ideal item icon bounds match the view's bounds in the screen
// in both LTR and RTL.
TEST_P(ShelfViewTextDirectionTest, IdealBoundsOfItemIcon) {
  ShelfID id = AddPlatformApp();
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
  ShelfID first_id = AddPlatformApp();
  ShelfID overflow_id = AddPlatformApp();
  int items_added = 0;
  while (!test_api_->IsOverflowButtonVisible()) {
    // Added button is visible after animation while in this loop.
    EXPECT_TRUE(GetButtonByID(overflow_id)->visible());
    overflow_id = AddPlatformApp();
    ++items_added;
    ASSERT_LT(items_added, 10000);
  }
  ShelfID last_id = AddPlatformApp();

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
  EXPECT_TRUE(test_api_->SameDragType(TYPE_PLATFORM_APP, TYPE_PLATFORM_APP));
  EXPECT_FALSE(test_api_->SameDragType(TYPE_PLATFORM_APP, TYPE_APP_SHORTCUT));
  EXPECT_FALSE(test_api_->SameDragType(TYPE_PLATFORM_APP,
                                       TYPE_BROWSER_SHORTCUT));
  EXPECT_FALSE(test_api_->SameDragType(TYPE_PLATFORM_APP, TYPE_WINDOWED_APP));
  EXPECT_FALSE(test_api_->SameDragType(TYPE_PLATFORM_APP, TYPE_APP_LIST));
  EXPECT_FALSE(test_api_->SameDragType(TYPE_PLATFORM_APP, TYPE_APP_PANEL));

  EXPECT_TRUE(test_api_->SameDragType(TYPE_APP_SHORTCUT, TYPE_APP_SHORTCUT));
  EXPECT_TRUE(test_api_->SameDragType(TYPE_APP_SHORTCUT,
                                      TYPE_BROWSER_SHORTCUT));
  EXPECT_FALSE(test_api_->SameDragType(TYPE_APP_SHORTCUT,
                                       TYPE_WINDOWED_APP));
  EXPECT_FALSE(test_api_->SameDragType(TYPE_APP_SHORTCUT, TYPE_APP_LIST));
  EXPECT_FALSE(test_api_->SameDragType(TYPE_APP_SHORTCUT, TYPE_APP_PANEL));

  EXPECT_TRUE(test_api_->SameDragType(TYPE_BROWSER_SHORTCUT,
                                      TYPE_BROWSER_SHORTCUT));
  EXPECT_FALSE(test_api_->SameDragType(TYPE_BROWSER_SHORTCUT,
                                       TYPE_WINDOWED_APP));
  EXPECT_FALSE(test_api_->SameDragType(TYPE_BROWSER_SHORTCUT, TYPE_APP_LIST));
  EXPECT_FALSE(test_api_->SameDragType(TYPE_BROWSER_SHORTCUT, TYPE_APP_PANEL));

  EXPECT_TRUE(test_api_->SameDragType(TYPE_WINDOWED_APP, TYPE_WINDOWED_APP));
  EXPECT_FALSE(test_api_->SameDragType(TYPE_WINDOWED_APP, TYPE_APP_LIST));
  EXPECT_FALSE(test_api_->SameDragType(TYPE_WINDOWED_APP, TYPE_APP_PANEL));

  EXPECT_TRUE(test_api_->SameDragType(TYPE_APP_LIST, TYPE_APP_LIST));
  EXPECT_FALSE(test_api_->SameDragType(TYPE_APP_LIST, TYPE_APP_PANEL));

  EXPECT_TRUE(test_api_->SameDragType(TYPE_APP_PANEL, TYPE_APP_PANEL));
}

// Adds platform app button until overflow and verifies that the last added
// platform app button is hidden.
TEST_F(ShelfViewTest, AddBrowserUntilOverflow) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetButtonCount(),
            test_api_->GetLastVisibleIndex() + 1);

  // Add platform app button until overflow.
  int items_added = 0;
  ShelfID last_added = AddPlatformApp();
  while (!test_api_->IsOverflowButtonVisible()) {
    // Added button is visible after animation while in this loop.
    EXPECT_TRUE(GetButtonByID(last_added)->visible());

    last_added = AddPlatformApp();
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
  ASSERT_EQ(test_api_->GetButtonCount(),
            test_api_->GetLastVisibleIndex() + 1);

  ShelfID browser_button_id = AddPlatformApp();

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
  ASSERT_EQ(test_api_->GetButtonCount(),
            test_api_->GetLastVisibleIndex() + 1);

  // Add platform app button until overflow, remember last visible platform app
  // button.
  int items_added = 0;
  ShelfID first_added = AddPlatformApp();
  EXPECT_TRUE(GetButtonByID(first_added)->visible());
  while (true) {
    ShelfID added = AddPlatformApp();
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
  ASSERT_EQ(test_api_->GetButtonCount(),
            test_api_->GetLastVisibleIndex() + 1);

  // Add platform app button.
  ShelfID platform_app = AddPlatformApp();
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
    platform_app = AddPlatformApp();
    ++items_added;
    ASSERT_LT(items_added, 10000);
  }
  EXPECT_TRUE(GetButtonByID(last_panel)->visible());
  EXPECT_FALSE(GetButtonByID(platform_app)->visible());
}

// Adds button until overflow then removes first added one. Verifies that
// the last added one changes from invisible to visible and overflow
// chevron is gone.
TEST_F(ShelfViewTest, RemoveButtonRevealsOverflowed) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetButtonCount(),
            test_api_->GetLastVisibleIndex() + 1);

  // Add platform app buttons until overflow.
  int items_added = 0;
  ShelfID first_added = AddPlatformApp();
  ShelfID last_added = first_added;
  while (!test_api_->IsOverflowButtonVisible()) {
    last_added = AddPlatformApp();
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
  ASSERT_EQ(test_api_->GetButtonCount(),
            test_api_->GetLastVisibleIndex() + 1);

  // Add platform app button until overflow.
  int items_added = 0;
  ShelfID last_added = AddPlatformApp();
  while (!test_api_->IsOverflowButtonVisible()) {
    last_added = AddPlatformApp();
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
  ASSERT_EQ(test_api_->GetButtonCount(),
            test_api_->GetLastVisibleIndex() + 1);

  // Add a few platform buttons quickly without wait for animation.
  int added_count = 0;
  while (!test_api_->IsOverflowButtonVisible()) {
    AddPlatformAppNoWait();
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
  ShelfButtonHost* button_host = shelf_view_;

  std::vector<std::pair<ShelfID, views::View*> > id_map;
  SetupForDragTest(&id_map);

  // Dragging browser shortcut at index 1.
  EXPECT_TRUE(model_->items()[1].type == TYPE_BROWSER_SHORTCUT);
  views::View* dragged_button = SimulateDrag(ShelfButtonHost::MOUSE, 1, 3);
  std::rotate(id_map.begin() + 1,
              id_map.begin() + 2,
              id_map.begin() + 4);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  button_host->PointerReleasedOnButton(
      dragged_button, ShelfButtonHost::MOUSE, false);
  EXPECT_TRUE(model_->items()[3].type == TYPE_BROWSER_SHORTCUT);

  // Dragging changes model order.
  dragged_button = SimulateDrag(ShelfButtonHost::MOUSE, 1, 3);
  std::rotate(id_map.begin() + 1,
              id_map.begin() + 2,
              id_map.begin() + 4);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // Cancelling the drag operation restores previous order.
  button_host->PointerReleasedOnButton(
      dragged_button, ShelfButtonHost::MOUSE, true);
  std::rotate(id_map.begin() + 1,
              id_map.begin() + 3,
              id_map.begin() + 4);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // Deleting an item keeps the remaining intact.
  dragged_button = SimulateDrag(ShelfButtonHost::MOUSE, 1, 3);
  model_->RemoveItemAt(1);
  id_map.erase(id_map.begin() + 1);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  button_host->PointerReleasedOnButton(
      dragged_button, ShelfButtonHost::MOUSE, false);

  // Adding a shelf item cancels the drag and respects the order.
  dragged_button = SimulateDrag(ShelfButtonHost::MOUSE, 1, 3);
  ShelfID new_id = AddAppShortcut();
  id_map.insert(id_map.begin() + 6,
                std::make_pair(new_id, GetButtonByID(new_id)));
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  button_host->PointerReleasedOnButton(
      dragged_button, ShelfButtonHost::MOUSE, false);

  // Adding a shelf item at the end (i.e. a panel)  canels drag and respects
  // the order.
  dragged_button = SimulateDrag(ShelfButtonHost::MOUSE, 1, 3);
  new_id = AddPanel();
  id_map.insert(id_map.begin() + 7,
                std::make_pair(new_id, GetButtonByID(new_id)));
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  button_host->PointerReleasedOnButton(
      dragged_button, ShelfButtonHost::MOUSE, false);
}

// Check that 2nd drag from the other pointer would be ignored.
TEST_F(ShelfViewTest, SimultaneousDrag) {
  ShelfButtonHost* button_host = shelf_view_;

  std::vector<std::pair<ShelfID, views::View*> > id_map;
  SetupForDragTest(&id_map);

  // Start a mouse drag.
  views::View* dragged_button_mouse =
      SimulateDrag(ShelfButtonHost::MOUSE, 1, 3);
  std::rotate(id_map.begin() + 1,
              id_map.begin() + 2,
              id_map.begin() + 4);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  // Attempt a touch drag before the mouse drag finishes.
  views::View* dragged_button_touch =
      SimulateDrag(ShelfButtonHost::TOUCH, 4, 2);

  // Nothing changes since 2nd drag is ignored.
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // Finish the mouse drag.
  button_host->PointerReleasedOnButton(
      dragged_button_mouse, ShelfButtonHost::MOUSE, false);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // Now start a touch drag.
  dragged_button_touch = SimulateDrag(ShelfButtonHost::TOUCH, 4, 2);
  std::rotate(id_map.begin() + 3,
              id_map.begin() + 4,
              id_map.begin() + 5);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // And attempt a mouse drag before the touch drag finishes.
  dragged_button_mouse = SimulateDrag(ShelfButtonHost::MOUSE, 1, 2);

  // Nothing changes since 2nd drag is ignored.
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  button_host->PointerReleasedOnButton(
      dragged_button_touch, ShelfButtonHost::TOUCH, false);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
}

// Check that clicking first on one item and then dragging another works as
// expected.
TEST_F(ShelfViewTest, ClickOneDragAnother) {
  ShelfButtonHost* button_host = shelf_view_;

  std::vector<std::pair<ShelfID, views::View*> > id_map;
  SetupForDragTest(&id_map);

  // A click on item 1 is simulated.
  SimulateClick(ShelfButtonHost::MOUSE, 1);

  // Dragging browser index at 0 should change the model order correctly.
  EXPECT_TRUE(model_->items()[1].type == TYPE_BROWSER_SHORTCUT);
  views::View* dragged_button = SimulateDrag(ShelfButtonHost::MOUSE, 1, 3);
  std::rotate(id_map.begin() + 1,
              id_map.begin() + 2,
              id_map.begin() + 4);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  button_host->PointerReleasedOnButton(
      dragged_button, ShelfButtonHost::MOUSE, false);
  EXPECT_TRUE(model_->items()[3].type == TYPE_BROWSER_SHORTCUT);
}

// Check that clicking an item and jittering the mouse a bit still selects the
// item.
TEST_F(ShelfViewTest, ClickAndMoveSlightly) {
  std::vector<std::pair<ShelfID, views::View*> > id_map;
  SetupForDragTest(&id_map);

  ShelfID shelf_id = (id_map.begin() + 1)->first;
  views::View* button = (id_map.begin() + 1)->second;

  // Replace the ShelfItemDelegate for |shelf_id| with one which tracks whether
  // the shelf item gets selected.
  ShelfItemSelectionTracker* selection_tracker = new ShelfItemSelectionTracker;
  item_manager_->SetShelfItemDelegate(
      shelf_id,
      scoped_ptr<ShelfItemDelegate>(selection_tracker).Pass());

  gfx::Vector2d press_offset(5, 30);
  gfx::Point press_location = gfx::Point() + press_offset;
  gfx::Point press_location_in_screen =
      button->GetBoundsInScreen().origin() + press_offset;

  ui::MouseEvent click_event(ui::ET_MOUSE_PRESSED,
                             press_location,
                             press_location_in_screen,
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMousePressed(click_event);

  ui::MouseEvent drag_event1(ui::ET_MOUSE_DRAGGED,
                             press_location + gfx::Vector2d(0, 1),
                             press_location_in_screen + gfx::Vector2d(0, 1),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseDragged(drag_event1);

  ui::MouseEvent drag_event2(ui::ET_MOUSE_DRAGGED,
                             press_location + gfx::Vector2d(-1, 0),
                             press_location_in_screen + gfx::Vector2d(-1, 0),
                             ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseDragged(drag_event2);

  ui::MouseEvent release_event(ui::ET_MOUSE_RELEASED,
                               press_location + gfx::Vector2d(-1, 0),
                               press_location_in_screen + gfx::Vector2d(-1, 0),
                               ui::EF_LEFT_MOUSE_BUTTON, 0);
  button->OnMouseReleased(release_event);

  EXPECT_TRUE(selection_tracker->WasSelected());
}

// Confirm that item status changes are reflected in the buttons.
TEST_F(ShelfViewTest, ShelfItemStatus) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetButtonCount(),
            test_api_->GetLastVisibleIndex() + 1);

  // Add platform app button.
  ShelfID last_added = AddPlatformApp();
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
  ASSERT_EQ(test_api_->GetButtonCount(),
            test_api_->GetLastVisibleIndex() + 1);

  // Add platform app button.
  ShelfID last_added = AddPlatformApp();
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
  shelf_view_->shelf_layout_manager()->SetAutoHideBehavior(
      SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  test_api_->RunMessageLoopUntilAnimationsDone();
  VerifyShelfItemBoundsAreValid();
  shelf_view_->shelf_layout_manager()->SetAutoHideBehavior(
      SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  test_api_->RunMessageLoopUntilAnimationsDone();
  VerifyShelfItemBoundsAreValid();
}

TEST_F(ShelfViewTest, ShelfTooltipTest) {
  ASSERT_EQ(test_api_->GetLastVisibleIndex() + 1,
            test_api_->GetButtonCount());

  // Prepare some items to the shelf.
  ShelfID app_button_id = AddAppShortcut();
  ShelfID platform_button_id = AddPlatformApp();

  ShelfButton* app_button = GetButtonByID(app_button_id);
  ShelfButton* platform_button = GetButtonByID(platform_button_id);

  ShelfButtonHost* button_host = shelf_view_;
  ShelfTooltipManager* tooltip_manager = shelf_view_->tooltip_manager();

  button_host->MouseEnteredButton(app_button);
  // There's a delay to show the tooltip, so it's not visible yet.
  EXPECT_FALSE(tooltip_manager->IsVisible());
  EXPECT_EQ(app_button, GetTooltipAnchorView());

  ShowTooltip();
  EXPECT_TRUE(tooltip_manager->IsVisible());

  // Once it's visible, it keeps visibility and is pointing to the same
  // item.
  button_host->MouseExitedButton(app_button);
  EXPECT_TRUE(tooltip_manager->IsVisible());
  EXPECT_EQ(app_button, GetTooltipAnchorView());

  // When entered to another item, it switches to the new item.  There is no
  // delay for the visibility.
  button_host->MouseEnteredButton(platform_button);
  EXPECT_TRUE(tooltip_manager->IsVisible());
  EXPECT_EQ(platform_button, GetTooltipAnchorView());

  button_host->MouseExitedButton(platform_button);
  tooltip_manager->Close();

  // Next time: enter app_button -> move immediately to tab_button.
  button_host->MouseEnteredButton(app_button);
  button_host->MouseExitedButton(app_button);
  button_host->MouseEnteredButton(platform_button);
  EXPECT_FALSE(tooltip_manager->IsVisible());
  EXPECT_EQ(platform_button, GetTooltipAnchorView());
}

// Verify a fix for crash caused by a tooltip update for a deletedshelf
// button, see crbug.com/288838.
TEST_F(ShelfViewTest, RemovingItemClosesTooltip) {
  ShelfButtonHost* button_host = shelf_view_;
  ShelfTooltipManager* tooltip_manager = shelf_view_->tooltip_manager();

  // Add an item to the shelf.
  ShelfID app_button_id = AddAppShortcut();
  ShelfButton* app_button = GetButtonByID(app_button_id);

  // Spawn a tooltip on that item.
  button_host->MouseEnteredButton(app_button);
  ShowTooltip();
  EXPECT_TRUE(tooltip_manager->IsVisible());

  // Remove the app shortcut while the tooltip is open. The tooltip should be
  // closed.
  RemoveByID(app_button_id);
  EXPECT_FALSE(tooltip_manager->IsVisible());

  // Change the shelf layout. This should not crash.
  Shell::GetInstance()->SetShelfAlignment(SHELF_ALIGNMENT_LEFT,
                                          Shell::GetPrimaryRootWindow());
}

// Changing the shelf alignment closes any open tooltip.
TEST_F(ShelfViewTest, ShelfAlignmentClosesTooltip) {
  ShelfButtonHost* button_host = shelf_view_;
  ShelfTooltipManager* tooltip_manager = shelf_view_->tooltip_manager();

  // Add an item to the shelf.
  ShelfID app_button_id = AddAppShortcut();
  ShelfButton* app_button = GetButtonByID(app_button_id);

  // Spawn a tooltip on the item.
  button_host->MouseEnteredButton(app_button);
  ShowTooltip();
  EXPECT_TRUE(tooltip_manager->IsVisible());

  // Changing shelf alignment hides the tooltip.
  Shell::GetInstance()->SetShelfAlignment(SHELF_ALIGNMENT_LEFT,
                                          Shell::GetPrimaryRootWindow());
  EXPECT_FALSE(tooltip_manager->IsVisible());
}

TEST_F(ShelfViewTest, ShouldHideTooltipTest) {
  ShelfID app_button_id = AddAppShortcut();
  ShelfID platform_button_id = AddPlatformApp();

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
  views::View* app_list_button = shelf_view_->GetAppListButtonView();
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
  all_area.Union(shelf_view_->GetAppListButtonView()->GetMirroredBounds());
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
  Shell::GetInstance()->ToggleAppList(NULL);
  ASSERT_TRUE(Shell::GetInstance()->GetAppListWindow());

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
  views::View* app_list_button = shelf_view_->GetAppListButtonView();
  EXPECT_TRUE(shelf_view_->ShouldHideTooltip(
      app_list_button->GetMirroredBounds().CenterPoint()));
}

// Test that by moving the mouse cursor off the button onto the bubble it closes
// the bubble.
TEST_F(ShelfViewTest, ShouldHideTooltipWhenHoveringOnTooltip) {
  ShelfTooltipManager* tooltip_manager = shelf_view_->tooltip_manager();
  tooltip_manager->CreateZeroDelayTimerForTest();
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  // Move the mouse off any item and check that no tooltip is shown.
  generator.MoveMouseTo(gfx::Point(0, 0));
  EXPECT_FALSE(tooltip_manager->IsVisible());

  // Move the mouse over the button and check that it is visible.
  views::View* app_list_button = shelf_view_->GetAppListButtonView();
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
  ASSERT_EQ(test_api_->GetButtonCount(),
            test_api_->GetLastVisibleIndex() + 1);

  // Add buttons until overflow. Let the non-overflow add animations finish but
  // leave the last running.
  int items_added = 0;
  AddPlatformAppNoWait();
  while (!test_api_->IsOverflowButtonVisible()) {
    test_api_->RunMessageLoopUntilAnimationsDone();
    AddPlatformAppNoWait();
    ++items_added;
    ASSERT_LT(items_added, 10000);
  }

  // Resize shelf view with that animation running and stay overflown.
  gfx::Rect bounds = shelf_view_->bounds();
  bounds.set_width(bounds.width() - kShelfSize);
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
  // Replace current ShelfDelegate with TestShelfDelegateForShelfView.
  ReplaceShelfDelegateForRipOffTest();

  AddButtonsUntilOverflow();

  // Show overflow bubble.
  test_api_->ShowOverflowBubble();
  ASSERT_TRUE(test_api_->overflow_bubble() &&
              test_api_->overflow_bubble()->IsShowing());

  ShelfViewTestAPI test_for_overflow_view(
      test_api_->overflow_bubble()->shelf_view());

  int ripped_index = test_for_overflow_view.GetLastVisibleIndex();
  gfx::Size bubble_size = test_for_overflow_view.GetPreferredSize();
  int item_width = test_for_overflow_view.GetButtonSize() +
      test_for_overflow_view.GetButtonSpacing();

  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     gfx::Point());
  ShelfButton* button = test_for_overflow_view.GetButton(ripped_index);
  // Rip off the last visible item.
  gfx::Point start_point = button->GetBoundsInScreen().CenterPoint();
  gfx::Point rip_off_point(start_point.x(), 0);
  generator.MoveMouseTo(start_point.x(), start_point.y());
  base::MessageLoop::current()->RunUntilIdle();
  generator.PressLeftButton();
  base::MessageLoop::current()->RunUntilIdle();
  generator.MoveMouseTo(rip_off_point.x(), rip_off_point.y());
  base::MessageLoop::current()->RunUntilIdle();
  test_for_overflow_view.RunMessageLoopUntilAnimationsDone();

  // Check the overflow bubble size when an item is ripped off.
  EXPECT_EQ(bubble_size.width() - item_width,
            test_for_overflow_view.GetPreferredSize().width());
  ASSERT_TRUE(test_api_->overflow_bubble() &&
              test_api_->overflow_bubble()->IsShowing());

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
  ASSERT_TRUE(test_api_->overflow_bubble() &&
              test_api_->overflow_bubble()->IsShowing());

  int item_width = test_api_->GetButtonSize() +
      test_api_->GetButtonSpacing();
  OverflowBubbleView* bubble_view = test_api_->overflow_bubble()->bubble_view();
  test::OverflowBubbleViewTestAPI bubble_view_api(bubble_view);

  // Add more buttons until OverflowBubble is scrollable and it has 3 invisible
  // items.
  while (bubble_view_api.GetContentsSize().width() <
         (bubble_view->GetContentsBounds().width() + 3 * item_width))
    AddAppShortcut();

  ASSERT_TRUE(test_api_->overflow_bubble() &&
              test_api_->overflow_bubble()->IsShowing());

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

  // Scrolls sufficiently to show last item.
  bubble_view_api.ScrollByXOffset(3 * item_width);
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
  Shelf* secondary_shelf = Shelf::ForWindow(Shell::GetAllRootWindows()[1]);
  ShelfView* shelf_view_for_secondary =
      ShelfTestAPI(secondary_shelf).shelf_view();

  // The bounds should be big enough for 4 buttons + overflow chevron.
  shelf_view_for_secondary->SetBounds(0, 0, 500, kShelfSize);

  ShelfViewTestAPI test_api_for_secondary(shelf_view_for_secondary);
  // Speeds up animation for test.
  test_api_for_secondary.SetAnimationDuration(1);

  AddButtonsUntilOverflow();

  // Test #1: Test drag insertion bounds of primary shelf.
  // Show overflow bubble.
  test_api_->ShowOverflowBubble();
  ASSERT_TRUE(test_api_->overflow_bubble() &&
              test_api_->overflow_bubble()->IsShowing());

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
  EXPECT_FALSE(drag_reinsert_bounds.Contains(
      gfx::Point(point_in_shelf_view.x(), 0)));

  // Test #2: Test drag insertion bounds of secondary shelf.
  // Show overflow bubble.
  test_api_for_secondary.ShowOverflowBubble();
  ASSERT_TRUE(test_api_for_secondary.overflow_bubble() &&
              test_api_for_secondary.overflow_bubble()->IsShowing());

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
  ASSERT_EQ(2U, Shell::GetAllRootWindows().size());

  aura::Window* second_root = Shell::GetAllRootWindows()[1];

  Shell::GetInstance()->SetShelfAlignment(SHELF_ALIGNMENT_LEFT, second_root);
  ASSERT_EQ(SHELF_ALIGNMENT_LEFT,
            Shell::GetInstance()->GetShelfAlignment(second_root));

  // Initially, app list and browser shortcut are added.
  EXPECT_EQ(2, model_->item_count());
  int browser_index = model_->GetItemIndexForType(TYPE_BROWSER_SHORTCUT);
  EXPECT_GT(browser_index, 0);

  Shelf* secondary_shelf = Shelf::ForWindow(second_root);
  ShelfView* shelf_view_for_secondary =
      ShelfTestAPI(secondary_shelf).shelf_view();

  ShelfViewTestAPI test_api_for_secondary_shelf_view(shelf_view_for_secondary);
  ShelfButton* button =
      test_api_for_secondary_shelf_view.GetButton(browser_index);

  // Fetch the start point of dragging.
  gfx::Point start_point = button->GetBoundsInScreen().CenterPoint();
  ::wm::ConvertPointFromScreen(second_root, &start_point);

  ui::test::EventGenerator generator(second_root, start_point);

  // Rip off the browser item.
  generator.PressLeftButton();
  generator.MoveMouseTo(start_point.x() + 400, start_point.y());
  test_api_for_secondary_shelf_view.RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(test_api_for_secondary_shelf_view.IsRippedOffFromShelf());
}

// Checks various drag and drop operations from OverflowBubble to Shelf.
TEST_F(ShelfViewTest, CheckDragAndDropFromOverflowBubbleToShelf) {
  // Replace current ShelfDelegate with TestShelfDelegateForShelfView.
  ReplaceShelfDelegateForRipOffTest();

  AddButtonsUntilOverflow();

  TestDraggingAnItemFromOverflowToShelf(false);
  TestDraggingAnItemFromOverflowToShelf(true);
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
      if (ShelfButton* button = test_api_->GetButton(i))
        EXPECT_TRUE(visible_bounds.Contains(button->GetBoundsInScreen()));
    CheckAppListButtonIsInBounds();
  }

  void CheckAppListButtonIsInBounds() {
    gfx::Rect visible_bounds = shelf_view_->GetVisibleItemsBoundsInScreen();
    gfx::Rect app_list_button_bounds = shelf_view_->GetAppListButtonView()->
       GetBoundsInScreen();
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
INSTANTIATE_TEST_CASE_P(VisibleBounds, ShelfViewVisibleBoundsTest,
    testing::Bool());

}  // namespace test
}  // namespace ash
