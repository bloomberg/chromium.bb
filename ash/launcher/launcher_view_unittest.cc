// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/launcher/launcher_view.h"

#include <algorithm>
#include <vector>

#include "ash/ash_switches.h"
#include "ash/launcher/launcher.h"
#include "ash/launcher/launcher_button.h"
#include "ash/launcher/launcher_icon_observer.h"
#include "ash/launcher/launcher_model.h"
#include "ash/launcher/launcher_tooltip_manager.h"
#include "ash/launcher/launcher_types.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/launcher_test_api.h"
#include "ash/test/launcher_view_test_api.h"
#include "ash/test/shell_test_api.h"
#include "ash/test/test_launcher_delegate.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "grit/ash_resources.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/aura_test_base.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/views/view_model.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

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
    Launcher* launcher = Launcher::ForPrimaryDisplay();
    observer_.reset(new TestLauncherIconObserver(launcher));

    launcher_view_test_.reset(new LauncherViewTestAPI(
        LauncherTestAPI(launcher).launcher_view()));
    launcher_view_test_->SetAnimationDuration(1);
  }

  virtual void TearDown() OVERRIDE {
    observer_.reset();
    AshTestBase::TearDown();
  }

  TestLauncherIconObserver* observer() { return observer_.get(); }

  LauncherViewTestAPI* launcher_view_test() {
    return launcher_view_test_.get();
  }

  Launcher* LauncherForSecondaryDisplay() {
    return Launcher::ForWindow(Shell::GetAllRootWindows()[1]);
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
  params.context = CurrentContext();

  scoped_ptr<views::Widget> widget(new views::Widget());
  widget->Init(params);
  launcher_delegate->AddLauncherItem(widget->GetNativeWindow());
  launcher_view_test()->RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(observer()->change_notified());
  observer()->Reset();

  widget->Show();
  widget->GetNativeWindow()->parent()->RemoveChild(widget->GetNativeWindow());
  launcher_view_test()->RunMessageLoopUntilAnimationsDone();
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
// launcher on external display as well as one on primary.
TEST_F(LauncherViewIconObserverTest, MAYBE_AddRemoveWithMultipleDisplays) {
  UpdateDisplay("400x400,400x400");
  TestLauncherIconObserver second_observer(LauncherForSecondaryDisplay());

  ash::test::TestLauncherDelegate* launcher_delegate =
      ash::test::TestLauncherDelegate::instance();
  ASSERT_TRUE(launcher_delegate);

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = CurrentContext();

  scoped_ptr<views::Widget> widget(new views::Widget());
  widget->Init(params);
  launcher_delegate->AddLauncherItem(widget->GetNativeWindow());
  launcher_view_test()->RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(observer()->change_notified());
  EXPECT_TRUE(second_observer.change_notified());
  observer()->Reset();
  second_observer.Reset();

  widget->GetNativeWindow()->parent()->RemoveChild(widget->GetNativeWindow());
  launcher_view_test()->RunMessageLoopUntilAnimationsDone();
  EXPECT_TRUE(observer()->change_notified());
  EXPECT_TRUE(second_observer.change_notified());

  observer()->Reset();
  second_observer.Reset();
}

TEST_F(LauncherViewIconObserverTest, BoundsChanged) {
  ash::ShelfWidget* shelf = Shell::GetPrimaryRootWindowController()->shelf();
  Launcher* launcher = Launcher::ForPrimaryDisplay();
  gfx::Size shelf_size =
      shelf->GetWindowBoundsInScreen().size();
  shelf_size.set_width(shelf_size.width() / 2);
  ASSERT_GT(shelf_size.width(), 0);
  launcher->SetLauncherViewBounds(gfx::Rect(shelf_size));
  // No animation happens for LauncherView bounds change.
  EXPECT_TRUE(observer()->change_notified());
  observer()->Reset();
}

////////////////////////////////////////////////////////////////////////////////
// LauncherView tests.

class LauncherViewTest : public AshTestBase {
 public:
  LauncherViewTest() : model_(NULL), launcher_view_(NULL), browser_index_(1) {}
  virtual ~LauncherViewTest() {}

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();
    test::ShellTestApi test_api(Shell::GetInstance());
    model_ = test_api.launcher_model();
    Launcher* launcher = Launcher::ForPrimaryDisplay();
    launcher_view_ = test::LauncherTestAPI(launcher).launcher_view();

    // The bounds should be big enough for 4 buttons + overflow chevron.
    launcher_view_->SetBounds(0, 0, 500, 50);

    test_api_.reset(new LauncherViewTestAPI(launcher_view_));
    test_api_->SetAnimationDuration(1);  // Speeds up animation for test.

    // Add browser shortcut launcher item at index 0 for test.
    AddBrowserShortcut();
  }

  virtual void TearDown() OVERRIDE {
    test_api_.reset();
    AshTestBase::TearDown();
  }

 protected:
  LauncherID AddBrowserShortcut() {
    LauncherItem browser_shortcut;
    browser_shortcut.type = TYPE_BROWSER_SHORTCUT;

    LauncherID id = model_->next_id();
    model_->AddAt(browser_index_, browser_shortcut);
    test_api_->RunMessageLoopUntilAnimationsDone();
    return id;
  }

  LauncherID AddAppShortcut() {
    LauncherItem item;
    item.type = TYPE_APP_SHORTCUT;
    item.status = STATUS_CLOSED;

    LauncherID id = model_->next_id();
    model_->Add(item);
    test_api_->RunMessageLoopUntilAnimationsDone();
    return id;
  }

  LauncherID AddPanel() {
    LauncherID id = AddPanelNoWait();
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

  LauncherID AddPanelNoWait() {
    LauncherItem item;
    item.type = TYPE_APP_PANEL;
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
      ash::LauncherID id = item.id;
      EXPECT_EQ(id_map[map_index].first, id);
      EXPECT_EQ(id_map[map_index].second, GetButtonByID(id));
      ++map_index;
    }
    ASSERT_EQ(map_index, id_map.size());
  }

  void VerifyLauncherItemBoundsAreValid() {
    for (int i=0;i <= test_api_->GetLastVisibleIndex(); ++i) {
      if (test_api_->GetButton(i)) {
        gfx::Rect launcher_view_bounds = launcher_view_->GetLocalBounds();
        gfx::Rect item_bounds = test_api_->GetBoundsByIndex(i);
        EXPECT_TRUE(item_bounds.x() >= 0);
        EXPECT_TRUE(item_bounds.y() >= 0);
        EXPECT_TRUE(item_bounds.right() <= launcher_view_bounds.width());
        EXPECT_TRUE(item_bounds.bottom() <= launcher_view_bounds.height());
      }
    }
  }

  views::View* SimulateButtonPressed(
      internal::LauncherButtonHost::Pointer pointer,
      int button_index) {
    internal::LauncherButtonHost* button_host = launcher_view_;
    views::View* button = test_api_->GetButton(button_index);
    ui::MouseEvent click_event(ui::ET_MOUSE_PRESSED,
                               button->bounds().origin(),
                               button->GetBoundsInScreen().origin(), 0);
    button_host->PointerPressedOnButton(button, pointer, click_event);
    return button;
  }

  views::View* SimulateClick(internal::LauncherButtonHost::Pointer pointer,
                             int button_index) {
    internal::LauncherButtonHost* button_host = launcher_view_;
    views::View* button = SimulateButtonPressed(pointer, button_index);
    button_host->PointerReleasedOnButton(button,
                                         internal::LauncherButtonHost::MOUSE,
                                         false);
    return button;
  }

  views::View* SimulateDrag(internal::LauncherButtonHost::Pointer pointer,
                            int button_index,
                            int destination_index) {
    internal::LauncherButtonHost* button_host = launcher_view_;
    views::View* button = SimulateButtonPressed(pointer, button_index);

    // Drag.
    views::View* destination = test_api_->GetButton(destination_index);
    ui::MouseEvent drag_event(ui::ET_MOUSE_DRAGGED,
                              destination->bounds().origin(),
                              destination->GetBoundsInScreen().origin(), 0);
    button_host->PointerDraggedOnButton(button, pointer, drag_event);
    return button;
  }

  void SetupForDragTest(
      std::vector<std::pair<LauncherID, views::View*> >* id_map) {
    // Initialize |id_map| with the automatically-created launcher buttons.
    for (size_t i = 0; i < model_->items().size(); ++i) {
      internal::LauncherButton* button = test_api_->GetButton(i);
      id_map->push_back(std::make_pair(model_->items()[i].id, button));
    }
    ASSERT_NO_FATAL_FAILURE(CheckModelIDs(*id_map));

    // Add 5 app launcher buttons for testing.
    for (int i = 0; i < 5; ++i) {
      LauncherID id = AddAppShortcut();
      // App Icon is located at index 0, and browser shortcut is located at
      // index 1. So we should start to add app shortcut at index 2.
      id_map->insert(id_map->begin() + (i + browser_index_ + 1),
                     std::make_pair(id, GetButtonByID(id)));
    }
    ASSERT_NO_FATAL_FAILURE(CheckModelIDs(*id_map));
  }

  views::View* GetTooltipAnchorView() {
    return launcher_view_->tooltip_manager()->anchor_;
  }

  void ShowTooltip() {
    launcher_view_->tooltip_manager()->ShowInternal();
  }

  LauncherModel* model_;
  internal::LauncherView* launcher_view_;
  int browser_index_;

  scoped_ptr<LauncherViewTestAPI> test_api_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LauncherViewTest);
};

