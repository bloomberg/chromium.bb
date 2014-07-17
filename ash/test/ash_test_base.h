// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_ASH_TEST_BASE_H_
#define ASH_TEST_ASH_TEST_BASE_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/wm/public/window_types.h"

#if defined(OS_WIN)
#include "ui/base/win/scoped_ole_initializer.h"
#endif

namespace gfx {
class Rect;
}

namespace ui {
namespace test {
class EventGenerator;
}
}

namespace aura {
class RootWindow;
class Window;
class WindowDelegate;
}  // namespace aura

namespace ash {
class DisplayManager;

namespace test {

class AshTestHelper;
class TestScreenshotDelegate;
class TestSystemTrayDelegate;
#if defined(OS_WIN)
class TestMetroViewerProcessHost;
#endif

class AshTestBase : public testing::Test {
 public:
  AshTestBase();
  virtual ~AshTestBase();

  // testing::Test:
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // Update the display configuration as given in |display_specs|.
  // See ash::test::DisplayManagerTestApi::UpdateDisplay for more details.
  void UpdateDisplay(const std::string& display_specs);

  // Returns a root Window. Usually this is the active root Window, but that
  // method can return NULL sometimes, and in those cases, we fall back on the
  // primary root Window.
  aura::Window* CurrentContext();

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

 protected:
  enum UserSessionBlockReason {
    FIRST_BLOCK_REASON,
    BLOCKED_BY_LOCK_SCREEN = FIRST_BLOCK_REASON,
    BLOCKED_BY_LOGIN_SCREEN,
    BLOCKED_BY_USER_ADDING_SCREEN,
    NUMBER_OF_BLOCK_REASONS
  };

  // Proxy to AshTestHelper::SupportsMultipleDisplays().
  static bool SupportsMultipleDisplays();

  // Proxy to AshTestHelper::SupportsHostWindowResize().
  static bool SupportsHostWindowResize();

  void set_start_session(bool start_session) { start_session_ = start_session; }

  AshTestHelper* ash_test_helper() { return ash_test_helper_.get(); }

  void RunAllPendingInMessageLoop();

  TestScreenshotDelegate* GetScreenshotDelegate();
  TestSystemTrayDelegate* GetSystemTrayDelegate();

  // Utility methods to emulate user logged in or not, session started or not
  // and user able to lock screen or not cases.
  void SetSessionStarted(bool session_started);
  void SetUserLoggedIn(bool user_logged_in);
  void SetCanLockScreen(bool can_lock_screen);
  void SetShouldLockScreenBeforeSuspending(bool should_lock);
  void SetUserAddingScreenRunning(bool user_adding_screen_running);

  // Methods to emulate blocking and unblocking user session with given
  // |block_reason|.
  void BlockUserSession(UserSessionBlockReason block_reason);
  void UnblockUserSession();

 private:
  bool setup_called_;
  bool teardown_called_;
  // |SetUp()| doesn't activate session if this is set to false.
  bool start_session_;
  scoped_ptr<content::TestBrowserThreadBundle> thread_bundle_;
  scoped_ptr<AshTestHelper> ash_test_helper_;
  scoped_ptr<ui::test::EventGenerator> event_generator_;
#if defined(OS_WIN)
  // Note that the order is important here as ipc_thread_ should be destroyed
  // after metro_viewer_host_->channel_.
  scoped_ptr<base::Thread> ipc_thread_;
  scoped_ptr<TestMetroViewerProcessHost> metro_viewer_host_;
  ui::ScopedOleInitializer ole_initializer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(AshTestBase);
};

class NoSessionAshTestBase : public AshTestBase {
 public:
  NoSessionAshTestBase() {
    set_start_session(false);
  }
  virtual ~NoSessionAshTestBase() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NoSessionAshTestBase);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_ASH_TEST_BASE_H_
