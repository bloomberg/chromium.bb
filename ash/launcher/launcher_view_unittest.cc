// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_view.h"

#include <algorithm>
#include <vector>

#include "ash/launcher/launcher.h"
#include "ash/launcher/launcher_button.h"
#include "ash/launcher/launcher_icon_observer.h"
#include "ash/launcher/launcher_model.h"
#include "ash/launcher/launcher_tooltip_manager.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/launcher_view_test_api.h"
#include "ash/test/test_launcher_delegate.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "grit/ui_resources.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/window.h"
#include "ui/base/events/event.h"
#include "ui/base/events/event_constants.h"
#include "ui/compositor/layer.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {
const int kExpectedAppIndex = 2;
}

namespace ash {
namespace test {

////////////////////////////////////////////////////////////////////////////////
// LauncherIconObserver tests.

class TestLauncherIconObserver : public LauncherIconObserver {
 public:
  explicit TestLauncherIconObserver(Launcher* launcher)
      : launcher_(launcher),
        change_notified_(false) {
    if (launcher_)
      launcher_->AddIconObserver(this);
  }

  virtual ~TestLauncherIconObserver() {
    if (launcher_)
      launcher_->RemoveIconObserver(this);
  }

  // LauncherIconObserver implementation.
  virtual void OnLauncherIconPositionsChanged() OVERRIDE {
    change_notified_ = true;
  }

  int change_notified() const { return change_notified_; }
  void Reset() { change_notified_ = false; }

 private:
  Launcher* launcher_;
  bool change_notified_;

  DISALLOW_COPY_AND_ASSIGN(TestLauncherIconObserver);
};

class LauncherViewIconObserverTest : public ash::test::AshTestBase {
 public:
  LauncherViewIconObserverTest() {}
  virtual ~LauncherViewIconObserverTest() {}

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();

    Launcher* launcher = Shell::GetInstance()->launcher();
    observer_.reset(new TestLauncherIconObserver(launcher));

    launcher_view_test_.reset(new LauncherViewTestAPI(
        launcher->GetLauncherViewForTest()));
    launcher_view_test_->SetAnimationDuration(1);
  }

  virtual void TearDown() OVERRIDE {
    observer_.reset();
    AshTestBase::TearDown();
  }

  TestLauncherIconObserver* observer() { return observer_.get(); }

  LauncherViewTestAPI* launcher_vew_test() {
    return launcher_view_test_.get();
  }

 private:
  scoped_ptr<TestLauncherIconObserver> observer_;
  scoped_ptr<LauncherViewTestAPI> launcher_view_test_;

  DISALLOW_COPY_AND_ASSIGN(LauncherViewIconObserverTest);
};

TEST_F(LauncherViewIconObserverTest, AddRemove) {
  ash::test::TestLauncherDelegate* launcher_delegate =
      ash::test::TestLauncherDelegate::instance();
  ASSERT_TRUE(launcher_delegate);

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 200, 200);

  scoped_ptr<views::Widget> widget(new views::Widget());
  widget->Init(params);
  launcher_delegate->AddLauncherItem(widget->GetNativeWindow());
  launcher_vew_test()->RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(observer()->change_notified());
  observer()->Reset();

  widget->Show();
  widget->GetNativeWindow()->parent()->RemoveChild(widget->GetNativeWindow());
  launcher_vew_test()->RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(observer()->change_notified());
  observer()->Reset();
}

TEST_F(LauncherViewIconObserverTest, BoundsChanged) {
  Launcher* launcher = Shell::GetInstance()->launcher();
  gfx::Size launcher_size =
      launcher->widget()->GetWindowBoundsInScreen().size();
  int total_width = launcher_size.width() / 2;
  ASSERT_GT(total_width, 0);
  launcher->SetStatusSize(gfx::Size(total_width, launcher_size.height()));
  // No animation happens for LauncherView bounds change.
  EXPECT_TRUE(observer()->change_notified());
  observer()->Reset();
}

////////////////////////////////////////////////////////////////////////////////
// LauncherView tests.

class MockLauncherDelegate : public ash::LauncherDelegate {
 public:
  MockLauncherDelegate() {}
  virtual ~MockLauncherDelegate() {}