class LauncherViewLegacyShelfLayoutTest : public LauncherViewTest {
 public:
  LauncherViewLegacyShelfLayoutTest() : LauncherViewTest() {
    browser_index_ = 0;
  }

  virtual ~LauncherViewLegacyShelfLayoutTest() {}

  virtual void SetUp() OVERRIDE {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        ash::switches::kAshDisableAlternateShelfLayout);
    LauncherViewTest::SetUp();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(LauncherViewLegacyShelfLayoutTest);
};

class LauncherViewTextDirectionTest
    : public LauncherViewTest,
      public testing::WithParamInterface<bool> {
 public:
  LauncherViewTextDirectionTest() : is_rtl_(GetParam()) {}
  virtual ~LauncherViewTextDirectionTest() {}

  virtual void SetUp() OVERRIDE {
    LauncherViewTest::SetUp();
    original_locale_ = l10n_util::GetApplicationLocale(std::string());
    if (is_rtl_)
      base::i18n::SetICUDefaultLocale("he");
    ASSERT_EQ(is_rtl_, base::i18n::IsRTL());
  }

  virtual void TearDown() OVERRIDE {
    if (is_rtl_)
      base::i18n::SetICUDefaultLocale(original_locale_);
    LauncherViewTest::TearDown();
  }

 private:
  bool is_rtl_;
  std::string original_locale_;

  DISALLOW_COPY_AND_ASSIGN(LauncherViewTextDirectionTest);
};

