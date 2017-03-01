// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_helper.h"

#include "ash/accelerators/accelerator_controller_delegate_aura.h"
#include "ash/common/test/test_session_state_delegate.h"
#include "ash/common/test/test_system_tray_delegate.h"
#include "ash/common/test/wm_shell_test_api.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/mus/screen_mus.h"
#include "ash/mus/window_manager.h"
#include "ash/mus/window_manager_application.h"
#include "ash/shell.h"
#include "ash/shell_init_params.h"
#include "ash/system/chromeos/screen_layout_observer.h"
#include "ash/test/ash_test_environment.h"
#include "ash/test/ash_test_views_delegate.h"
#include "ash/test/shell_test_api.h"
#include "ash/test/test_screenshot_delegate.h"
#include "ash/test/test_shell_delegate.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/string_split.h"
#include "base/test/sequenced_worker_pool_owner.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "ui/aura/env.h"
#include "ui/aura/input_state_lookup.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/test/env_test_helper.h"
#include "ui/aura/test/event_generator_delegate_aura.h"
#include "ui/aura/test/mus/window_tree_client_private.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/platform_window_defaults.h"
#include "ui/base/test/material_design_controller_test_api.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/context_factories_for_test.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/message_center/message_center.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/wm_state.h"

using display::ManagedDisplayInfo;

