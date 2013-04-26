// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_helper.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/test/display_manager_test_api.h"
#include "ash/test/shell_test_api.h"
#include "ash/test/test_shell_delegate.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/base/ime/text_input_test_support.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"

#if defined(OS_WIN)
#include "ash/test/test_metro_viewer_process_host.h"
#include "base/test/test_process_killer_win.h"
#include "base/win/metro.h"
#include "base/win/windows_version.h"
#include "ui/aura/remote_root_window_host_win.h"
#include "ui/aura/root_window_host_win.h"
#include "ui/base/ime/win/tsf_bridge.h"
#include "win8/test/test_registrar_constants.h"
#endif

#if defined(ENABLE_MESSAGE_CENTER)
#include "ui/message_center/message_center.h"
#endif

namespace ash {
namespace test {

AshTestHelper::AshTestHelper(base::MessageLoopForUI* message_loop)
    : message_loop_(message_loop),
      test_shell_delegate_(NULL) {
  CHECK(message_loop_);
}

AshTestHelper::~AshTestHelper() {
}

void AshTestHelper::SetUp() {
  // TODO(jamescook): Can we do this without changing command line?
  // Use the origin (1,1) so that it doesn't over
  // lap with the native mouse cursor.
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kAshHostWindowBounds, "1+1-800x600");
#if defined(OS_WIN)
  aura::test::SetUsePopupAsRootWindowForTest(true);
  if (base::win::IsTSFAwareRequired())
    ui::TSFBridge::Initialize();
#endif
  // Disable animations during tests.
  zero_duration_mode_.reset(new ui::ScopedAnimationDurationScaleMode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION));
  ui::TextInputTestSupport::Initialize();

  // Creates Shell and hook with Desktop.
  test_shell_delegate_ = new TestShellDelegate;

#if defined(ENABLE_MESSAGE_CENTER)
  // Creates MessageCenter since g_browser_process is not created in AshTestBase
  // tests.
  message_center::MessageCenter::Initialize();
#endif
  ash::Shell::CreateInstance(test_shell_delegate_);
  Shell* shell = Shell::GetInstance();
  test::DisplayManagerTestApi(shell->display_manager()).
      DisableChangeDisplayUponHostResize();

  Shell::GetPrimaryRootWindow()->Show();
  Shell::GetPrimaryRootWindow()->ShowRootWindow();
  // Move the mouse cursor to far away so that native events doesn't
  // interfere test expectations.
  Shell::GetPrimaryRootWindow()->MoveCursorTo(gfx::Point(-1000, -1000));
  shell->cursor_manager()->EnableMouseEvents();
  ShellTestApi(shell).DisableOutputConfiguratorAnimation();

#if defined(OS_WIN)
  if (base::win::GetVersion() >= base::win::VERSION_WIN8 &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kForceAshToDesktop)) {
    metro_viewer_host_.reset(new TestMetroViewerProcessHost("viewer"));
    CHECK(metro_viewer_host_->LaunchViewerAndWaitForConnection(
        win8::test::kDefaultTestAppUserModelId));
    aura::RemoteRootWindowHostWin* root_window_host =
        aura::RemoteRootWindowHostWin::Instance();
    CHECK(root_window_host != NULL);
  }
#endif
}

void AshTestHelper::TearDown() {
#if defined(OS_WIN)
  if (base::win::GetVersion() >= base::win::VERSION_WIN8 &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          ash::switches::kForceAshToDesktop)) {
    // Check that our viewer connection is still established.
    CHECK(!metro_viewer_host_->closed_unexpectedly());
  }
#endif

  // Tear down the shell.
  Shell::DeleteInstance();

#if defined(ENABLE_MESSAGE_CENTER)
  // Remove global message center state.
  message_center::MessageCenter::Shutdown();
#endif

  aura::Env::DeleteInstance();
  ui::TextInputTestSupport::Shutdown();

#if defined(OS_WIN)
  aura::test::SetUsePopupAsRootWindowForTest(false);
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
  zero_duration_mode_.reset();
}

void AshTestHelper::RunAllPendingInMessageLoop() {
#if !defined(OS_MACOSX)
  DCHECK(MessageLoopForUI::current() == message_loop_);
  base::RunLoop run_loop(aura::Env::GetInstance()->GetDispatcher());
  run_loop.RunUntilIdle();
#endif
}

aura::RootWindow* AshTestHelper::CurrentContext() {
  aura::RootWindow* root_window = Shell::GetActiveRootWindow();
  if (!root_window)
    root_window = Shell::GetPrimaryRootWindow();
  DCHECK(root_window);
  return root_window;
}

}  // namespace test
}  // namespace ash