// Checks that the ideal item icon bounds match the view's bounds in the screen
// in both LTR and RTL.
TEST_P(LauncherViewTextDirectionTest, IdealBoundsOfItemIcon) {
  LauncherID id = AddPlatformApp();
  internal::LauncherButton* button = GetButtonByID(id);
  gfx::Rect item_bounds = button->GetBoundsInScreen();
  gfx::Point icon_offset = button->GetIconBounds().origin();
  item_bounds.Offset(icon_offset.OffsetFromOrigin());
  gfx::Rect ideal_bounds = launcher_view_->GetIdealBoundsOfItemIcon(id);
  gfx::Point screen_origin;
  views::View::ConvertPointToScreen(launcher_view_, &screen_origin);
  ideal_bounds.Offset(screen_origin.x(), screen_origin.y());
  EXPECT_EQ(item_bounds.x(), ideal_bounds.x());
  EXPECT_EQ(item_bounds.y(), ideal_bounds.y());
}

// Checks that launcher view contents are considered in the correct drag group.
TEST_F(LauncherViewTest, EnforceDragType) {
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
TEST_F(LauncherViewTest, AddBrowserUntilOverflow) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetLastVisibleIndex() + 1,
            test_api_->GetButtonCount());

  // Add platform app button until overflow.
  int items_added = 0;
  LauncherID last_added = AddPlatformApp();
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
TEST_F(LauncherViewTest, AddAppShortcutWithBrowserButtonUntilOverflow) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetLastVisibleIndex() + 1,
            test_api_->GetButtonCount());

  LauncherID browser_button_id = AddPlatformApp();

  // Add app shortcut until overflow.
  int items_added = 0;
  LauncherID last_added = AddAppShortcut();
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