namespace ash {
namespace test {
namespace {

bool IsMash() {
  return aura::Env::GetInstance()->mode() == aura::Env::Mode::MUS;
}

bool CompareByDisplayId(RootWindowController* root1,
                        RootWindowController* root2) {
  return root1->GetWindow()->GetDisplayNearestWindow().id() <
         root2->GetWindow()->GetDisplayNearestWindow().id();
}

}  // namespace

AshTestHelper::AshTestHelper(AshTestEnvironment* ash_test_environment)
    : ash_test_environment_(ash_test_environment),
      test_shell_delegate_(nullptr),
      test_screenshot_delegate_(nullptr),
      dbus_thread_manager_initialized_(false),
      bluez_dbus_manager_initialized_(false) {
  ui::test::EnableTestConfigForPlatformWindows();
  aura::test::InitializeAuraEventGeneratorDelegate();
}

AshTestHelper::~AshTestHelper() {}

void AshTestHelper::SetUp(bool start_session) {
  display::ResetDisplayIdForTest();
  const bool is_mash = IsMash();
  if (is_mash)
    aura::test::EnvTestHelper().SetAlwaysUseLastMouseLocation(true);
  // WindowManager creates WMState for mash.
  if (!is_mash)
    wm_state_ = base::MakeUnique<::wm::WMState>();
  views_delegate_ = ash_test_environment_->CreateViewsDelegate();

  // Disable animations during tests.
  zero_duration_mode_.reset(new ui::ScopedAnimationDurationScaleMode(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION));
  ui::InitializeInputMethodForTesting();

  // Creates Shell and hook with Desktop.
  if (!test_shell_delegate_)
    test_shell_delegate_ = new TestShellDelegate;

  if (!is_mash) {
    // All of this initialization is done in WindowManagerApplication for mash.

    // Creates MessageCenter since g_browser_process is not created in
    // AshTestBase tests.
    message_center::MessageCenter::Initialize();

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
  }

  ash_test_environment_->SetUp();
  // Reset the global state for the cursor manager. This includes the
  // last cursor visibility state, etc.
  ::wm::CursorManager::ResetCursorVisibilityStateForTest();

  // ContentTestSuiteBase might have already initialized
  // MaterialDesignController in unit_tests suite.
  ui::test::MaterialDesignControllerTestAPI::Uninitialize();
  ui::MaterialDesignController::Initialize();

  if (is_mash)
    CreateMashWindowManager();
  else
    CreateShell();

  aura::test::EnvTestHelper().SetInputStateLookup(
      std::unique_ptr<aura::InputStateLookup>());

  Shell* shell = Shell::GetInstance();
  if (start_session) {
    GetTestSessionStateDelegate()->SetActiveUserSessionStarted(true);
    GetTestSessionStateDelegate()->SetHasActiveUser(true);
  }

  if (!is_mash) {
    // ScreenLayoutObserver is specific to classic-ash.
    // Tests that change the display configuration generally don't care about
    // the notifications and the popup UI can interfere with things like
    // cursors.
    shell->screen_layout_observer()->set_show_notifications_for_testing(false);

    // DisplayManager is specific to classic-ash.
    display::test::DisplayManagerTestApi(
        Shell::GetInstance()->display_manager())
        .DisableChangeDisplayUponHostResize();
    ShellTestApi(shell).DisableDisplayAnimator();

    // TODO: disabled for mash as AcceleratorControllerDelegateAura isn't
    // created in mash http://crbug.com/632111.
    test_screenshot_delegate_ = new TestScreenshotDelegate();
    shell->accelerator_controller_delegate()->SetScreenshotDelegate(
        std::unique_ptr<ScreenshotDelegate>(test_screenshot_delegate_));
  }
}

void AshTestHelper::TearDown() {
  window_manager_app_.reset();

  const bool is_mash = IsMash();

  // WindowManger owns the Shell in mash.
  if (!is_mash)
    Shell::DeleteInstance();

  // Suspend the tear down until all resources are returned via
  // MojoCompositorFrameSinkClient::ReclaimResources()
  RunAllPendingInMessageLoop();
  ash_test_environment_->TearDown();

  test_screenshot_delegate_ = NULL;

  if (!is_mash) {
    // Remove global message center state.
    message_center::MessageCenter::Shutdown();

    chromeos::CrasAudioHandler::Shutdown();
  }

  if (bluez_dbus_manager_initialized_) {
    device::BluetoothAdapterFactory::Shutdown();
    bluez::BluezDBusManager::Shutdown();
    bluez_dbus_manager_initialized_ = false;
  }

  if (dbus_thread_manager_initialized_) {
    chromeos::DBusThreadManager::Shutdown();
    dbus_thread_manager_initialized_ = false;
  }

  ui::TerminateContextFactoryForTests();

  ui::ShutdownInputMethodForTesting();
  zero_duration_mode_.reset();

  views_delegate_.reset();
  wm_state_.reset();

  // WindowManager owns the CaptureController for mash.
  CHECK(is_mash || !::wm::CaptureController::Get());
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

void AshTestHelper::UpdateDisplayForMash(const std::string& display_spec) {
  // TODO(erg): This is not equivalent to display::DisplayManager::
  // UpdateDisplaysWith(). This does not calculate all the internal metric
  // changes, does not calculate focus changes and does not dispatch to all the
  // observers in ash which want to be notified about the previous two.
  //
  // Once this is fixed, RootWindowControllerTest.MoveWindows_Basic, among
  // other unit tests, should work. http://crbug.com/695632.
  CHECK(IsMash());
  const std::vector<std::string> parts = base::SplitString(
      display_spec, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  std::vector<RootWindowController*> root_window_controllers =
      GetRootsOrderedByDisplayId();
  int next_x = 0;
  for (size_t i = 0,
              end = std::min(parts.size(), root_window_controllers.size());
       i < end; ++i) {
    UpdateDisplay(root_window_controllers[i], parts[i], &next_x);
  }
  for (size_t i = root_window_controllers.size(); i < parts.size(); ++i) {
    root_window_controllers.push_back(
        CreateRootWindowController(parts[i], &next_x));
  }
  const bool in_shutdown = false;
  while (root_window_controllers.size() > parts.size()) {
    window_manager_app_->window_manager()->DestroyRootWindowController(
        root_window_controllers.back(), in_shutdown);
    root_window_controllers.pop_back();
  }
}

display::Display AshTestHelper::GetSecondaryDisplay() {
  if (!IsMash())
    return Shell::GetInstance()->display_manager()->GetSecondaryDisplay();

  std::vector<RootWindowController*> roots = GetRootsOrderedByDisplayId();
  CHECK_LE(2U, roots.size());
  return roots.size() < 2 ? display::Display()
                          : roots[1]->GetWindow()->GetDisplayNearestWindow();
}

void AshTestHelper::CreateMashWindowManager() {
  CHECK(IsMash());
  window_manager_app_ = base::MakeUnique<mus::WindowManagerApplication>();

  window_manager_app_->window_manager_.reset(new mus::WindowManager(nullptr));
  window_manager_app_->window_manager()->shell_delegate_for_test_.reset(
      test_shell_delegate_);
  window_manager_app_->window_manager()
      ->create_session_state_delegate_stub_for_test_ = false;

  window_tree_client_setup_.InitForWindowManager(
      window_manager_app_->window_manager_.get(),
      window_manager_app_->window_manager_.get());
  aura::test::EnvTestHelper().SetWindowTreeClient(
      window_tree_client_setup_.window_tree_client());
  window_manager_app_->InitWindowManager(
      window_tree_client_setup_.OwnWindowTreeClient(),
      ash_test_environment_->GetBlockingPool());

  aura::WindowTreeClient* window_tree_client =
      window_manager_app_->window_manager()->window_tree_client();
  window_tree_client_private_ =
      base::MakeUnique<aura::WindowTreeClientPrivate>(window_tree_client);
  int next_x = 0;
  CreateRootWindowController("800x600", &next_x);
}

void AshTestHelper::CreateShell() {
  CHECK(!IsMash());
  ui::ContextFactory* context_factory = nullptr;
  ui::ContextFactoryPrivate* context_factory_private = nullptr;
  bool enable_pixel_output = false;
  ui::InitializeContextFactoryForTests(enable_pixel_output, &context_factory,
                                       &context_factory_private);
  ShellInitParams init_params;
  init_params.delegate = test_shell_delegate_;
  init_params.context_factory = context_factory;
  init_params.context_factory_private = context_factory_private;
  init_params.blocking_pool = ash_test_environment_->GetBlockingPool();
  Shell::CreateInstance(init_params);
}

RootWindowController* AshTestHelper::CreateRootWindowController(
    const std::string& display_spec,
    int* next_x) {
  ManagedDisplayInfo display_info =
      ManagedDisplayInfo::CreateFromSpec(display_spec);
  gfx::Rect bounds = display_info.bounds_in_native();
  bounds.set_x(*next_x);
  *next_x += bounds.size().width();
  display::Display display(next_display_id_++, bounds);
  display.set_device_scale_factor(display_info.device_scale_factor());
  gfx::Rect work_area(bounds.size());
  // Offset the height slightly to give a different work area. -20 is arbitrary,
  // it could be anything.
  work_area.set_height(std::max(0, work_area.height() - 20));
  display.set_work_area(work_area);
  window_tree_client_private_->CallWmNewDisplayAdded(display);
  return GetRootsOrderedByDisplayId().back();
}

void AshTestHelper::UpdateDisplay(RootWindowController* root_window_controller,
                                  const std::string& display_spec,
                                  int* next_x) {
  ManagedDisplayInfo display_info =
      ManagedDisplayInfo::CreateFromSpec(display_spec);
  gfx::Rect bounds = display_info.bounds_in_native();
  bounds.set_x(*next_x);
  *next_x += bounds.size().width();
  display::Display updated_display =
      root_window_controller->GetWindow()->GetDisplayNearestWindow();
  gfx::Insets work_area_insets = updated_display.GetWorkAreaInsets();
  updated_display.set_bounds(bounds);
  updated_display.UpdateWorkAreaFromInsets(work_area_insets);
  updated_display.set_device_scale_factor(display_info.device_scale_factor());
  root_window_controller->GetWindow()->SetBounds(gfx::Rect(bounds.size()));
  ScreenMus* screen = window_manager_app_->window_manager()->screen_.get();
  const bool is_primary =
      screen->display_list().FindDisplayById(updated_display.id()) ==
      screen->display_list().GetPrimaryDisplayIterator();
  screen->display_list().UpdateDisplay(
      updated_display, is_primary ? display::DisplayList::Type::PRIMARY
                                  : display::DisplayList::Type::NOT_PRIMARY);
}

std::vector<RootWindowController*> AshTestHelper::GetRootsOrderedByDisplayId() {
  std::set<RootWindowController*> roots =
      window_manager_app_->window_manager()->GetRootWindowControllers();
  std::vector<RootWindowController*> ordered_roots;
  ordered_roots.insert(ordered_roots.begin(), roots.begin(), roots.end());
  std::sort(ordered_roots.begin(), ordered_roots.end(), &CompareByDisplayId);
  return ordered_roots;
}

}  // namespace test
}  // namespace ash
