// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_ASH_TEST_HELPER_H_
#define ASH_TEST_ASH_TEST_HELPER_H_

#include <memory>

#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/test/material_design_controller_test_api.h"
#include "base/compiler_specific.h"
#include "base/macros.h"

namespace aura {
class Window;
}  // namespace aura

namespace ui {
class ScopedAnimationDurationScaleMode;
}  // namespace ui

namespace wm {
class WMState;
}

namespace ash {
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
  // |material_mode| determines the material design mode to be used for the
  // tests. If |material_mode| is UNINITIALIZED, the value from command line
  // switches is used.
  void SetUp(bool start_session, MaterialDesignController::Mode material_mode);

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

  // True if the running environment supports multiple displays,
  // or false otherwise (e.g. win8 bot).
  static bool SupportsMultipleDisplays();

 private:
  AshTestEnvironment* ash_test_environment_;  // Not owned.
  TestShellDelegate* test_shell_delegate_;  // Owned by ash::Shell.
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> zero_duration_mode_;

  // Owned by ash::AcceleratorController
  TestScreenshotDelegate* test_screenshot_delegate_;

  std::unique_ptr<::wm::WMState> wm_state_;
  std::unique_ptr<AshTestViewsDelegate> views_delegate_;

#if defined(OS_CHROMEOS)
  // Check if DBus Thread Manager was initialized here.
  bool dbus_thread_manager_initialized_;
  // Check if Bluez DBus Manager was initialized here.
  bool bluez_dbus_manager_initialized_;
#endif

  std::unique_ptr<test::MaterialDesignControllerTestAPI> material_design_state_;

  DISALLOW_COPY_AND_ASSIGN(AshTestHelper);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_ASH_TEST_HELPER_H_
