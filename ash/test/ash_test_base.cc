// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_base.h"

#include <string>
#include <vector>

#include "ash/ash_switches.h"
#include "ash/display/display_controller.h"
#include "ash/shell.h"
#include "ash/shell/toplevel_window.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/display_manager_test_api.h"
#include "ash/test/test_session_state_delegate.h"
#include "ash/test/test_shell_delegate.h"
#include "ash/test/test_system_tray_delegate.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/window_positioner.h"
#include "base/command_line.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/test/event_generator_delegate_aura.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/events/gestures/gesture_configuration.h"
#include "ui/gfx/display.h"
#include "ui/gfx/point.h"
#include "ui/gfx/screen.h"

#if defined(OS_CHROMEOS)
#include "ash/system/chromeos/tray_display.h"
#endif

#if defined(OS_WIN)
#include "ash/test/test_metro_viewer_process_host.h"
#include "base/test/test_process_killer_win.h"
#include "base/win/metro.h"
#include "base/win/windows_version.h"
#include "ui/aura/remote_window_tree_host_win.h"
#include "ui/aura/window_tree_host_win.h"
#include "ui/platform_window/win/win_window.h"
#include "win8/test/test_registrar_constants.h"
#endif

#if defined(USE_X11)
#include "ui/gfx/x/x11_connection.h"
#endif

namespace ash {
namespace test {
namespace {

class AshEventGeneratorDelegate
    : public aura::test::EventGeneratorDelegateAura {
 public:
  AshEventGeneratorDelegate() {}
  virtual ~AshEventGeneratorDelegate() {}

  // aura::test::EventGeneratorDelegateAura overrides:
  virtual aura::WindowTreeHost* GetHostAt(
      const gfx::Point& point_in_screen) const OVERRIDE {
    gfx::Screen* screen = Shell::GetScreen();
    gfx::Display display = screen->GetDisplayNearestPoint(point_in_screen);
    return Shell::GetInstance()->display_controller()->
        GetRootWindowForDisplayId(display.id())->GetHost();
  }

  virtual aura::client::ScreenPositionClient* GetScreenPositionClient(
      const aura::Window* window) const OVERRIDE {
    return aura::client::GetScreenPositionClient(window->GetRootWindow());
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AshEventGeneratorDelegate);
};

}  // namespace

/////////////////////////////////////////////////////////////////////////////

AshTestBase::AshTestBase()
    : setup_called_(false),
      teardown_called_(false),
      start_session_(true) {
#if defined(USE_X11)
  // This is needed for tests which use this base class but are run in browser
  // test binaries so don't get the default initialization in the unit test
  // suite.
  gfx::InitializeThreadedX11();
#endif

  thread_bundle_.reset(new content::TestBrowserThreadBundle);
  // Must initialize |ash_test_helper_| here because some tests rely on
  // AshTestBase methods before they call AshTestBase::SetUp().
  ash_test_helper_.reset(new AshTestHelper(base::MessageLoopForUI::current()));
}

AshTestBase::~AshTestBase() {
  CHECK(setup_called_)
      << "You have overridden SetUp but never called AshTestBase::SetUp";
  CHECK(teardown_called_)
      << "You have overridden TearDown but never called AshTestBase::TearDown";
}

void AshTestBase::SetUp() {
  setup_called_ = true;

  // Clears the saved state so that test doesn't use on the wrong
  // default state.
  shell::ToplevelWindow::ClearSavedStateForTest();

  // TODO(jamescook): Can we do this without changing command line?
  // Use the origin (1,1) so that it doesn't over
  // lap with the native mouse cursor.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switches::kAshHostWindowBounds)) {
    command_line->AppendSwitchASCII(
        switches::kAshHostWindowBounds, "1+1-800x600");
  }
#if defined(OS_WIN)
  ui::test::SetUsePopupAsRootWindowForTest(true);
#endif
  ash_test_helper_->SetUp(start_session_);

  Shell::GetPrimaryRootWindow()->Show();
  Shell::GetPrimaryRootWindow()->GetHost()->Show();
  // Move the mouse cursor to far away so that native events doesn't
  // interfere test expectations.
  Shell::GetPrimaryRootWindow()->MoveCursorTo(gfx::Point(-1000, -1000));
  ash::Shell::GetInstance()->cursor_manager()->EnableMouseEvents();