TEST_F(LauncherViewLegacyShelfLayoutTest,
       AddAppShortcutWithBrowserButtonUntilOverflow) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetLastVisibleIndex() + 1,
            test_api_->GetButtonCount());

  LauncherID browser_button_id = AddPlatformApp();

  // Add app shortcut until overflow.
  int items_added = 0;
  LauncherID last_added = AddAppShortcut();
  while (!test_api_->IsOverflowButtonVisible()) {
    // Added button is visible after animation while in this loop.
    EXPECT_TRUE(GetButtonByID(last_added)->visible());

    last_added = AddAppShortcut();
    ++items_added;
    ASSERT_LT(items_added, 10000);
  }

  // The last added app short button should be visible.
  EXPECT_TRUE(GetButtonByID(last_added)->visible());
  // And the platform app button is invisible.
  EXPECT_FALSE(GetButtonByID(browser_button_id)->visible());
}

TEST_F(LauncherViewTest, AddPanelHidesPlatformAppButton) {
  ASSERT_EQ(test_api_->GetLastVisibleIndex() + 1,
            test_api_->GetButtonCount());

  // Add platform app button until overflow, remember last visible platform app
  // button.
  int items_added = 0;
  LauncherID first_added = AddPlatformApp();
  EXPECT_TRUE(GetButtonByID(first_added)->visible());
  while (true) {
    LauncherID added = AddPlatformApp();
    if (test_api_->IsOverflowButtonVisible()) {
      EXPECT_FALSE(GetButtonByID(added)->visible());
      RemoveByID(added);
      break;
    }
    ++items_added;
    ASSERT_LT(items_added, 10000);
  }

  LauncherID panel = AddPanel();
  EXPECT_TRUE(test_api_->IsOverflowButtonVisible());

  RemoveByID(panel);
  EXPECT_FALSE(test_api_->IsOverflowButtonVisible());
}

TEST_F(LauncherViewLegacyShelfLayoutTest, AddPanelHidesPlatformAppButton) {
  ASSERT_EQ(test_api_->GetLastVisibleIndex() + 1,
            test_api_->GetButtonCount());

  // Add platform app button until overflow, remember last visible platform app
  // button.
  int items_added = 0;
  LauncherID first_added = AddPlatformApp();
  EXPECT_TRUE(GetButtonByID(first_added)->visible());
  LauncherID last_visible = first_added;
  while (true) {
    LauncherID added = AddPlatformApp();
    if (test_api_->IsOverflowButtonVisible()) {
      EXPECT_FALSE(GetButtonByID(added)->visible());
      break;
    }
    last_visible = added;
    ++items_added;
    ASSERT_LT(items_added, 10000);
  }

  LauncherID panel = AddPanel();
  EXPECT_TRUE(GetButtonByID(panel)->visible());
  EXPECT_FALSE(GetButtonByID(last_visible)->visible());

  RemoveByID(panel);
  EXPECT_TRUE(GetButtonByID(last_visible)->visible());
}

// When there are more panels then platform app buttons we should hide panels
// rather than platform apps.
TEST_F(LauncherViewTest, PlatformAppHidesExcessPanels) {
  ASSERT_EQ(test_api_->GetLastVisibleIndex() + 1,
            test_api_->GetButtonCount());

  // Add platform app button.
  LauncherID platform_app = AddPlatformApp();
  LauncherID first_panel = AddPanel();

  EXPECT_TRUE(GetButtonByID(platform_app)->visible());
  EXPECT_TRUE(GetButtonByID(first_panel)->visible());

  // Add panels until there is an overflow.
  LauncherID last_panel = first_panel;
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
TEST_F(LauncherViewTest, RemoveButtonRevealsOverflowed) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetLastVisibleIndex() + 1,
            test_api_->GetButtonCount());

  // Add platform app buttons until overflow.
  int items_added = 0;
  LauncherID first_added = AddPlatformApp();
  LauncherID last_added = first_added;
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
TEST_F(LauncherViewTest, RemoveLastOverflowed) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetLastVisibleIndex() + 1,
            test_api_->GetButtonCount());

  // Add platform app button until overflow.
  int items_added = 0;
  LauncherID last_added = AddPlatformApp();
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
TEST_F(LauncherViewTest, AddButtonQuickly) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetLastVisibleIndex() + 1,
            test_api_->GetButtonCount());

  // Add a few platform buttons quickly without wait for animation.
  int added_count = 0;
  while (!test_api_->IsOverflowButtonVisible()) {
    AddPlatformAppNoWait();
    ++added_count;
    ASSERT_LT(added_count, 10000);
  }

  // LauncherView should be big enough to hold at least 3 new buttons.
  ASSERT_GE(added_count, 3);

  // Wait for the last animation to finish.
  test_api_->RunMessageLoopUntilAnimationsDone();

  // Verifies non-overflow buttons are visible.
  for (int i = 0; i <= test_api_->GetLastVisibleIndex(); ++i) {
    internal::LauncherButton* button = test_api_->GetButton(i);
    if (button) {
      EXPECT_TRUE(button->visible()) << "button index=" << i;
      EXPECT_EQ(1.0f, button->layer()->opacity()) << "button index=" << i;
    }
  }
}