  // LauncherDelegate overrides:
  virtual void CreateNewTab() OVERRIDE {}
  virtual void CreateNewWindow() OVERRIDE {}
  virtual void ItemClicked(const ash::LauncherItem& item,
                           int event_flags) OVERRIDE {}
  virtual int GetBrowserShortcutResourceId() OVERRIDE {
    return IDR_AURA_LAUNCHER_BROWSER_SHORTCUT;
  }
  virtual string16 GetTitle(const ash::LauncherItem& item) OVERRIDE {
    return string16();
  }
  virtual ui::MenuModel* CreateContextMenu(
      const ash::LauncherItem& item) OVERRIDE {
    return NULL;
  }
  virtual ui::MenuModel* CreateContextMenuForLauncher() OVERRIDE {
    return NULL;
  }
  virtual ash::LauncherID GetIDByWindow(aura::Window* window) OVERRIDE {
    NOTREACHED();
    return -1;
  }
  virtual bool IsDraggable(const ash::LauncherItem& item) OVERRIDE {
    return true;
  }
};

class LauncherViewTest : public AshTestBase {
 public:
  LauncherViewTest() {}
  virtual ~LauncherViewTest() {}

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();

    model_.reset(new LauncherModel);

    launcher_view_.reset(new internal::LauncherView(
        model_.get(), &delegate_, NULL));
    launcher_view_->Init();
    // The bounds should be big enough for 4 buttons + overflow chevron.
    launcher_view_->SetBounds(0, 0, 500, 50);

    test_api_.reset(new LauncherViewTestAPI(launcher_view_.get()));
    test_api_->SetAnimationDuration(1);  // Speeds up animation for test.
  }

  virtual void TearDown() OVERRIDE {
    launcher_view_.reset();
    AshTestBase::TearDown();
  }

 protected:
  LauncherID AddAppShortcut() {
    LauncherItem item;
    item.type = TYPE_APP_SHORTCUT;
    item.status = STATUS_CLOSED;

    LauncherID id = model_->next_id();
    model_->Add(item);
    test_api_->RunMessageLoopUntilAnimationsDone();
    return id;
  }

  LauncherID AddTabbedBrowserNoWait() {
    LauncherItem item;
    item.type = TYPE_TABBED;
    item.status = STATUS_RUNNING;

    LauncherID id = model_->next_id();
    model_->Add(item);
    return id;
  }

  LauncherID AddTabbedBrowser() {
    LauncherID id = AddTabbedBrowserNoWait();
    test_api_->RunMessageLoopUntilAnimationsDone();
    return id;
  }

  LauncherID AddPlatformAppNoWait() {
    LauncherItem item;
    item.type = TYPE_PLATFORM_APP;
    item.status = STATUS_RUNNING;

    LauncherID id = model_->next_id();
    model_->Add(item);
    return id;
  }

  LauncherID AddPlatformApp() {
    LauncherID id = AddPlatformAppNoWait();
    test_api_->RunMessageLoopUntilAnimationsDone();
    return id;
  }

  void RemoveByID(LauncherID id) {
    model_->RemoveItemAt(model_->ItemIndexByID(id));
    test_api_->RunMessageLoopUntilAnimationsDone();
  }

  internal::LauncherButton* GetButtonByID(LauncherID id) {
    int index = model_->ItemIndexByID(id);
    return test_api_->GetButton(index);
  }

  LauncherItem GetItemByID(LauncherID id) {
    LauncherItems::const_iterator items = model_->ItemByID(id);
    return *items;
  }

  void CheckModelIDs(
      const std::vector<std::pair<LauncherID, views::View*> >& id_map) {
    size_t map_index = 0;
    for (size_t model_index = 0;
         model_index < model_->items().size();
         ++model_index) {
      ash::LauncherItem item = model_->items()[model_index];
//      if (item.type == ash::TYPE_APP_LIST)
//        continue;
      ash::LauncherID id = item.id;
      EXPECT_EQ(id_map[map_index].first, id);
      EXPECT_EQ(id_map[map_index].second, GetButtonByID(id));
      ++map_index;
    }
    ASSERT_EQ(map_index, id_map.size());
  }

  views::View* SimulateDrag(internal::LauncherButtonHost::Pointer pointer,
                            int button_index,
                            int destination_index) {
    // Add kExpectedAppIndex to each button index to allow default icons.
    internal::LauncherButtonHost* button_host = launcher_view_.get();

    // Mouse down.
    views::View* button =
        test_api_->GetButton(kExpectedAppIndex + button_index);
    ui::MouseEvent click_event(ui::ET_MOUSE_PRESSED,
                               button->bounds().origin(),
                               button->bounds().origin(), 0);
    button_host->PointerPressedOnButton(button, pointer, click_event);

    // Drag.
    views::View* destination =
        test_api_->GetButton(kExpectedAppIndex + destination_index);
    ui::MouseEvent drag_event(ui::ET_MOUSE_DRAGGED,
                              destination->bounds().origin(),
                              destination->bounds().origin(), 0);
    button_host->PointerDraggedOnButton(button, pointer, drag_event);
    return button;
  }

  void SetupForDragTest(
      std::vector<std::pair<LauncherID, views::View*> >* id_map) {
    // Initialize |id_map| with the automatically-created launcher buttons.
    for (size_t i = 0; i < model_->items().size(); ++i) {
      internal::LauncherButton* button = test_api_->GetButton(i);
//      if (!button)
//        continue;

      id_map->push_back(std::make_pair(model_->items()[i].id, button));
    }
    ASSERT_NO_FATAL_FAILURE(CheckModelIDs(*id_map));

    // Add 5 app launcher buttons for testing.
    for (int i = 0; i < 5; ++i) {
      LauncherID id = AddAppShortcut();
      id_map->push_back(std::make_pair(id, GetButtonByID(id)));
    }
    ASSERT_NO_FATAL_FAILURE(CheckModelIDs(*id_map));
  }

  views::View* GetTooltipAnchorView() {
    return launcher_view_->tooltip_manager()->anchor_;
  }

  void ShowTooltip() {
    launcher_view_->tooltip_manager()->ShowInternal();
  }

  MockLauncherDelegate delegate_;
  scoped_ptr<LauncherModel> model_;
  scoped_ptr<internal::LauncherView> launcher_view_;
  scoped_ptr<LauncherViewTestAPI> test_api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LauncherViewTest);
};

