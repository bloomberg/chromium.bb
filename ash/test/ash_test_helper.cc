// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_helper.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/ash_switches.h"
#include "ash/display/display_info.h"
#include "ash/material_design/material_design_controller.h"
#include "ash/shell.h"
#include "ash/shell_init_params.h"
#include "ash/test/ash_test_views_delegate.h"
#include "ash/test/content/test_shell_content_state.h"
#include "ash/test/display_manager_test_api.h"
#include "ash/test/material_design_controller_test_api.h"
#include "ash/test/shell_test_api.h"
#include "ash/test/test_screenshot_delegate.h"
#include "ash/test/test_session_state_delegate.h"
#include "ash/test/test_shell_delegate.h"
#include "ash/test/test_system_tray_delegate.h"
#include "base/run_loop.h"
#include "content/public/browser/browser_thread.h"
#include "ui/aura/env.h"
#include "ui/aura/input_state_lookup.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/aura/test/event_generator_delegate_aura.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/test/material_design_controller_test_api.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/message_center/message_center.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/cursor_manager.h"

#if defined(OS_CHROMEOS)
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
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
      test_shell_delegate_(nullptr),
      test_screenshot_delegate_(nullptr),
      content_state_(nullptr),
      test_shell_content_state_(nullptr) {
  CHECK(message_loop_);
#if defined(OS_CHROMEOS)
  dbus_thread_manager_initialized_ = false;
  bluez_dbus_manager_initialized_ = false;
#endif
#if defined(USE_X11)
  aura::test::SetUseOverrideRedirectWindowByDefault(true);
#endif
  aura::test::InitializeAuraEventGeneratorDelegate();
}

AshTestHelper::~AshTestHelper() {
}

void AshTestHelper::SetUp(bool start_session) {
  ResetDisplayIdForTest();
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
    chromeos::DBusThreadManager::Initialize();
    dbus_thread_manager_initialized_ = true;
  }

  if (!bluez::BluezDBusManager::IsInitialized()) {
    bluez::BluezDBusManager::Initialize(
        chromeos::DBusThreadManager::Get()->GetSystemBus(),
        chromeos::DBusThreadManager::Get()->IsUsingStub(
            chromeos::DBusClientBundle::BLUETOOTH));
    bluez_dbus_manager_initialized_ = true;
  }

  // Create CrasAudioHandler for testing since g_browser_process is not
  // created in AshTestBase tests.
  chromeos::CrasAudioHandler::InitializeForTesting();
#endif
  ShellContentState* content_state = content_state_;
  if (!content_state) {
    test_shell_content_state_ = new TestShellContentState;
    content_state = test_shell_content_state_;
  }
  ShellContentState::SetInstance(content_state);

  // Reset the global state for the cursor manager. This includes the
  // last cursor visibility state, etc.
  ::wm::CursorManager::ResetCursorVisibilityStateForTest();

  // ContentTestSuiteBase might have already initialized
  // MaterialDesignController in unit_tests suite.
  ui::test::MaterialDesignControllerTestAPI::Uninitialize();
  ui::MaterialDesignController::Initialize();
  ash::MaterialDesignController::Initialize();
  ShellInitParams init_params;
  init_params.delegate = test_shell_delegate_;
  init_params.context_factory = context_factory;
  init_params.blocking_pool = content::BrowserThread::GetBlockingPool();
  ash::Shell::CreateInstance(init_params);
  aura::test::EnvTestHelper(aura::Env::GetInstance())
      .SetInputStateLookup(std::unique_ptr<aura::InputStateLookup>());

  Shell* shell = Shell::GetInstance();
  if (start_session) {
    GetTestSessionStateDelegate()->SetActiveUserSessionStarted(true);
    GetTestSessionStateDelegate()->SetHasActiveUser(true);
  }

  test::DisplayManagerTestApi().DisableChangeDisplayUponHostResize();
  ShellTestApi(shell).DisableDisplayAnimator();

  test_screenshot_delegate_ = new TestScreenshotDelegate();
  shell->accelerator_controller()->SetScreenshotDelegate(
      std::unique_ptr<ScreenshotDelegate>(test_screenshot_delegate_));
}

void AshTestHelper::TearDown() {
  // Tear down the shell.
  Shell::DeleteInstance();
  test::MaterialDesignControllerTestAPI::Uninitialize();
  ShellContentState::DestroyInstance();

  test_screenshot_delegate_ = NULL;

  // Remove global message center state.
  message_center::MessageCenter::Shutdown();

#if defined(OS_CHROMEOS)
  chromeos::CrasAudioHandler::Shutdown();
  if (bluez_dbus_manager_initialized_) {
    bluez::BluezDBusManager::Shutdown();
    bluez_dbus_manager_initialized_ = false;
  }
  if (dbus_thread_manager_initialized_) {
    chromeos::DBusThreadManager::Shutdown();
    dbus_thread_manager_initialized_ = false;
  }
#endif

  ui::TerminateContextFactoryForTests();

  // Need to reset the initial login status.
  TestSystemTrayDelegate::SetInitialLoginStatus(user::LOGGED_IN_USER);

  ui::ShutdownInputMethodForTesting();
  zero_duration_mode_.reset();

  CHECK(!::wm::ScopedCaptureClient::IsActive());

  views_delegate_.reset();
}

void AshTestHelper::RunAllPendingInMessageLoop() {
  DCHECK(base::MessageLoopForUI::current() == message_loop_);
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

// static
TestSessionStateDelegate* AshTestHelper::GetTestSessionStateDelegate() {
  CHECK(Shell::HasInstance());
  return static_cast<TestSessionStateDelegate*>(
      Shell::GetInstance()->session_state_delegate());
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
