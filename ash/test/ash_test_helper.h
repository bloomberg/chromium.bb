// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_ASH_TEST_HELPER_H_
#define ASH_TEST_ASH_TEST_HELPER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/aura/test/mus/test_window_tree_client_setup.h"

namespace aura {
class Window;
class WindowTreeClientPrivate;
}  // namespace aura

namespace display {
class Display;
}

namespace ui {
class ScopedAnimationDurationScaleMode;
}  // namespace ui

namespace wm {
class WMState;
}

namespace ash {

class RootWindowController;

namespace mus {
class WindowManagerApplication;
}

namespace test {

class AshTestEnvironment;
class AshTestViewsDelegate;
class TestScreenshotDelegate;
class TestShellDelegate;
class TestSessionStateDelegate;

// A helper class that does common initialization required for Ash. Creates a
// root window and an ash::Shell instance with a test delegate.
class AshTestHelper {
 public:
  explicit AshTestHelper(AshTestEnvironment* ash_test_environment);
  ~AshTestHelper();

  // Creates the ash::Shell and performs associated initialization.  Set
  // |start_session| to true if the user should log in before the test is run.
  void SetUp(bool start_session);

  // Destroys the ash::Shell and performs associated cleanup.
  void TearDown();

  // Returns a root Window. Usually this is the active root Window, but that
  // method can return NULL sometimes, and in those cases, we fall back on the
  // primary root Window.
  aura::Window* CurrentContext();

  void RunAllPendingInMessageLoop();

  static TestSessionStateDelegate* GetTestSessionStateDelegate();

  TestShellDelegate* test_shell_delegate() { return test_shell_delegate_; }
  void set_test_shell_delegate(TestShellDelegate* test_shell_delegate) {
    test_shell_delegate_ = test_shell_delegate;
  }
  TestScreenshotDelegate* test_screenshot_delegate() {
    return test_screenshot_delegate_;
  }
  AshTestViewsDelegate* views_delegate() { return views_delegate_.get(); }

  AshTestEnvironment* ash_test_environment() { return ash_test_environment_; }

  // Version of DisplayManagerTestApi::UpdateDisplay() for mash.
  void UpdateDisplayForMash(const std::string& display_spec);

  display::Display GetSecondaryDisplay();

  // Null in classic ash.
  mus::WindowManagerApplication* window_manager_app() {
    return window_manager_app_.get();
  }

 private:
  // Called when running in mash to create the WindowManager.
  void CreateMashWindowManager();

  // Called when running in ash to create Shell.
  void CreateShell();

  // Creates a new RootWindowController based on |display_spec|. The origin is
  // set to |next_x| and on exit |next_x| is set to the origin + the width.
  RootWindowController* CreateRootWindowController(
      const std::string& display_spec,
      int* next_x);

  // Updates an existing display based on |display_spec|.
  void UpdateDisplay(RootWindowController* root_window_controller,
                     const std::string& display_spec,
                     int* next_x);

  std::vector<RootWindowController*> GetRootsOrderedByDisplayId();

  AshTestEnvironment* ash_test_environment_;  // Not owned.
  TestShellDelegate* test_shell_delegate_;  // Owned by ash::Shell.
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> zero_duration_mode_;

  // Owned by ash::AcceleratorController
  TestScreenshotDelegate* test_screenshot_delegate_;

  std::unique_ptr<::wm::WMState> wm_state_;
  std::unique_ptr<AshTestViewsDelegate> views_delegate_;

  // Check if DBus Thread Manager was initialized here.
  bool dbus_thread_manager_initialized_;
  // Check if Bluez DBus Manager was initialized here.
  bool bluez_dbus_manager_initialized_;

  aura::TestWindowTreeClientSetup window_tree_client_setup_;
  std::unique_ptr<mus::WindowManagerApplication> window_manager_app_;
  std::unique_ptr<aura::WindowTreeClientPrivate> window_tree_client_private_;
  // Id for the next Display created by CreateRootWindowController().
  int64_t next_display_id_ = 1;

  DISALLOW_COPY_AND_ASSIGN(AshTestHelper);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_ASH_TEST_HELPER_H_