// Adds browser button until overflow and verifies that the last added browser
// button is hidden.
TEST_F(LauncherViewTest, AddBrowserUntilOverflow) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetLastVisibleIndex() + 1,
            test_api_->GetButtonCount());

  // Add tabbed browser until overflow.
  LauncherID last_added = AddTabbedBrowser();
  while (!test_api_->IsOverflowButtonVisible()) {
    // Added button is visible after animation while in this loop.
    EXPECT_TRUE(GetButtonByID(last_added)->visible());

    last_added = AddTabbedBrowser();
  }

  // The last added button should be invisible.
  EXPECT_FALSE(GetButtonByID(last_added)->visible());
}

// Adds one browser button then adds app shortcut until overflow. Verifies that
// the browser button gets hidden on overflow and last added app shortcut is
// still visible.
TEST_F(LauncherViewTest, AddAppShortcutWithBrowserButtonUntilOverflow) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetLastVisibleIndex() + 1,
            test_api_->GetButtonCount());

  LauncherID browser_button_id = AddTabbedBrowser();

  // Add app shortcut until overflow.
  LauncherID last_added = AddAppShortcut();
  while (!test_api_->IsOverflowButtonVisible()) {
    // Added button is visible after animation while in this loop.
    EXPECT_TRUE(GetButtonByID(last_added)->visible());

    last_added = AddAppShortcut();
  }

  // The last added app short button should be visible.
  EXPECT_TRUE(GetButtonByID(last_added)->visible());
  // And the browser button is invisible.
  EXPECT_FALSE(GetButtonByID(browser_button_id)->visible());
}

// Adds button until overflow then removes first added one. Verifies that
// the last added one changes from invisible to visible and overflow
// chevron is gone.
TEST_F(LauncherViewTest, RemoveButtonRevealsOverflowed) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetLastVisibleIndex() + 1,
            test_api_->GetButtonCount());

  // Add tabbed browser until overflow.
  LauncherID first_added= AddTabbedBrowser();
  LauncherID last_added = first_added;
  while (!test_api_->IsOverflowButtonVisible())
    last_added = AddTabbedBrowser();

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
TEST_F(LauncherViewTest, RemoveLastOverflowed) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetLastVisibleIndex() + 1,
            test_api_->GetButtonCount());

  // Add tabbed browser until overflow.
  LauncherID last_added= AddTabbedBrowser();
  while (!test_api_->IsOverflowButtonVisible())
    last_added = AddTabbedBrowser();

  RemoveByID(last_added);
  EXPECT_FALSE(test_api_->IsOverflowButtonVisible());
}

