// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_helper.h"

#include "ash/accelerators/accelerator_controller_delegate_aura.h"
#include "ash/common/material_design/material_design_controller.h"
#include "ash/common/test/material_design_controller_test_api.h"
#include "ash/common/test/test_new_window_client.h"
#include "ash/common/test/test_session_state_delegate.h"
#include "ash/common/test/test_system_tray_delegate.h"
#include "ash/common/test/wm_shell_test_api.h"
#include "ash/common/wm_shell.h"
#include "ash/shell.h"
#include "ash/shell_init_params.h"
#include "ash/test/ash_test_environment.h"
#include "ash/test/ash_test_views_delegate.h"
#include "ash/test/display_manager_test_api.h"
#include "ash/test/shell_test_api.h"
#include "ash/test/test_screenshot_delegate.h"
#include "ash/test/test_shell_delegate.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "ui/aura/env.h"
#include "ui/aura/input_state_lookup.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/aura/test/event_generator_delegate_aura.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/test/material_design_controller_test_api.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/message_center/message_center.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/wm_state.h"

#if defined(OS_CHROMEOS)
#include "ash/system/chromeos/screen_layout_observer.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
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

AshTestHelper::AshTestHelper(AshTestEnvironment* ash_test_environment)
    : ash_test_environment_(ash_test_environment),
      test_shell_delegate_(nullptr),
      test_screenshot_delegate_(nullptr) {
#if defined(OS_CHROMEOS)
  dbus_thread_manager_initialized_ = false;
  bluez_dbus_manager_initialized_ = false;
#endif
#if defined(USE_X11)
  aura::test::SetUseOverrideRedirectWindowByDefault(true);
#endif
  aura::test::InitializeAuraEventGeneratorDelegate();
}

AshTestHelper::~AshTestHelper() {}

void AshTestHelper::SetUp(bool start_session,
                          MaterialDesignController::Mode material_mode) {
  display::ResetDisplayIdForTest();
  wm_state_ = base::MakeUnique<::wm::WMState>();
  views_delegate_ = ash_test_environment_->CreateViewsDelegate();

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
    chromeos::DBusThreadManager::Initialize(
        chromeos::DBusThreadManager::PROCESS_ASH);
    dbus_thread_manager_initialized_ = true;
  }

  if (!bluez::BluezDBusManager::IsInitialized()) {
    bluez::BluezDBusManager::Initialize(
        chromeos::DBusThreadManager::Get()->GetSystemBus(),
        chromeos::DBusThreadManager::Get()->IsUsingFakes());
    bluez_dbus_manager_initialized_ = true;
  }

  // Create CrasAudioHandler for testing since g_browser_process is not
  // created in AshTestBase tests.
  chromeos::CrasAudioHandler::InitializeForTesting();
#endif
  ash_test_environment_->SetUp();
  // Reset the global state for the cursor manager. This includes the
  // last cursor visibility state, etc.
  ::wm::CursorManager::ResetCursorVisibilityStateForTest();

  // ContentTestSuiteBase might have already initialized
  // MaterialDesignController in unit_tests suite.
  ui::test::MaterialDesignControllerTestAPI::Uninitialize();
  ui::MaterialDesignController::Initialize();
  ash::MaterialDesignController::Initialize();
  if (material_mode == MaterialDesignController::Mode::UNINITIALIZED)
    material_mode = MaterialDesignController::GetMode();
  material_design_state_.reset(
      new test::MaterialDesignControllerTestAPI(material_mode));

  ShellInitParams init_params;
  init_params.delegate = test_shell_delegate_;
  init_params.context_factory = context_factory;
  init_params.blocking_pool = ash_test_environment_->GetBlockingPool();
  Shell::CreateInstance(init_params);
  aura::test::EnvTestHelper(aura::Env::GetInstance())
      .SetInputStateLookup(std::unique_ptr<aura::InputStateLookup>());

  Shell* shell = Shell::GetInstance();
  if (start_session) {
    GetTestSessionStateDelegate()->SetActiveUserSessionStarted(true);
    GetTestSessionStateDelegate()->SetHasActiveUser(true);
  }

#if defined(OS_CHROMEOS)
  // Tests that change the display configuration generally don't care about the
  // notifications and the popup UI can interfere with things like cursors.
  shell->screen_layout_observer()->set_show_notifications_for_testing(false);
#endif

  test::DisplayManagerTestApi(Shell::GetInstance()->display_manager())
      .DisableChangeDisplayUponHostResize();
  ShellTestApi(shell).DisableDisplayAnimator();

  test_screenshot_delegate_ = new TestScreenshotDelegate();
  shell->accelerator_controller_delegate()->SetScreenshotDelegate(
      std::unique_ptr<ScreenshotDelegate>(test_screenshot_delegate_));

  WmShellTestApi().SetNewWindowClient(base::MakeUnique<TestNewWindowClient>());
}

void AshTestHelper::TearDown() {
  // Tear down the shell.
  Shell::DeleteInstance();
  material_design_state_.reset();
  test::MaterialDesignControllerTestAPI::Uninitialize();
  ash_test_environment_->TearDown();

  test_screenshot_delegate_ = NULL;

  // Remove global message center state.
  message_center::MessageCenter::Shutdown();

#if defined(OS_CHROMEOS)
  chromeos::CrasAudioHandler::Shutdown();
  if (bluez_dbus_manager_initialized_) {
    device::BluetoothAdapterFactory::Shutdown();
    bluez::BluezDBusManager::Shutdown();
    bluez_dbus_manager_initialized_ = false;
  }
  if (dbus_thread_manager_initialized_) {
    chromeos::DBusThreadManager::Shutdown();
    dbus_thread_manager_initialized_ = false;
  }
#endif

  ui::TerminateContextFactoryForTests();

  TestSystemTrayDelegate::SetSystemUpdateRequired(false);

  ui::ShutdownInputMethodForTesting();
  zero_duration_mode_.reset();

  CHECK(!::wm::ScopedCaptureClient::IsActive());

  views_delegate_.reset();
}

void AshTestHelper::RunAllPendingInMessageLoop() {
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

// static
TestSessionStateDelegate* AshTestHelper::GetTestSessionStateDelegate() {
  CHECK(WmShell::HasInstance());
  return static_cast<TestSessionStateDelegate*>(
      WmShell::Get()->GetSessionStateDelegate());
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

}  // namespace test
}  // namespace ash
