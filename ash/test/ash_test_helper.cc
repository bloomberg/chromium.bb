// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_helper.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/shell_init_params.h"
#include "ash/test/ash_test_views_delegate.h"
#include "ash/test/display_manager_test_api.h"
#include "ash/test/shell_test_api.h"
#include "ash/test/test_screenshot_delegate.h"
#include "ash/test/test_session_state_delegate.h"
#include "ash/test/test_shell_delegate.h"
#include "ash/test/test_system_tray_delegate.h"
#include "base/run_loop.h"
#include "ui/aura/env.h"
#include "ui/aura/input_state_lookup.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/aura/test/event_generator_delegate_aura.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/message_center/message_center.h"
#include "ui/wm/core/capture_controller.h"

#if defined(OS_CHROMEOS)
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "ui/keyboard/keyboard.h"
#endif

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

#if defined(USE_X11)
#include "ui/aura/window_tree_host_x11.h"
#endif

namespace ash {
namespace test {

AshTestHelper::AshTestHelper(base::MessageLoopForUI* message_loop)
    : message_loop_(message_loop),
      test_shell_delegate_(NULL),
      test_screenshot_delegate_(NULL),
      dbus_thread_manager_initialized_(false) {
  CHECK(message_loop_);
#if defined(USE_X11)
  aura::test::SetUseOverrideRedirectWindowByDefault(true);
#endif
  aura::test::InitializeAuraEventGeneratorDelegate();
}

AshTestHelper::~AshTestHelper() {
}

void AshTestHelper::SetUp(bool start_session) {
  views_delegate_.reset(new AshTestViewsDelegate);

  // Disable animations during tests.
  zero_duration_mode_.reset(new ui::ScopedAnimationDurationScaleMode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION));
  ui::InitializeInputMethodForTesting();

  bool enable_pixel_output = false;
  ui::ContextFactory* context_factory =
      ui::InitializeContextFactoryForTests(enable_pixel_output);

  // Creates Shell and hook with Desktop.
  if (!test_shell_delegate_)
    test_shell_delegate_ = new TestShellDelegate;

  // Creates MessageCenter since g_browser_process is not created in AshTestBase
  // tests.
  message_center::MessageCenter::Initialize();

#if defined(OS_CHROMEOS)
  // Create DBusThreadManager for testing.
  if (!chromeos::DBusThreadManager::IsInitialized()) {
    chromeos::DBusThreadManager::InitializeWithStub();
    dbus_thread_manager_initialized_ = true;
  }
  // Create CrasAudioHandler for testing since g_browser_process is not
  // created in AshTestBase tests.
  chromeos::CrasAudioHandler::InitializeForTesting();
#endif
  ShellInitParams init_params;
  init_params.delegate = test_shell_delegate_;
  init_params.context_factory = context_factory;
  ash::Shell::CreateInstance(init_params);
  aura::test::EnvTestHelper(aura::Env::GetInstance()).SetInputStateLookup(
      scoped_ptr<aura::InputStateLookup>());

  Shell* shell = Shell::GetInstance();
  if (start_session) {
    test_shell_delegate_->test_session_state_delegate()->
        SetActiveUserSessionStarted(true);
    test_shell_delegate_->test_session_state_delegate()->
        SetHasActiveUser(true);
  }

  test::DisplayManagerTestApi(shell->display_manager()).
      DisableChangeDisplayUponHostResize();
  ShellTestApi(shell).DisableDisplayConfiguratorAnimation();

  test_screenshot_delegate_ = new TestScreenshotDelegate();
  shell->accelerator_controller()->SetScreenshotDelegate(
      scoped_ptr<ScreenshotDelegate>(test_screenshot_delegate_));
}

void AshTestHelper::TearDown() {
  // Tear down the shell.
  Shell::DeleteInstance();
  test_screenshot_delegate_ = NULL;

  // Remove global message center state.
  message_center::MessageCenter::Shutdown();

#if defined(OS_CHROMEOS)
  chromeos::CrasAudioHandler::Shutdown();
  if (dbus_thread_manager_initialized_) {
    chromeos::DBusThreadManager::Shutdown();
    dbus_thread_manager_initialized_ = false;
  }
  keyboard::ResetKeyboardForTesting();
#endif

  aura::Env::DeleteInstance();
  ui::TerminateContextFactoryForTests();

  // Need to reset the initial login status.
  TestSystemTrayDelegate::SetInitialLoginStatus(user::LOGGED_IN_USER);

  ui::ShutdownInputMethodForTesting();
  zero_duration_mode_.reset();

  CHECK(!wm::ScopedCaptureClient::IsActive());

  views_delegate_.reset();
}

void AshTestHelper::RunAllPendingInMessageLoop() {
  DCHECK(base::MessageLoopForUI::current() == message_loop_);
  aura::Env::CreateInstance(true);
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

aura::Window* AshTestHelper::CurrentContext() {
  aura::Window* root_window = Shell::GetTargetRootWindow();
  if (!root_window)
    root_window = Shell::GetPrimaryRootWindow();
  DCHECK(root_window);
  return root_window;
}

// static
bool AshTestHelper::SupportsMultipleDisplays() {
#if defined(OS_WIN)
  return false;
#else
  return true;
#endif
}

// static
bool AshTestHelper::SupportsHostWindowResize() {
#if defined(OS_WIN)
  return false;
#else
  return true;
#endif
}

}  // namespace test
}  // namespace ash