// Check that model changes are handled correctly while a launcher icon is being
// dragged.
TEST_F(LauncherViewTest, ModelChangesWhileDragging) {
  internal::LauncherButtonHost* button_host = launcher_view_;

  std::vector<std::pair<LauncherID, views::View*> > id_map;
  SetupForDragTest(&id_map);

  // Dragging browser shortcut at index 1.
  EXPECT_TRUE(model_->items()[1].type == TYPE_BROWSER_SHORTCUT);
  views::View* dragged_button = SimulateDrag(
      internal::LauncherButtonHost::MOUSE, 1, 3);
  std::rotate(id_map.begin() + 1,
              id_map.begin() + 2,
              id_map.begin() + 4);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  button_host->PointerReleasedOnButton(dragged_button,
                                       internal::LauncherButtonHost::MOUSE,
                                       false);
  EXPECT_TRUE(model_->items()[3].type == TYPE_BROWSER_SHORTCUT);

  // Dragging changes model order.
  dragged_button = SimulateDrag(
      internal::LauncherButtonHost::MOUSE, 1, 3);
  std::rotate(id_map.begin() + 1,
              id_map.begin() + 2,
              id_map.begin() + 4);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // Cancelling the drag operation restores previous order.
  button_host->PointerReleasedOnButton(dragged_button,
                                       internal::LauncherButtonHost::MOUSE,
                                       true);
  std::rotate(id_map.begin() + 1,
              id_map.begin() + 3,
              id_map.begin() + 4);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // Deleting an item keeps the remaining intact.
  dragged_button = SimulateDrag(internal::LauncherButtonHost::MOUSE, 1, 3);
  model_->RemoveItemAt(1);
  id_map.erase(id_map.begin() + 1);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  button_host->PointerReleasedOnButton(dragged_button,
                                       internal::LauncherButtonHost::MOUSE,
                                       false);

  // Adding a launcher item cancels the drag and respects the order.
  dragged_button = SimulateDrag(internal::LauncherButtonHost::MOUSE, 1, 3);
  LauncherID new_id = AddAppShortcut();
  id_map.insert(id_map.begin() + 6,
                std::make_pair(new_id, GetButtonByID(new_id)));
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  button_host->PointerReleasedOnButton(dragged_button,
                                       internal::LauncherButtonHost::MOUSE,
                                       false);

  // Adding a launcher item at the end (i.e. a panel)  canels drag and respects
  // the order.
  dragged_button = SimulateDrag(internal::LauncherButtonHost::MOUSE, 1, 3);
  new_id = AddPanel();
  id_map.insert(id_map.begin() + 7,
                std::make_pair(new_id, GetButtonByID(new_id)));
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  button_host->PointerReleasedOnButton(dragged_button,
                                       internal::LauncherButtonHost::MOUSE,
                                       false);
}