// Adds browser button without waiting for animation to finish and verifies
// that all added buttons are visible.
TEST_F(LauncherViewTest, AddButtonQuickly) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetLastVisibleIndex() + 1,
            test_api_->GetButtonCount());

  // Add a few tabbed browser quickly without wait for animation.
  int added_count = 0;
  while (!test_api_->IsOverflowButtonVisible()) {
    AddTabbedBrowserNoWait();
    ++added_count;
  }

  // LauncherView should be big enough to hold at least 3 new buttons.
  ASSERT_GE(added_count, 3);

  // Wait for the last animation to finish.
  test_api_->RunMessageLoopUntilAnimationsDone();

  // Verifies non-overflow buttons are visible.
  for (int i = 1; i <= test_api_->GetLastVisibleIndex(); ++i) {
    internal::LauncherButton* button = test_api_->GetButton(i);
    EXPECT_TRUE(button->visible()) << "button index=" << i;
    EXPECT_EQ(1.0f, button->layer()->opacity()) << "button index=" << i;
  }
}

// Check that model changes are handled correctly while a launcher icon is being
// dragged.
TEST_F(LauncherViewTest, ModelChangesWhileDragging) {
  internal::LauncherButtonHost* button_host = launcher_view_.get();

  std::vector<std::pair<LauncherID, views::View*> > id_map;
  SetupForDragTest(&id_map);

  // Dragging changes model order.
  views::View* dragged_button = SimulateDrag(
      internal::LauncherButtonHost::MOUSE, 0, 2);
  std::rotate(id_map.begin() + kExpectedAppIndex,
              id_map.begin() + kExpectedAppIndex + 1,
              id_map.begin() + kExpectedAppIndex + 3);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // Cancelling the drag operation restores previous order.
  button_host->PointerReleasedOnButton(dragged_button,
                                       internal::LauncherButtonHost::MOUSE,
                                       true);
  std::rotate(id_map.begin() + kExpectedAppIndex,
              id_map.begin() + kExpectedAppIndex + 2,
              id_map.begin() + kExpectedAppIndex + 3);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // Deleting an item keeps the remaining intact.
  dragged_button = SimulateDrag(internal::LauncherButtonHost::MOUSE, 0, 2);
  model_->RemoveItemAt(kExpectedAppIndex + 1);
  id_map.erase(id_map.begin() + kExpectedAppIndex + 1);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  button_host->PointerReleasedOnButton(dragged_button,
                                       internal::LauncherButtonHost::MOUSE,
                                       false);

  // Adding a launcher item cancels the drag and respects the order.
  dragged_button = SimulateDrag(internal::LauncherButtonHost::MOUSE, 0, 2);
  LauncherID new_id = AddAppShortcut();
  id_map.push_back(std::make_pair(new_id, GetButtonByID(new_id)));
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  button_host->PointerReleasedOnButton(dragged_button,
                                       internal::LauncherButtonHost::MOUSE,
                                       false);
}

// Check that 2nd drag from the other pointer would be ignored.
TEST_F(LauncherViewTest, SimultaneousDrag) {
  internal::LauncherButtonHost* button_host = launcher_view_.get();

  std::vector<std::pair<LauncherID, views::View*> > id_map;
  SetupForDragTest(&id_map);

  // Start a mouse drag.
  views::View* dragged_button_mouse = SimulateDrag(
      internal::LauncherButtonHost::MOUSE, 0, 2);
  std::rotate(id_map.begin() + kExpectedAppIndex,
              id_map.begin() + kExpectedAppIndex + 1,
              id_map.begin() + kExpectedAppIndex + 3);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  // Attempt a touch drag before the mouse drag finishes.
  views::View* dragged_button_touch = SimulateDrag(
      internal::LauncherButtonHost::TOUCH, 3, 1);

  // Nothing changes since 2nd drag is ignored.
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // Finish the mouse drag.
  button_host->PointerReleasedOnButton(dragged_button_mouse,
                                       internal::LauncherButtonHost::MOUSE,
                                       false);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // Now start a touch drag.
  dragged_button_touch = SimulateDrag(
      internal::LauncherButtonHost::TOUCH, 3, 1);
  std::rotate(id_map.begin() + kExpectedAppIndex + 2,
              id_map.begin() + kExpectedAppIndex + 3,
              id_map.begin() + kExpectedAppIndex + 4);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // And attempt a mouse drag before the touch drag finishes.
  dragged_button_mouse = SimulateDrag(
      internal::LauncherButtonHost::MOUSE, 0, 1);

  // Nothing changes since 2nd drag is ignored.
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  button_host->PointerReleasedOnButton(dragged_button_touch,
                                       internal::LauncherButtonHost::TOUCH,
                                       false);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
}

