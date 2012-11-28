// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/focus_cycler.h"

#include "ash/launcher/launcher.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_delegate.h"
#include "ash/system/tray/system_tray.h"
#include "ash/wm/window_util.h"
#include "ash/test/ash_test_base.h"
#include "ash/shell_factory.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace test {

using aura::Window;
using internal::FocusCycler;

namespace {

internal::StatusAreaWidgetDelegate* GetStatusAreaWidgetDelegate(
    views::Widget* widget) {
  return static_cast<internal::StatusAreaWidgetDelegate*>(
      widget->GetContentsView());
}

}  // namespace

class FocusCyclerTest : public AshTestBase {
 public:
  FocusCyclerTest() {}

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();

    focus_cycler_.reset(new FocusCycler());

    ASSERT_TRUE(Launcher::ForPrimaryDisplay());
  }

  virtual void TearDown() OVERRIDE {
    if (tray_.get()) {
      GetStatusAreaWidgetDelegate(tray_->GetWidget())->
          SetFocusCyclerForTesting(NULL);
      tray_.reset();
    }

    Launcher::ForPrimaryDisplay()->SetFocusCycler(NULL);

    focus_cycler_.reset();

    AshTestBase::TearDown();
  }

 protected:
  // Creates the system tray, returning true on success.
  bool CreateTray() {
    if (tray_.get())
      return false;
    aura::Window* parent = Shell::GetPrimaryRootWindowController()->
        GetContainer(ash::internal::kShellWindowId_StatusContainer);

    internal::StatusAreaWidget* widget = new internal::StatusAreaWidget(parent);
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

  views::Widget* launcher_widget() {
    return Launcher::ForPrimaryDisplay()->widget();
  }

  void InstallFocusCycleOnLauncher() {
    // Add the launcher
    Launcher* launcher = Launcher::ForPrimaryDisplay();
    launcher->SetFocusCycler(focus_cycler());
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

  InstallFocusCycleOnLauncher();

  // Create a single test window.
  scoped_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycle focus to the status area
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(tray()->GetWidget()->IsActive());

  // Cycle focus to the launcher
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(launcher_widget()->IsActive());

  // Cycle focus to the browser
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

TEST_F(FocusCyclerTest, CycleFocusBackward) {
  ASSERT_TRUE(CreateTray());

  InstallFocusCycleOnLauncher();

  // Create a single test window.
  scoped_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycle focus to the launcher
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(launcher_widget()->IsActive());

  // Cycle focus to the status area
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(tray()->GetWidget()->IsActive());

  // Cycle focus to the browser
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

TEST_F(FocusCyclerTest, CycleFocusForwardBackward) {
  ASSERT_TRUE(CreateTray());

  InstallFocusCycleOnLauncher();

  // Create a single test window.
  scoped_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycle focus to the launcher
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(launcher_widget()->IsActive());

  // Cycle focus to the status area
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(tray()->GetWidget()->IsActive());

  // Cycle focus to the browser
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycle focus to the status area
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(tray()->GetWidget()->IsActive());

  // Cycle focus to the launcher
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(launcher_widget()->IsActive());

  // Cycle focus to the browser
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

TEST_F(FocusCyclerTest, CycleFocusNoBrowser) {
  ASSERT_TRUE(CreateTray());

  InstallFocusCycleOnLauncher();

  // Add the launcher and focus it
  focus_cycler()->FocusWidget(launcher_widget());

  // Cycle focus to the status area
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(tray()->GetWidget()->IsActive());

  // Cycle focus to the launcher
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(launcher_widget()->IsActive());

  // Cycle focus to the status area
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(tray()->GetWidget()->IsActive());

  // Cycle focus to the launcher
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(launcher_widget()->IsActive());

  // Cycle focus to the status area
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(tray()->GetWidget()->IsActive());
}

TEST_F(FocusCyclerTest, Launcher_CycleFocusForward) {
  ASSERT_TRUE(CreateTray());
  InstallFocusCycleOnLauncher();
  launcher_widget()->Hide();

  // Create a single test window.
  scoped_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycle focus to the status area
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(tray()->GetWidget()->IsActive());

  // Cycle focus to the browser
  focus_cycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

TEST_F(FocusCyclerTest, Launcher_CycleFocusBackwardInvisible) {
  ASSERT_TRUE(CreateTray());
  InstallFocusCycleOnLauncher();
  launcher_widget()->Hide();

  // Create a single test window.
  scoped_ptr<Window> window0(CreateTestWindowInShellWithId(0));
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycle focus to the status area
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(tray()->GetWidget()->IsActive());

  // Cycle focus to the browser
  focus_cycler()->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

}  // namespace test
}  // namespace ash