  // Changing GestureConfiguration shouldn't make tests fail.
  ui::GestureConfiguration::set_max_touch_move_in_pixels_for_click(5);

#if defined(OS_WIN)
  if (!command_line->HasSwitch(ash::switches::kForceAshToDesktop)) {
    if (base::win::GetVersion() >= base::win::VERSION_WIN8) {
      ipc_thread_.reset(new base::Thread("test_metro_viewer_ipc_thread"));
      base::Thread::Options options;
      options.message_loop_type = base::MessageLoop::TYPE_IO;
      ipc_thread_->StartWithOptions(options);
      metro_viewer_host_.reset(
          new TestMetroViewerProcessHost(ipc_thread_->message_loop_proxy()));
      CHECK(metro_viewer_host_->LaunchViewerAndWaitForConnection(
          win8::test::kDefaultTestAppUserModelId));
      aura::RemoteWindowTreeHostWin* window_tree_host =
          aura::RemoteWindowTreeHostWin::Instance();
      CHECK(window_tree_host != NULL);
    }
    ash::WindowPositioner::SetMaximizeFirstWindow(true);
  }
#endif
}

void AshTestBase::TearDown() {
  teardown_called_ = true;
  // Flush the message loop to finish pending release tasks.
  RunAllPendingInMessageLoop();

#if defined(OS_WIN)
  if (base::win::GetVersion() >= base::win::VERSION_WIN8 &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kForceAshToDesktop)) {
    // Check that our viewer connection is still established.
    CHECK(!metro_viewer_host_->closed_unexpectedly());
  }
#endif

  ash_test_helper_->TearDown();
#if defined(OS_WIN)
  ui::test::SetUsePopupAsRootWindowForTest(false);
  // Kill the viewer process if we spun one up.
  metro_viewer_host_.reset();

  // Clean up any dangling viewer processes as the metro APIs sometimes leave
  // zombies behind. A default browser process in metro will have the
  // following command line arg so use that to avoid killing all processes named
  // win8::test::kDefaultTestExePath.
  const wchar_t kViewerProcessArgument[] = L"DefaultBrowserServer";
  base::KillAllNamedProcessesWithArgument(win8::test::kDefaultTestExePath,
                                          kViewerProcessArgument);
#endif

  event_generator_.reset();
  // Some tests set an internal display id,
  // reset it here, so other tests will continue in a clean environment.
  gfx::Display::SetInternalDisplayId(gfx::Display::kInvalidDisplayID);
}

ui::test::EventGenerator& AshTestBase::GetEventGenerator() {
  if (!event_generator_) {
    event_generator_.reset(
        new ui::test::EventGenerator(new AshEventGeneratorDelegate()));
  }
  return *event_generator_.get();
}

bool AshTestBase::SupportsMultipleDisplays() {
  return AshTestHelper::SupportsMultipleDisplays();
}

bool AshTestBase::SupportsHostWindowResize() {
  return AshTestHelper::SupportsHostWindowResize();
}

void AshTestBase::UpdateDisplay(const std::string& display_specs) {
  DisplayManagerTestApi display_manager_test_api(
      Shell::GetInstance()->display_manager());
  display_manager_test_api.UpdateDisplay(display_specs);
}

aura::Window* AshTestBase::CurrentContext() {
  return ash_test_helper_->CurrentContext();
}

aura::Window* AshTestBase::CreateTestWindowInShellWithId(int id) {
  return CreateTestWindowInShellWithDelegate(NULL, id, gfx::Rect());
}

aura::Window* AshTestBase::CreateTestWindowInShellWithBounds(
    const gfx::Rect& bounds) {
  return CreateTestWindowInShellWithDelegate(NULL, 0, bounds);
}

aura::Window* AshTestBase::CreateTestWindowInShell(SkColor color,
                                                   int id,
                                                   const gfx::Rect& bounds) {
  return CreateTestWindowInShellWithDelegate(
      new aura::test::ColorTestWindowDelegate(color), id, bounds);
}