// Confirm that item status changes are reflected in the buttons.
TEST_F(LauncherViewTest, LauncherItemStatus) {
  ASSERT_EQ(test_api_->GetLastVisibleIndex() + 1,
            test_api_->GetButtonCount());

  // Add tabbed browser.
  LauncherID last_added = AddTabbedBrowser();
  LauncherItem item = GetItemByID(last_added);
  int index = model_->ItemIndexByID(last_added);
  internal::LauncherButton* button = GetButtonByID(last_added);
  ASSERT_EQ(internal::LauncherButton::STATE_RUNNING, button->state());
  item.status = ash::STATUS_ACTIVE;
  model_->Set(index, item);
  ASSERT_EQ(internal::LauncherButton::STATE_ACTIVE, button->state());
  item.status = ash::STATUS_ATTENTION;
  model_->Set(index, item);
  ASSERT_EQ(internal::LauncherButton::STATE_ATTENTION, button->state());
}

// Confirm that item status changes are reflected in the buttons
// for platform apps.
TEST_F(LauncherViewTest, LauncherItemStatusPlatformApp) {
  ASSERT_EQ(test_api_->GetLastVisibleIndex() + 1,
            test_api_->GetButtonCount());

  // Add tabbed browser.
  LauncherID last_added = AddPlatformApp();
  LauncherItem item = GetItemByID(last_added);
  int index = model_->ItemIndexByID(last_added);
  internal::LauncherButton* button = GetButtonByID(last_added);
  ASSERT_EQ(internal::LauncherButton::STATE_RUNNING, button->state());
  item.status = ash::STATUS_ACTIVE;
  model_->Set(index, item);
  ASSERT_EQ(internal::LauncherButton::STATE_ACTIVE, button->state());
  item.status = ash::STATUS_ATTENTION;
  model_->Set(index, item);
  ASSERT_EQ(internal::LauncherButton::STATE_ATTENTION, button->state());
}

TEST_F(LauncherViewTest, LauncherTooltipTest) {
  ASSERT_EQ(test_api_->GetLastVisibleIndex() + 1,
            test_api_->GetButtonCount());

  // Prepare some items to the launcher.
  LauncherID app_button_id = AddAppShortcut();
  LauncherID tab_button_id = AddTabbedBrowser();

  internal::LauncherButton* app_button = GetButtonByID(app_button_id);
  internal::LauncherButton* tab_button = GetButtonByID(tab_button_id);

  internal::LauncherButtonHost* button_host = launcher_view_.get();
  internal::LauncherTooltipManager* tooltip_manager =
      launcher_view_->tooltip_manager();

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
  button_host->MouseEnteredButton(tab_button);
  EXPECT_TRUE(tooltip_manager->IsVisible());
  EXPECT_EQ(tab_button, GetTooltipAnchorView());

  button_host->MouseExitedButton(tab_button);
  tooltip_manager->Close();

  // Next time: enter app_button -> move immediately to tab_button.
  button_host->MouseEnteredButton(app_button);
  button_host->MouseExitedButton(app_button);
  button_host->MouseEnteredButton(tab_button);
  EXPECT_FALSE(tooltip_manager->IsVisible());
  EXPECT_EQ(tab_button, GetTooltipAnchorView());
}