TEST_F(LauncherViewLegacyShelfLayoutTest, ModelChangesWhileDragging) {
  internal::LauncherButtonHost* button_host = launcher_view_;

  std::vector<std::pair<LauncherID, views::View*> > id_map;
  SetupForDragTest(&id_map);

  // Dragging browser shortcut at index 0.
  EXPECT_TRUE(model_->items()[0].type == TYPE_BROWSER_SHORTCUT);
  views::View* dragged_button = SimulateDrag(
      internal::LauncherButtonHost::MOUSE, 0, 2);
  std::rotate(id_map.begin(),
              id_map.begin() + 1,
              id_map.begin() + 3);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  button_host->PointerReleasedOnButton(dragged_button,
                                       internal::LauncherButtonHost::MOUSE,
                                       false);
  EXPECT_TRUE(model_->items()[2].type == TYPE_BROWSER_SHORTCUT);

  // Dragging changes model order.
  dragged_button = SimulateDrag(
      internal::LauncherButtonHost::MOUSE, 0, 2);
  std::rotate(id_map.begin(),
              id_map.begin() + 1,
              id_map.begin() + 3);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // Cancelling the drag operation restores previous order.
  button_host->PointerReleasedOnButton(dragged_button,
                                       internal::LauncherButtonHost::MOUSE,
                                       true);
  std::rotate(id_map.begin(),
              id_map.begin() + 2,
              id_map.begin() + 3);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // Deleting an item keeps the remaining intact.
  dragged_button = SimulateDrag(internal::LauncherButtonHost::MOUSE, 0, 2);
  model_->RemoveItemAt(1);
  id_map.erase(id_map.begin() + 1);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  button_host->PointerReleasedOnButton(dragged_button,
                                       internal::LauncherButtonHost::MOUSE,
                                       false);

  // Adding a launcher item cancels the drag and respects the order.
  dragged_button = SimulateDrag(internal::LauncherButtonHost::MOUSE, 0, 2);
  LauncherID new_id = AddAppShortcut();
  id_map.insert(id_map.begin() + 5,
                std::make_pair(new_id, GetButtonByID(new_id)));
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  button_host->PointerReleasedOnButton(dragged_button,
                                       internal::LauncherButtonHost::MOUSE,
                                       false);

  // Adding a launcher item at the end (i.e. a panel)  canels drag and respects
  // the order.
  dragged_button = SimulateDrag(internal::LauncherButtonHost::MOUSE, 0, 2);
  new_id = AddPanel();
  id_map.insert(id_map.begin() + 7,
                std::make_pair(new_id, GetButtonByID(new_id)));
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  button_host->PointerReleasedOnButton(dragged_button,
                                       internal::LauncherButtonHost::MOUSE,
                                       false);
}

// Check that 2nd drag from the other pointer would be ignored.
TEST_F(LauncherViewTest, SimultaneousDrag) {
  internal::LauncherButtonHost* button_host = launcher_view_;

  std::vector<std::pair<LauncherID, views::View*> > id_map;
  SetupForDragTest(&id_map);

  // Start a mouse drag.
  views::View* dragged_button_mouse = SimulateDrag(
      internal::LauncherButtonHost::MOUSE, 1, 3);
  std::rotate(id_map.begin() + 1,
              id_map.begin() + 2,
              id_map.begin() + 4);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  // Attempt a touch drag before the mouse drag finishes.
  views::View* dragged_button_touch = SimulateDrag(
      internal::LauncherButtonHost::TOUCH, 4, 2);

  // Nothing changes since 2nd drag is ignored.
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // Finish the mouse drag.
  button_host->PointerReleasedOnButton(dragged_button_mouse,
                                       internal::LauncherButtonHost::MOUSE,
                                       false);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // Now start a touch drag.
  dragged_button_touch = SimulateDrag(
      internal::LauncherButtonHost::TOUCH, 4, 2);
  std::rotate(id_map.begin() + 3,
              id_map.begin() + 4,
              id_map.begin() + 5);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  // And attempt a mouse drag before the touch drag finishes.
  dragged_button_mouse = SimulateDrag(
      internal::LauncherButtonHost::MOUSE, 1, 2);

  // Nothing changes since 2nd drag is ignored.
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));

  button_host->PointerReleasedOnButton(dragged_button_touch,
                                       internal::LauncherButtonHost::TOUCH,
                                       false);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
}

// Check that clicking first on one item and then dragging another works as
// expected.
TEST_F(LauncherViewTest, ClickOneDragAnother) {
  internal::LauncherButtonHost* button_host = launcher_view_;

  std::vector<std::pair<LauncherID, views::View*> > id_map;
  SetupForDragTest(&id_map);

  // A click on item 1 is simulated.
  SimulateClick(internal::LauncherButtonHost::MOUSE, 1);

  // Dragging browser index at 0 should change the model order correctly.
  EXPECT_TRUE(model_->items()[1].type == TYPE_BROWSER_SHORTCUT);
  views::View* dragged_button = SimulateDrag(
      internal::LauncherButtonHost::MOUSE, 1, 3);
  std::rotate(id_map.begin() + 1,
              id_map.begin() + 2,
              id_map.begin() + 4);
  ASSERT_NO_FATAL_FAILURE(CheckModelIDs(id_map));
  button_host->PointerReleasedOnButton(dragged_button,
                                       internal::LauncherButtonHost::MOUSE,
                                       false);
  EXPECT_TRUE(model_->items()[3].type == TYPE_BROWSER_SHORTCUT);
}

