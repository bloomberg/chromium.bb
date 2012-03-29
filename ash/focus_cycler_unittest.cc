// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/focus_cycler.h"

#include "ash/launcher/launcher.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/status_area/status_area_view.h"
#include "ash/wm/window_util.h"
#include "ash/test/ash_test_base.h"
#include "ash/shell_factory.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace test {

using aura::test::CreateTestWindowWithId;
using aura::Window;
using internal::FocusCycler;

namespace {

internal::StatusAreaView* GetStatusAreaView(views::Widget* widget) {
  return static_cast<internal::StatusAreaView*>(
      widget->GetContentsView()->child_at(0));
}

}  // namespace

typedef AshTestBase FocusCyclerTest;

TEST_F(FocusCyclerTest, CycleFocusBrowserOnly) {
  scoped_ptr<FocusCycler> focus_cycler(new FocusCycler());

  // Create a single test window.
  Window* default_container =
      ash::Shell::GetInstance()->GetContainer(
          internal::kShellWindowId_DefaultContainer);
  scoped_ptr<Window> window0(CreateTestWindowWithId(0, default_container));
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycle the window
  focus_cycler->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

TEST_F(FocusCyclerTest, CycleFocusForward) {
  scoped_ptr<FocusCycler> focus_cycler(new FocusCycler());

  // Add the Status area
  views::Widget* status_widget = internal::CreateStatusArea(NULL);
  ASSERT_TRUE(status_widget);
  focus_cycler->AddWidget(status_widget);
  GetStatusAreaView(status_widget)->SetFocusCyclerForTesting(
      focus_cycler.get());

  // Add a mock button to the status area.
  status_widget->GetContentsView()->AddChildView(
      new views::MenuButton(NULL, string16(), NULL, false));

  // Add the launcher
  Launcher* launcher = Shell::GetInstance()->launcher();
  ASSERT_TRUE(launcher);
  views::Widget* launcher_widget = launcher->widget();
  ASSERT_TRUE(launcher_widget);
  launcher->SetFocusCycler(focus_cycler.get());

  // Create a single test window.
  Window* default_container =
      ash::Shell::GetInstance()->GetContainer(
          internal::kShellWindowId_DefaultContainer);
  scoped_ptr<Window> window0(CreateTestWindowWithId(0, default_container));
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycle focus to the status area
  focus_cycler->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(status_widget->IsActive());

  // Cycle focus to the launcher
  focus_cycler->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(launcher_widget->IsActive());

  // Cycle focus to the browser
  focus_cycler->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

TEST_F(FocusCyclerTest, CycleFocusBackward) {
  scoped_ptr<FocusCycler> focus_cycler(new FocusCycler());

  // Add the Status area
  views::Widget* status_widget = internal::CreateStatusArea(NULL);
  ASSERT_TRUE(status_widget);
  focus_cycler->AddWidget(status_widget);
  GetStatusAreaView(status_widget)->SetFocusCyclerForTesting(
      focus_cycler.get());

  // Add a mock button to the status area.
  status_widget->GetContentsView()->AddChildView(
      new views::MenuButton(NULL, string16(), NULL, false));

  // Add the launcher
  Launcher* launcher = Shell::GetInstance()->launcher();
  ASSERT_TRUE(launcher);
  views::Widget* launcher_widget = launcher->widget();
  ASSERT_TRUE(launcher_widget);
  launcher->SetFocusCycler(focus_cycler.get());

  // Create a single test window.
  Window* default_container =
      ash::Shell::GetInstance()->GetContainer(
          internal::kShellWindowId_DefaultContainer);
  scoped_ptr<Window> window0(CreateTestWindowWithId(0, default_container));
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycle focus to the launcher
  focus_cycler->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(launcher_widget->IsActive());

  // Cycle focus to the status area
  focus_cycler->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(status_widget->IsActive());

  // Cycle focus to the browser
  focus_cycler->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

class FocusCyclerLauncherTest : public AshTestBase {
 public:
  FocusCyclerLauncherTest() : AshTestBase() {}
  virtual ~FocusCyclerLauncherTest() {}

  virtual void SetUp() OVERRIDE {
    AshTestBase::SetUp();

    // Hide the launcher
    Launcher* launcher = Shell::GetInstance()->launcher();
    ASSERT_TRUE(launcher);
    views::Widget* launcher_widget = launcher->widget();
    ASSERT_TRUE(launcher_widget);
    launcher_widget->Hide();
  }

  virtual void TearDown() OVERRIDE {
    // Show the launcher
    Launcher* launcher = Shell::GetInstance()->launcher();
    ASSERT_TRUE(launcher);
    views::Widget* launcher_widget = launcher->widget();
    ASSERT_TRUE(launcher_widget);
    launcher_widget->Show();

    AshTestBase::TearDown();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FocusCyclerLauncherTest);
};

TEST_F(FocusCyclerLauncherTest, CycleFocusForwardInvisible) {
  scoped_ptr<FocusCycler> focus_cycler(new FocusCycler());

  // Add the Status area
  views::Widget* status_widget = internal::CreateStatusArea(NULL);
  ASSERT_TRUE(status_widget);
  focus_cycler->AddWidget(status_widget);
  GetStatusAreaView(status_widget)->SetFocusCyclerForTesting(
      focus_cycler.get());

  // Add a mock button to the status area.
  status_widget->GetContentsView()->AddChildView(
      new views::MenuButton(NULL, string16(), NULL, false));

  // Add the launcher
  Launcher* launcher = Shell::GetInstance()->launcher();
  ASSERT_TRUE(launcher);
  views::Widget* launcher_widget = launcher->widget();
  ASSERT_TRUE(launcher_widget);
  launcher->SetFocusCycler(focus_cycler.get());

  // Create a single test window.
  Window* default_container =
      ash::Shell::GetInstance()->GetContainer(
          internal::kShellWindowId_DefaultContainer);
  scoped_ptr<Window> window0(CreateTestWindowWithId(0, default_container));
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycle focus to the status area
  focus_cycler->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(status_widget->IsActive());

  // Cycle focus to the browser
  focus_cycler->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

TEST_F(FocusCyclerLauncherTest, CycleFocusBackwardInvisible) {
  scoped_ptr<FocusCycler> focus_cycler(new FocusCycler());

  // Add the Status area
  views::Widget* status_widget = internal::CreateStatusArea(NULL);
  ASSERT_TRUE(status_widget);
  focus_cycler->AddWidget(status_widget);
  GetStatusAreaView(status_widget)->SetFocusCyclerForTesting(
      focus_cycler.get());

  // Add a mock button to the status area.
  status_widget->GetContentsView()->AddChildView(
      new views::MenuButton(NULL, string16(), NULL, false));

  // Add the launcher
  Launcher* launcher = Shell::GetInstance()->launcher();
  ASSERT_TRUE(launcher);
  views::Widget* launcher_widget = launcher->widget();
  ASSERT_TRUE(launcher_widget);
  launcher->SetFocusCycler(focus_cycler.get());

  // Create a single test window.
  Window* default_container =
      ash::Shell::GetInstance()->GetContainer(
          internal::kShellWindowId_DefaultContainer);
  scoped_ptr<Window> window0(CreateTestWindowWithId(0, default_container));
  wm::ActivateWindow(window0.get());
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Cycle focus to the status area
  focus_cycler->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(status_widget->IsActive());

  // Cycle focus to the browser
  focus_cycler->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

}  // namespace test
}  // namespace ash
