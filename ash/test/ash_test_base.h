// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_ASH_TEST_BASE_H_
#define ASH_TEST_ASH_TEST_BASE_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "ash/common/material_design/material_design_controller.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/display/display.h"
#include "ui/wm/public/window_types.h"

#if defined(OS_WIN)
#include "ui/base/win/scoped_ole_initializer.h"
#endif

namespace aura {
class Window;
class WindowDelegate;
}  // namespace aura

namespace display {
class DisplayManager;

namespace test {
class DisplayManagerTestApi;
}  // namespace test
}  // namespace display

namespace gfx {
class Rect;
}

namespace ui {
namespace test {
class EventGenerator;
}
}

namespace views {
class Widget;
class WidgetDelegate;
}

namespace ash {
class AshTestImplAura;
class SystemTray;
class WmShelf;

namespace test {

class AshTestEnvironment;
class AshTestHelper;
class TestScreenshotDelegate;
class TestSystemTrayDelegate;

class AshTestBase : public testing::Test {
 public:
  AshTestBase();
  ~AshTestBase() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  // Returns the WmShelf for the primary display.
  static WmShelf* GetPrimaryShelf();

  // Returns the system tray on the primary display.
  static SystemTray* GetPrimarySystemTray();

  // Update the display configuration as given in |display_specs|.
  // See ash::test::DisplayManagerTestApi::UpdateDisplay for more details.
  void UpdateDisplay(const std::string& display_specs);

  // Returns a root Window. Usually this is the active root Window, but that
  // method can return NULL sometimes, and in those cases, we fall back on the
  // primary root Window.
  aura::Window* CurrentContext();

  // Creates and shows a widget. See ash/public/cpp/shell_window_ids.h for
  // values for |container_id|.
  static std::unique_ptr<views::Widget> CreateTestWidget(
      views::WidgetDelegate* delegate,
      int container_id,
      const gfx::Rect& bounds);

  // Versions of the functions in aura::test:: that go through our shell
  // StackingController instead of taking a parent.
  aura::Window* CreateTestWindowInShellWithId(int id);
  aura::Window* CreateTestWindowInShellWithBounds(const gfx::Rect& bounds);
  aura::Window* CreateTestWindowInShell(SkColor color,
                                        int id,
                                        const gfx::Rect& bounds);
  aura::Window* CreateTestWindowInShellWithDelegate(
      aura::WindowDelegate* delegate,
      int id,
      const gfx::Rect& bounds);
  aura::Window* CreateTestWindowInShellWithDelegateAndType(
      aura::WindowDelegate* delegate,
      ui::wm::WindowType type,
      int id,
      const gfx::Rect& bounds);

  // Attach |window| to the current shell's root window.
  void ParentWindowInPrimaryRootWindow(aura::Window* window);

  // Returns the EventGenerator that uses screen coordinates and works
  // across multiple displays. It createse a new generator if it
  // hasn't been created yet.
  ui::test::EventGenerator& GetEventGenerator();

  // Convenience method to return the DisplayManager.
  display::DisplayManager* display_manager();

  // Test if moving a mouse to |point_in_screen| warps it to another
  // display.
  bool TestIfMouseWarpsAt(ui::test::EventGenerator& event_generator,
                          const gfx::Point& point_in_screen);

 protected:
  enum UserSessionBlockReason {
    FIRST_BLOCK_REASON,
    BLOCKED_BY_LOCK_SCREEN = FIRST_BLOCK_REASON,
    BLOCKED_BY_LOGIN_SCREEN,
    BLOCKED_BY_USER_ADDING_SCREEN,
    NUMBER_OF_BLOCK_REASONS
  };

  // Returns the rotation currentl active for the display |id|.
  static display::Display::Rotation GetActiveDisplayRotation(int64_t id);

  // Returns the rotation currently active for the internal display.
  static display::Display::Rotation GetCurrentInternalDisplayRotation();

  // Proxy to AshTestHelper::SupportsMultipleDisplays().
  static bool SupportsMultipleDisplays();

  void set_start_session(bool start_session) { start_session_ = start_session; }

  // Sets material mode for the test. This will override material mode set via
  // command line switches.
  void set_material_mode(MaterialDesignController::Mode material_mode) {
    CHECK(!setup_called_);
    material_mode_ = material_mode;
  }

  AshTestHelper* ash_test_helper() { return ash_test_helper_.get(); }

  void RunAllPendingInMessageLoop();

  TestScreenshotDelegate* GetScreenshotDelegate();

  TestSystemTrayDelegate* GetSystemTrayDelegate();

  // Utility methods to emulate user logged in or not, session started or not
  // and user able to lock screen or not cases.
  void SetSessionStarted(bool session_started);
  // Sets the SessionState to active, marking the begining of transitioning to
  // a user session. The session is considered blocked until SetSessionStarted
  // is called.
  void SetSessionStarting();
  void SetUserLoggedIn(bool user_logged_in);
  void SetShouldLockScreenAutomatically(bool should_lock);
  void SetUserAddingScreenRunning(bool user_adding_screen_running);

  // Methods to emulate blocking and unblocking user session with given
  // |block_reason|.
  void BlockUserSession(UserSessionBlockReason block_reason);
  void UnblockUserSession();

  void DisableIME();

  // Swap the primary display with the secondary.
  void SwapPrimaryDisplay();

 private:
  friend class ash::AshTestImplAura;

  bool setup_called_;
  bool teardown_called_;
  // |SetUp()| doesn't activate session if this is set to false.
  bool start_session_;
  MaterialDesignController::Mode material_mode_;
  std::unique_ptr<AshTestEnvironment> ash_test_environment_;
  std::unique_ptr<AshTestHelper> ash_test_helper_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
#if defined(OS_WIN)
  ui::ScopedOleInitializer ole_initializer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(AshTestBase);
};

class NoSessionAshTestBase : public AshTestBase {
 public:
  NoSessionAshTestBase() { set_start_session(false); }
  ~NoSessionAshTestBase() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NoSessionAshTestBase);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_ASH_TEST_BASE_H_
