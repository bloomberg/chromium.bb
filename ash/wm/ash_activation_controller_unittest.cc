// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/ash_activation_controller.h"

#include "ash/launcher/launcher.h"
#include "ash/root_window_controller.h"
#include "ash/shell_delegate.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_util.h"
#include "ui/aura/window.h"
#include "ui/views/corewm/corewm_switches.h"

namespace ash {

namespace wm {

namespace {

class AshActivationControllerTest : public test::AshTestBase {
 public:
  AshActivationControllerTest()
      : launcher_(NULL), launcher_widget_(NULL), launcher_window_(NULL) {}
  virtual ~AshActivationControllerTest() {}

  virtual void SetUp() OVERRIDE {
    test::AshTestBase::SetUp();
    ash_activation_controller_.reset(new internal::AshActivationController());
    launcher_ = Launcher::ForPrimaryDisplay();
    ASSERT_TRUE(launcher_);
    launcher_widget_ = launcher_->widget();
    ASSERT_TRUE(launcher_widget_);
    launcher_window_ = launcher_widget_->GetNativeWindow();
    ASSERT_TRUE(launcher_window_);
  }

  void SetSpokenFeedbackState(bool enabled) {
    if (Shell::GetInstance()->delegate()->IsSpokenFeedbackEnabled() !=
        enabled) {
      Shell::GetInstance()->delegate()->ToggleSpokenFeedback(
          A11Y_NOTIFICATION_NONE);
    }
  }

 protected:
  scoped_ptr<internal::ActivationControllerDelegate> ash_activation_controller_;
  ash::Launcher* launcher_;
  views::Widget* launcher_widget_;
  aura::Window* launcher_window_;

  DISALLOW_COPY_AND_ASSIGN(AshActivationControllerTest);
};

TEST_F(AshActivationControllerTest, LauncherFallback) {
  // When spoken feedback is disabled, then fallback should not occur.
  {
    SetSpokenFeedbackState(false);
    aura::Window* result = ash_activation_controller_->WillActivateWindow(NULL);
    EXPECT_EQ(NULL, result);
  }

  // When spoken feedback is enabled, then fallback should occur.
  {
    SetSpokenFeedbackState(true);
    aura::Window* result = ash_activation_controller_->WillActivateWindow(NULL);
    EXPECT_EQ(launcher_window_, result);
  }

  // No fallback when activating another window.
  {
    aura::Window* test_window = CreateTestWindowInShellWithId(0);
    aura::Window* result = ash_activation_controller_->
        WillActivateWindow(test_window);
    EXPECT_EQ(test_window, result);
  }
}

TEST_F(AshActivationControllerTest, LauncherFallbackOnShutdown) {
  SetSpokenFeedbackState(true);
  // While shutting down a root window controller, activation controller
  // is notified about destroyed windows and therefore will try to activate
  // a launcher as fallback, which would result in segmentation faults since
  // the launcher's window or the workspace's controller may be already
  // destroyed.
  GetRootWindowController(Shell::GetActiveRootWindow())->CloseChildWindows();

  aura::Window* result = ash_activation_controller_->WillActivateWindow(NULL);
  EXPECT_EQ(NULL, result);
}

TEST_F(AshActivationControllerTest, LauncherEndToEndFallbackOnDestroyTest) {
  // TODO(mtomasz): make this test work with the FocusController.
  if (views::corewm::UseFocusController())
    return;

  // This test checks the whole fallback activation flow.
  SetSpokenFeedbackState(true);

  scoped_ptr<aura::Window> test_window(CreateTestWindowInShellWithId(0));
  ActivateWindow(test_window.get());
  ASSERT_EQ(test_window.get(), GetActiveWindow());

  // Close the window.
  test_window.reset();

  // Verify if the launcher got activated as fallback.
  ASSERT_EQ(launcher_window_, GetActiveWindow());
}

TEST_F(AshActivationControllerTest, LauncherEndToEndFallbackOnMinimizeTest) {
  // TODO(mtomasz): make this test work with the FocusController.
  if (views::corewm::UseFocusController())
    return;

  // This test checks the whole fallback activation flow.
  SetSpokenFeedbackState(true);

  scoped_ptr<aura::Window> test_window(CreateTestWindowInShellWithId(0));
  ActivateWindow(test_window.get());
  ASSERT_EQ(test_window.get(), GetActiveWindow());

  // Minimize the window.
  MinimizeWindow(test_window.get());

  // Verify if the launcher got activated as fallback.
  ASSERT_EQ(launcher_window_, GetActiveWindow());
}

TEST_F(AshActivationControllerTest, LauncherEndToEndNoFallbackOnDestroyTest) {
  // This test checks the whole fallback activation flow when spoken feedback
  // is disabled.
  SetSpokenFeedbackState(false);

  scoped_ptr<aura::Window> test_window(CreateTestWindowInShellWithId(0));
  ActivateWindow(test_window.get());
  ASSERT_EQ(test_window.get(), GetActiveWindow());

  // Close the window.
  test_window.reset();

  // Verify if the launcher didn't get activated as fallback.
  ASSERT_NE(launcher_window_, GetActiveWindow());
}

TEST_F(AshActivationControllerTest, LauncherEndToEndNoFallbackOnMinimizeTest) {
  // This test checks the whole fallback activation flow when spoken feedback
  // is disabled.
  SetSpokenFeedbackState(false);

  scoped_ptr<aura::Window> test_window(CreateTestWindowInShellWithId(0));
  ActivateWindow(test_window.get());
  ASSERT_EQ(test_window.get(), GetActiveWindow());

  // Minimize the window.
  MinimizeWindow(test_window.get());

  // Verify if the launcher didn't get activated as fallback.
  ASSERT_NE(launcher_window_, GetActiveWindow());
}

}  // namespace

}  // namespace wm

}  // namespace ash
