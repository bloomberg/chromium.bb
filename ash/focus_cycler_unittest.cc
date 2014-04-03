// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/focus_cycler.h"

#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_factory.h"
#include "ash/shell_window_ids.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/tray/system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_util.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/views/accessible_pane_view.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace test {

using aura::Window;

namespace {

StatusAreaWidgetDelegate* GetStatusAreaWidgetDelegate(views::Widget* widget) {
  return static_cast<StatusAreaWidgetDelegate*>(widget->GetContentsView());
}

class PanedWidgetDelegate : public views::WidgetDelegate {
 public:
  PanedWidgetDelegate(views::Widget* widget) : widget_(widget) {}

  void SetAccessiblePanes(const std::vector<views::View*>& panes) {
    accessible_panes_ = panes;
  }

  // views::WidgetDelegate.
  virtual void GetAccessiblePanes(std::vector<views::View*>* panes) OVERRIDE {
    std::copy(accessible_panes_.begin(),
              accessible_panes_.end(),
              std::back_inserter(*panes));
  }
  virtual views::Widget* GetWidget() OVERRIDE {
    return widget_;
  };
  virtual const views::Widget* GetWidget() const OVERRIDE {
    return widget_;
  }

 private:
  views::Widget* widget_;
  std::vector<views::View*> accessible_panes_;
};

}  // namespace

class FocusCyclerTest : public AshTestBase {
 public:
  FocusCyclerTest() {}

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();

    focus_cycler_.reset(new FocusCycler());

    ASSERT_TRUE(Shelf::ForPrimaryDisplay());
  }

  virtual void TearDown() OVERRIDE {
    if (tray_) {
      GetStatusAreaWidgetDelegate(tray_->GetWidget())->
          SetFocusCyclerForTesting(NULL);
      tray_.reset();
    }

    shelf_widget()->SetFocusCycler(NULL);

    focus_cycler_.reset();

    AshTestBase::TearDown();
  }

 protected:
  // Creates the system tray, returning true on success.
  bool CreateTray() {
    if (tray_)
      return false;
    aura::Window* parent =
        Shell::GetPrimaryRootWindowController()->GetContainer(
            ash::kShellWindowId_StatusContainer);

    StatusAreaWidget* widget = new StatusAreaWidget(parent);
    widget->CreateTrayViews();
    widget->Show();
    tray_.reset(widget->system_tray());
    if (!tray_->GetWidget())
      return false;
    focus_cycler_->AddWidget(tray()->GetWidget());
    GetStatusAreaWidgetDelegate(tray_->GetWidget())->SetFocusCyclerForTesting(
        focus_cycler());
    return true;
  }

  FocusCycler* focus_cycler() { return focus_cycler_.get(); }

  SystemTray* tray() { return tray_.get(); }

  ShelfWidget* shelf_widget() {
    return Shelf::ForPrimaryDisplay()->shelf_widget();
  }

  void InstallFocusCycleOnShelf() {
    // Add the shelf.
    shelf_widget()->SetFocusCycler(focus_cycler());
  }

 private:
  scoped_ptr<FocusCycler> focus_cycler_;
  scoped_ptr<SystemTray> tray_;

  DISALLOW_COPY_AND_ASSIGN(FocusCyclerTest);
};