// Confirm that item status changes are reflected in the buttons.
TEST_F(LauncherViewTest, LauncherItemStatus) {
  ASSERT_EQ(test_api_->GetLastVisibleIndex() + 1,
            test_api_->GetButtonCount());

  // Add platform app button.
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

TEST_F(LauncherViewLegacyShelfLayoutTest,
       LauncherItemPositionReflectedOnStateChanged) {
  ASSERT_EQ(test_api_->GetLastVisibleIndex() + 1,
            test_api_->GetButtonCount());

  // Add 2 items to the launcher.
  LauncherID item1_id = AddPlatformApp();
  LauncherID item2_id = AddPlatformAppNoWait();
  internal::LauncherButton* item1_button = GetButtonByID(item1_id);
  internal::LauncherButton* item2_button = GetButtonByID(item2_id);

  internal::LauncherButton::State state_mask =
      static_cast<internal::LauncherButton::State>
          (internal::LauncherButton::STATE_NORMAL |
          internal::LauncherButton::STATE_HOVERED |
          internal::LauncherButton::STATE_RUNNING |
          internal::LauncherButton::STATE_ACTIVE |
          internal::LauncherButton::STATE_ATTENTION |
          internal::LauncherButton::STATE_FOCUSED);

  // Clear the button states.
  item1_button->ClearState(state_mask);
  item2_button->ClearState(state_mask);

  // Since default alignment in tests is bottom, state is reflected in y-axis.
  ASSERT_EQ(item1_button->GetIconBounds().y(),
            item2_button->GetIconBounds().y());
  item1_button->AddState(internal::LauncherButton::STATE_HOVERED);
  ASSERT_NE(item1_button->GetIconBounds().y(),
            item2_button->GetIconBounds().y());
  item1_button->ClearState(internal::LauncherButton::STATE_HOVERED);
}

// Confirm that item status changes are reflected in the buttons
// for platform apps.
TEST_F(LauncherViewTest, LauncherItemStatusPlatformApp) {
  ASSERT_EQ(test_api_->GetLastVisibleIndex() + 1,
            test_api_->GetButtonCount());

  // Add platform app button.
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

// Confirm that launcher item bounds are correctly updated on shelf changes.
TEST_F(LauncherViewTest, LauncherItemBoundsCheck) {
  internal::ShelfLayoutManager* shelf_layout_manager =
      Shell::GetPrimaryRootWindowController()->shelf()->shelf_layout_manager();
  VerifyLauncherItemBoundsAreValid();
  shelf_layout_manager->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  test_api_->RunMessageLoopUntilAnimationsDone();
  VerifyLauncherItemBoundsAreValid();
  shelf_layout_manager->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  test_api_->RunMessageLoopUntilAnimationsDone();
  VerifyLauncherItemBoundsAreValid();
}

TEST_F(LauncherViewTest, LauncherTooltipTest) {
  ASSERT_EQ(test_api_->GetLastVisibleIndex() + 1,
            test_api_->GetButtonCount());

  // Prepare some items to the launcher.
  LauncherID app_button_id = AddAppShortcut();
  LauncherID platform_button_id = AddPlatformApp();

  internal::LauncherButton* app_button = GetButtonByID(app_button_id);
  internal::LauncherButton* platform_button = GetButtonByID(platform_button_id);

  internal::LauncherButtonHost* button_host = launcher_view_;
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

// Verify a fix for crash caused by a tooltip update for a deleted launcher
// button, see crbug.com/288838.
TEST_F(LauncherViewTest, RemovingItemClosesTooltip) {
  internal::LauncherButtonHost* button_host = launcher_view_;
  internal::LauncherTooltipManager* tooltip_manager =
      launcher_view_->tooltip_manager();

  // Add an item to the launcher.
  LauncherID app_button_id = AddAppShortcut();
  internal::LauncherButton* app_button = GetButtonByID(app_button_id);

  // Spawn a tooltip on that item.
  button_host->MouseEnteredButton(app_button);
  ShowTooltip();
  EXPECT_TRUE(tooltip_manager->IsVisible());

  // Remove the app shortcut while the tooltip is open. The tooltip should be
  // closed.
  RemoveByID(app_button_id);
  EXPECT_FALSE(tooltip_manager->IsVisible());

  // Change the shelf layout. This should not crash.
  ash::Shell::GetInstance()->SetShelfAlignment(
      ash::SHELF_ALIGNMENT_LEFT,
      ash::Shell::GetPrimaryRootWindow());
}

// Changing the shelf alignment closes any open tooltip.
TEST_F(LauncherViewTest, ShelfAlignmentClosesTooltip) {
  internal::LauncherButtonHost* button_host = launcher_view_;
  internal::LauncherTooltipManager* tooltip_manager =
      launcher_view_->tooltip_manager();

  // Add an item to the launcher.
  LauncherID app_button_id = AddAppShortcut();
  internal::LauncherButton* app_button = GetButtonByID(app_button_id);

  // Spawn a tooltip on the item.
  button_host->MouseEnteredButton(app_button);
  ShowTooltip();
  EXPECT_TRUE(tooltip_manager->IsVisible());

  // Changing shelf alignment hides the tooltip.
  ash::Shell::GetInstance()->SetShelfAlignment(
      ash::SHELF_ALIGNMENT_LEFT,
      ash::Shell::GetPrimaryRootWindow());
  EXPECT_FALSE(tooltip_manager->IsVisible());
}

TEST_F(LauncherViewTest, ShouldHideTooltipTest) {
  LauncherID app_button_id = AddAppShortcut();
  LauncherID platform_button_id = AddPlatformApp();

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
  gfx::Rect platform_button_rect =
      GetButtonByID(platform_button_id)->GetMirroredBounds();
  ASSERT_FALSE(app_button_rect.Intersects(platform_button_rect));
  EXPECT_FALSE(launcher_view_->ShouldHideTooltip(
      gfx::UnionRects(app_button_rect, platform_button_rect).CenterPoint()));

  // The tooltip should hide if it's outside of all buttons.
  gfx::Rect all_area;
  for (int i = 0; i < test_api_->GetButtonCount(); i++) {
    internal::LauncherButton* button = test_api_->GetButton(i);
    if (!button)
      continue;

    all_area.Union(button->GetMirroredBounds());
  }
  all_area.Union(launcher_view_->GetAppListButtonView()->GetMirroredBounds());
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
  Shell::GetInstance()->ToggleAppList(NULL);
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

// Test that by moving the mouse cursor off the button onto the bubble it closes
// the bubble.
TEST_F(LauncherViewTest, ShouldHideTooltipWhenHoveringOnTooltip) {
  internal::LauncherTooltipManager* tooltip_manager =
      launcher_view_->tooltip_manager();
  tooltip_manager->CreateZeroDelayTimerForTest();
  aura::test::EventGenerator generator(Shell::GetPrimaryRootWindow());

  // Move the mouse off any item and check that no tooltip is shown.
  generator.MoveMouseTo(gfx::Point(0, 0));
  EXPECT_FALSE(tooltip_manager->IsVisible());

  // Move the mouse over the button and check that it is visible.
  views::View* app_list_button = launcher_view_->GetAppListButtonView();
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

// Resizing launcher view while an add animation without fade-in is running,
// which happens when overflow happens. App list button should end up in its
// new ideal bounds.
TEST_F(LauncherViewTest, ResizeDuringOverflowAddAnimation) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetLastVisibleIndex() + 1,
            test_api_->GetButtonCount());

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

// Check that the first item in the list follows Fitt's law by including the
// first pixel and being therefore bigger then the others.
TEST_F(LauncherViewLegacyShelfLayoutTest, CheckFittsLaw) {
  // All buttons should be visible.
  ASSERT_EQ(test_api_->GetLastVisibleIndex() + 1,
            test_api_->GetButtonCount());
  gfx::Rect ideal_bounds_0 = test_api_->GetIdealBoundsByIndex(0);
  gfx::Rect ideal_bounds_1 = test_api_->GetIdealBoundsByIndex(1);
  EXPECT_GT(ideal_bounds_0.width(), ideal_bounds_1.width());
}

INSTANTIATE_TEST_CASE_P(LtrRtl, LauncherViewTextDirectionTest, testing::Bool());

}  // namespace test
}  // namespace ash