TEST_F(LauncherViewTest, ShouldHideTooltipTest) {
  LauncherID app_button_id = AddAppShortcut();
  LauncherID tab_button_id = AddTabbedBrowser();

  // The tooltip shouldn't hide if the mouse is on normal buttons.
  for (int i = 0; i < test_api_->GetButtonCount(); i++) {
    internal::LauncherButton* button = test_api_->GetButton(i);
    if (!button)
      continue;

    EXPECT_FALSE(launcher_view_->ShouldHideTooltip(
        button->GetMirroredBounds().CenterPoint()))
        << "LauncherView tries to hide on button " << i;
  }

  // The tooltip should not hide on the app-list button.
  views::View* app_list_button = launcher_view_->GetAppListButtonView();
  EXPECT_FALSE(launcher_view_->ShouldHideTooltip(
      app_list_button->GetMirroredBounds().CenterPoint()));

  // The tooltip shouldn't hide if the mouse is in the gap between two buttons.
  gfx::Rect app_button_rect = GetButtonByID(app_button_id)->GetMirroredBounds();
  gfx::Rect tab_button_rect = GetButtonByID(tab_button_id)->GetMirroredBounds();
  ASSERT_FALSE(app_button_rect.Intersects(tab_button_rect));
  EXPECT_FALSE(launcher_view_->ShouldHideTooltip(
      app_button_rect.Union(tab_button_rect).CenterPoint()));

  // The tooltip should hide if it's outside of all buttons.
  gfx::Rect all_area;
  for (int i = 0; i < test_api_->GetButtonCount(); i++) {
    internal::LauncherButton* button = test_api_->GetButton(i);
    if (!button)
      continue;

    all_area = all_area.Union(button->GetMirroredBounds());
  }
  all_area = all_area.Union(
      launcher_view_->GetAppListButtonView()->GetMirroredBounds());
  EXPECT_FALSE(launcher_view_->ShouldHideTooltip(all_area.origin()));
  EXPECT_FALSE(launcher_view_->ShouldHideTooltip(
      gfx::Point(all_area.right() - 1, all_area.bottom() - 1)));
  EXPECT_TRUE(launcher_view_->ShouldHideTooltip(
      gfx::Point(all_area.right(), all_area.y())));
  EXPECT_TRUE(launcher_view_->ShouldHideTooltip(
      gfx::Point(all_area.x() - 1, all_area.y())));
  EXPECT_TRUE(launcher_view_->ShouldHideTooltip(
      gfx::Point(all_area.x(), all_area.y() - 1)));
  EXPECT_TRUE(launcher_view_->ShouldHideTooltip(
      gfx::Point(all_area.x(), all_area.bottom())));
}

TEST_F(LauncherViewTest, ShouldHideTooltipWithAppListWindowTest) {
  Shell::GetInstance()->ToggleAppList();
  ASSERT_TRUE(Shell::GetInstance()->GetAppListWindow());

  // The tooltip shouldn't hide if the mouse is on normal buttons.
  for (int i = 1; i < test_api_->GetButtonCount(); i++) {
    internal::LauncherButton* button = test_api_->GetButton(i);
    if (!button)
      continue;

    EXPECT_FALSE(launcher_view_->ShouldHideTooltip(
        button->GetMirroredBounds().CenterPoint()))
        << "LauncherView tries to hide on button " << i;
  }

  // The tooltip should hide on the app-list button.
  views::View* app_list_button = launcher_view_->GetAppListButtonView();
  EXPECT_TRUE(launcher_view_->ShouldHideTooltip(
      app_list_button->GetMirroredBounds().CenterPoint()));
}

// Resizing launcher view while an add animation without fade-in is running,
// which happens when overflow happens. App list button should end up in its
// new ideal bounds.
TEST_F(LauncherViewTest, ResizeDuringOverflowAddAnimation) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetLastVisibleIndex() + 1,
            test_api_->GetButtonCount());

  // Add buttons until overflow. Let the non-overflow add animations finish but
  // leave the last running.
  AddTabbedBrowserNoWait();
  while (!test_api_->IsOverflowButtonVisible()) {
    test_api_->RunMessageLoopUntilAnimationsDone();
    AddTabbedBrowserNoWait();
  }

  // Resize launcher view with that animation running and stay overflown.
  gfx::Rect bounds = launcher_view_->bounds();
  bounds.set_width(bounds.width() - kLauncherPreferredSize);
  launcher_view_->SetBoundsRect(bounds);
  ASSERT_TRUE(test_api_->IsOverflowButtonVisible());

  // Finish the animation.
  test_api_->RunMessageLoopUntilAnimationsDone();

  // App list button should ends up in its new ideal bounds.
  const int app_list_button_index = test_api_->GetButtonCount() - 1;
  const gfx::Rect& app_list_ideal_bounds =
      test_api_->GetIdealBoundsByIndex(app_list_button_index);
  const gfx::Rect& app_list_bounds =
      test_api_->GetBoundsByIndex(app_list_button_index);
  EXPECT_EQ(app_list_bounds, app_list_ideal_bounds);
}

}  // namespace test
}  // namespace ash