TEST_F(FocusCyclerTest, CycleFocusBrowserOnly) {
  // Create a single test window.
  scoped_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycle the window
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

TEST_F(FocusCyclerTest, CycleFocusForward) {
  ASSERT_TRUE(CreateTray());

  InstallFocusCycleOnShelf();

  // Create a single test window.
  scoped_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycle focus to the status area.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(tray()->GetWidget()->IsActive());

  // Cycle focus to the shelf.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(shelf_widget()->IsActive());

  // Cycle focus to the browser.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

TEST_F(FocusCyclerTest, CycleFocusBackward) {
  ASSERT_TRUE(CreateTray());

  InstallFocusCycleOnShelf();

  // Create a single test window.
  scoped_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycle focus to the shelf.
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(shelf_widget()->IsActive());

  // Cycle focus to the status area.
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(tray()->GetWidget()->IsActive());

  // Cycle focus to the browser.
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

TEST_F(FocusCyclerTest, CycleFocusForwardBackward) {
  ASSERT_TRUE(CreateTray());

  InstallFocusCycleOnShelf();

  // Create a single test window.
  scoped_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycle focus to the shelf.
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(shelf_widget()->IsActive());

  // Cycle focus to the status area.
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(tray()->GetWidget()->IsActive());

  // Cycle focus to the browser.
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycle focus to the status area.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(tray()->GetWidget()->IsActive());

  // Cycle focus to the shelf.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(shelf_widget()->IsActive());

  // Cycle focus to the browser.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

TEST_F(FocusCyclerTest, CycleFocusNoBrowser) {
  ASSERT_TRUE(CreateTray());

  InstallFocusCycleOnShelf();

  // Add the shelf and focus it.
  focus_cycler()->FocusWidget(shelf_widget());

  // Cycle focus to the status area.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(tray()->GetWidget()->IsActive());

  // Cycle focus to the shelf.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(shelf_widget()->IsActive());

  // Cycle focus to the status area.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(tray()->GetWidget()->IsActive());

  // Cycle focus to the shelf.
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(shelf_widget()->IsActive());

  // Cycle focus to the status area.
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(tray()->GetWidget()->IsActive());
}

// Tests that focus cycles from the active browser to the status area and back.
TEST_F(FocusCyclerTest, Shelf_CycleFocusForward) {
  ASSERT_TRUE(CreateTray());
  InstallFocusCycleOnShelf();
  shelf_widget()->Hide();

  // Create two test windows.
  scoped_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  scoped_ptr<Window> window1(CreateTestWindowInShellWithId(1));
  wm::ActivateWindow(window1.get());
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycle focus to the status area.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(tray()->GetWidget()->IsActive());

  // Cycle focus to the browser.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycle focus to the status area.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(tray()->GetWidget()->IsActive());
}

TEST_F(FocusCyclerTest, Shelf_CycleFocusBackwardInvisible) {
  ASSERT_TRUE(CreateTray());
  InstallFocusCycleOnShelf();
  shelf_widget()->Hide();

  // Create a single test window.
  scoped_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycle focus to the status area.
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(tray()->GetWidget()->IsActive());

  // Cycle focus to the browser.
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

TEST_F(FocusCyclerTest, CycleFocusThroughWindowWithPanes) {
  ASSERT_TRUE(CreateTray());

  InstallFocusCycleOnShelf();

  scoped_ptr<PanedWidgetDelegate> test_widget_delegate;
  scoped_ptr<views::Widget> browser_widget(new views::Widget);
  test_widget_delegate.reset(new PanedWidgetDelegate(browser_widget.get()));
  views::Widget::InitParams widget_params(
      views::Widget::InitParams::TYPE_WINDOW);
  widget_params.context = CurrentContext();
  widget_params.delegate = test_widget_delegate.get();
  widget_params.ownership =
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  browser_widget->Init(widget_params);
  browser_widget->Show();

  aura::Window* browser_window = browser_widget->GetNativeView();

  views::View* root_view = browser_widget->GetRootView();

  views::AccessiblePaneView* pane1 = new views::AccessiblePaneView();
  root_view->AddChildView(pane1);

  views::View* view1 = new views::View;
  view1->SetFocusable(true);
  pane1->AddChildView(view1);

  views::View* view2 = new views::View;
  view2->SetFocusable(true);
  pane1->AddChildView(view2);

  views::AccessiblePaneView* pane2 = new views::AccessiblePaneView();
  root_view->AddChildView(pane2);

  views::View* view3 = new views::View;
  view3->SetFocusable(true);
  pane2->AddChildView(view3);

  views::View* view4 = new views::View;
  view4->SetFocusable(true);
  pane2->AddChildView(view4);

  std::vector<views::View*> panes;
  panes.push_back(pane1);
  panes.push_back(pane2);

  test_widget_delegate->SetAccessiblePanes(panes);

  views::FocusManager* focus_manager = browser_widget->GetFocusManager();

  // Cycle focus to the status area.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(tray()->GetWidget()->IsActive());

  // Cycle focus to the shelf.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(shelf_widget()->IsActive());

  // Cycle focus to the first pane in the browser.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(browser_window));
  EXPECT_EQ(focus_manager->GetFocusedView(), view1);

  // Cycle focus to the second pane in the browser.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(browser_window));
  EXPECT_EQ(focus_manager->GetFocusedView(), view3);

  // Cycle focus back to the status area.
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(tray()->GetWidget()->IsActive());

  // Reverse direction - back to the second pane in the browser.
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(wm::IsActiveWindow(browser_window));
  EXPECT_EQ(focus_manager->GetFocusedView(), view3);

  // Back to the first pane in the browser.
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(wm::IsActiveWindow(browser_window));
  EXPECT_EQ(focus_manager->GetFocusedView(), view1);

  // Back to the shelf.
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(shelf_widget()->IsActive());

  // Back to the status area.
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(tray()->GetWidget()->IsActive());

  // Pressing "Escape" while on the status area should
  // deactivate it, and activate the browser window.
  aura::Window* root = Shell::GetPrimaryRootWindow();
  aura::test::EventGenerator event_generator(root, root);
  event_generator.PressKey(ui::VKEY_ESCAPE, 0);
  EXPECT_TRUE(wm::IsActiveWindow(browser_window));
  EXPECT_EQ(focus_manager->GetFocusedView(), view1);

  // Similarly, pressing "Escape" while on the shelf.
  // should do the same thing.
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(shelf_widget()->IsActive());
  event_generator.PressKey(ui::VKEY_ESCAPE, 0);
  EXPECT_TRUE(wm::IsActiveWindow(browser_window));
  EXPECT_EQ(focus_manager->GetFocusedView(), view1);
}

}  // namespace test
}  // namespace ash