aura::Window* AshTestBase::CreateTestWindowInShellWithDelegate(
    aura::WindowDelegate* delegate,
    int id,
    const gfx::Rect& bounds) {
  return CreateTestWindowInShellWithDelegateAndType(
      delegate, ui::wm::WINDOW_TYPE_NORMAL, id, bounds);
}

aura::Window* AshTestBase::CreateTestWindowInShellWithDelegateAndType(
    aura::WindowDelegate* delegate,
    ui::wm::WindowType type,
    int id,
    const gfx::Rect& bounds) {
  aura::Window* window = new aura::Window(delegate);
  window->set_id(id);
  window->SetType(type);
  window->Init(aura::WINDOW_LAYER_TEXTURED);
  window->Show();

  if (bounds.IsEmpty()) {
    ParentWindowInPrimaryRootWindow(window);
  } else {
    gfx::Display display =
        Shell::GetScreen()->GetDisplayMatching(bounds);
    aura::Window* root = ash::Shell::GetInstance()->display_controller()->
        GetRootWindowForDisplayId(display.id());
    gfx::Point origin = bounds.origin();
    wm::ConvertPointFromScreen(root, &origin);
    window->SetBounds(gfx::Rect(origin, bounds.size()));
    aura::client::ParentWindowWithContext(window, root, bounds);
  }
  window->SetProperty(aura::client::kCanMaximizeKey, true);
  return window;
}

void AshTestBase::ParentWindowInPrimaryRootWindow(aura::Window* window) {
  aura::client::ParentWindowWithContext(
      window, Shell::GetPrimaryRootWindow(), gfx::Rect());
}

void AshTestBase::RunAllPendingInMessageLoop() {
  ash_test_helper_->RunAllPendingInMessageLoop();
}

TestScreenshotDelegate* AshTestBase::GetScreenshotDelegate() {
  return ash_test_helper_->test_screenshot_delegate();
}

TestSystemTrayDelegate* AshTestBase::GetSystemTrayDelegate() {
  return static_cast<TestSystemTrayDelegate*>(
      Shell::GetInstance()->system_tray_delegate());
}

void AshTestBase::SetSessionStarted(bool session_started) {
  ash_test_helper_->test_shell_delegate()->test_session_state_delegate()->
      SetActiveUserSessionStarted(session_started);
}

void AshTestBase::SetUserLoggedIn(bool user_logged_in) {
  ash_test_helper_->test_shell_delegate()->test_session_state_delegate()->
      SetHasActiveUser(user_logged_in);
}

void AshTestBase::SetCanLockScreen(bool can_lock_screen) {
  ash_test_helper_->test_shell_delegate()->test_session_state_delegate()->
      SetCanLockScreen(can_lock_screen);
}

void AshTestBase::SetShouldLockScreenBeforeSuspending(bool should_lock) {
  ash_test_helper_->test_shell_delegate()->test_session_state_delegate()->
      SetShouldLockScreenBeforeSuspending(should_lock);
}

void AshTestBase::SetUserAddingScreenRunning(bool user_adding_screen_running) {
  ash_test_helper_->test_shell_delegate()->test_session_state_delegate()->
      SetUserAddingScreenRunning(user_adding_screen_running);
}

void AshTestBase::BlockUserSession(UserSessionBlockReason block_reason) {
  switch (block_reason) {
    case BLOCKED_BY_LOCK_SCREEN:
      SetSessionStarted(true);
      SetUserAddingScreenRunning(false);
      Shell::GetInstance()->session_state_delegate()->LockScreen();
      break;
    case BLOCKED_BY_LOGIN_SCREEN:
      SetUserAddingScreenRunning(false);
      SetSessionStarted(false);
      break;
    case BLOCKED_BY_USER_ADDING_SCREEN:
      SetUserAddingScreenRunning(true);
      SetSessionStarted(true);
      break;
    default:
      NOTREACHED();
      break;
  }
}

void AshTestBase::UnblockUserSession() {
  Shell::GetInstance()->session_state_delegate()->UnlockScreen();
  SetSessionStarted(true);
  SetUserAddingScreenRunning(false);
}


}  // namespace test
}  // namespace ash
