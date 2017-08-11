// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_desktop_controller_aura.h"

#include <string>

#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "extensions/shell/browser/shell_app_window_client.h"
#include "extensions/shell/browser/shell_screen.h"
#include "extensions/shell/common/switches.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/image_cursors.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_factory.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/wm/core/base_focus_rules.h"
#include "ui/wm/core/compound_event_filter.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/focus_controller.h"
#include "ui/wm/core/native_cursor_manager.h"
#include "ui/wm/core/native_cursor_manager_delegate.h"

#if defined(OS_CHROMEOS)

#include "chromeos/dbus/dbus_thread_manager.h"
#include "ui/chromeos/user_activity_power_manager_notifier.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/ozone/public/ozone_platform.h"

#endif  // defined(OS_CHROMEOS)

namespace extensions {
namespace {

// A class that bridges the gap between CursorManager and Aura. It borrows
// heavily from NativeCursorManagerAsh.
class ShellNativeCursorManager : public wm::NativeCursorManager {
 public:
  explicit ShellNativeCursorManager(
      ShellDesktopControllerAura* desktop_controller)
      : desktop_controller_(desktop_controller),
        image_cursors_(new ui::ImageCursors) {}
  ~ShellNativeCursorManager() override {}

  // wm::NativeCursorManager overrides.
  void SetDisplay(const display::Display& display,
                  wm::NativeCursorManagerDelegate* delegate) override {
    if (image_cursors_->SetDisplay(display, display.device_scale_factor()))
      SetCursor(delegate->GetCursor(), delegate);
  }

  void SetCursor(gfx::NativeCursor cursor,
                 wm::NativeCursorManagerDelegate* delegate) override {
    image_cursors_->SetPlatformCursor(&cursor);
    cursor.set_device_scale_factor(image_cursors_->GetScale());
    delegate->CommitCursor(cursor);

    if (delegate->IsCursorVisible())
      SetCursorOnAllRootWindows(cursor);
  }

  void SetVisibility(bool visible,
                     wm::NativeCursorManagerDelegate* delegate) override {
    delegate->CommitVisibility(visible);

    if (visible) {
      SetCursor(delegate->GetCursor(), delegate);
    } else {
      gfx::NativeCursor invisible_cursor(ui::CursorType::kNone);
      image_cursors_->SetPlatformCursor(&invisible_cursor);
      SetCursorOnAllRootWindows(invisible_cursor);
    }
  }

  void SetCursorSize(ui::CursorSize cursor_size,
                     wm::NativeCursorManagerDelegate* delegate) override {
    image_cursors_->SetCursorSize(cursor_size);
    delegate->CommitCursorSize(cursor_size);
    if (delegate->IsCursorVisible())
      SetCursor(delegate->GetCursor(), delegate);
  }

  void SetMouseEventsEnabled(
      bool enabled,
      wm::NativeCursorManagerDelegate* delegate) override {
    delegate->CommitMouseEventsEnabled(enabled);
    SetVisibility(delegate->IsCursorVisible(), delegate);
  }

 private:
  // Sets |cursor| as the active cursor within Aura.
  void SetCursorOnAllRootWindows(gfx::NativeCursor cursor) {
    if (desktop_controller_->GetPrimaryHost())
      desktop_controller_->GetPrimaryHost()->SetCursor(cursor);
  }

  ShellDesktopControllerAura* desktop_controller_;  // Not owned.

  std::unique_ptr<ui::ImageCursors> image_cursors_;

  DISALLOW_COPY_AND_ASSIGN(ShellNativeCursorManager);
};

class AppsFocusRules : public wm::BaseFocusRules {
 public:
  AppsFocusRules() {}
  ~AppsFocusRules() override {}

  bool SupportsChildActivation(aura::Window* window) const override {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AppsFocusRules);
};

}  // namespace

ShellDesktopControllerAura::ShellDesktopControllerAura(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context),
      app_window_client_(new ShellAppWindowClient) {
  extensions::AppWindowClient::Set(app_window_client_.get());

#if defined(OS_CHROMEOS)
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(
      this);
  display_configurator_.reset(new display::DisplayConfigurator);
  display_configurator_->Init(
      ui::OzonePlatform::GetInstance()->CreateNativeDisplayDelegate(), false);
  display_configurator_->ForceInitialConfigure();
  display_configurator_->AddObserver(this);
#endif

  InitWindowManager();
}

ShellDesktopControllerAura::~ShellDesktopControllerAura() {
  TearDownWindowManager();
#if defined(OS_CHROMEOS)
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(
      this);
#endif
  extensions::AppWindowClient::Set(NULL);
}

void ShellDesktopControllerAura::Run() {
  base::RunLoop run_loop;
  run_loop_ = &run_loop;
  run_loop.Run();
  run_loop_ = nullptr;
}

AppWindow* ShellDesktopControllerAura::CreateAppWindow(
    content::BrowserContext* context,
    const Extension* extension) {
  return root_window_controller_->CreateAppWindow(context, extension);
}

void ShellDesktopControllerAura::AddAppWindow(gfx::NativeWindow window) {
  root_window_controller_->AddAppWindow(window);
}

void ShellDesktopControllerAura::CloseAppWindows() {
  root_window_controller_->CloseAppWindows();
}

void ShellDesktopControllerAura::CloseRootWindowController(
    RootWindowController* root_window_controller) {
  DCHECK_EQ(root_window_controller_.get(), root_window_controller);

  // Just quit. The RootWindowController must stay alive until we begin
  // TearDownWindowManager().
  // run_loop_ may be null in tests.
  if (run_loop_)
    run_loop_->QuitWhenIdle();
}

#if defined(OS_CHROMEOS)
void ShellDesktopControllerAura::PowerButtonEventReceived(
    bool down,
    const base::TimeTicks& timestamp) {
  if (down) {
    chromeos::DBusThreadManager::Get()
        ->GetPowerManagerClient()
        ->RequestShutdown();
  }
}

void ShellDesktopControllerAura::OnDisplayModeChanged(
    const display::DisplayConfigurator::DisplayStateList& displays) {
  gfx::Size size = GetPrimaryDisplaySize();
  if (!size.IsEmpty())
    root_window_controller_->UpdateSize(size);
}
#endif

ui::EventDispatchDetails ShellDesktopControllerAura::DispatchKeyEventPostIME(
    ui::KeyEvent* key_event) {
  DCHECK(root_window_controller_);

  // TODO(michaelpg): With multiple windows, determine which root window
  // triggered the event. See ash::WindowTreeHostManager for example.
  return GetPrimaryHost()->DispatchKeyEventPostIME(key_event);
}

aura::WindowTreeHost* ShellDesktopControllerAura::GetPrimaryHost() {
  if (!root_window_controller_)
    return nullptr;
  return root_window_controller_->host();
}

void ShellDesktopControllerAura::InitWindowManager() {
  root_window_event_filter_ = base::MakeUnique<wm::CompoundEventFilter>();

  // Set up basic pieces of ui::wm.
  screen_ = base::MakeUnique<ShellScreen>(this, GetStartingWindowSize());
  display::Screen::SetScreenInstance(screen_.get());

  focus_controller_ =
      base::MakeUnique<wm::FocusController>(new AppsFocusRules());

  cursor_manager_ = base::MakeUnique<wm::CursorManager>(
      base::MakeUnique<ShellNativeCursorManager>(this));

  user_activity_detector_ = base::MakeUnique<ui::UserActivityDetector>();
#if defined(OS_CHROMEOS)
  user_activity_notifier_ =
      base::MakeUnique<ui::UserActivityPowerManagerNotifier>(
          user_activity_detector_.get());
#endif

  // Create the root window, then set it up as our primary window.
  CreateRootWindowController();
  FinalizeWindowManager();
}

void ShellDesktopControllerAura::CreateRootWindowController() {
  // TODO(michaelpg): Support creating a host for a secondary display.
  root_window_controller_ = base::MakeUnique<RootWindowController>(
      this, screen_.get(),
      gfx::Rect(screen_->GetPrimaryDisplay().GetSizeInPixel()),
      browser_context_);

  // Initialize the root window with our clients.
  aura::Window* root_window = root_window_controller_->host()->window();
  root_window->AddPreTargetHandler(root_window_event_filter_.get());
  aura::client::SetFocusClient(root_window, focus_controller_.get());
  root_window->AddPreTargetHandler(focus_controller_.get());
  wm::SetActivationClient(root_window, focus_controller_.get());
  aura::client::SetCursorClient(root_window, cursor_manager_.get());
}

void ShellDesktopControllerAura::FinalizeWindowManager() {
  // Create an input method and become its delegate.
  input_method_ =
      ui::CreateInputMethod(this, GetPrimaryHost()->GetAcceleratedWidget());

  cursor_manager_->SetDisplay(screen_->GetPrimaryDisplay());
  cursor_manager_->SetCursor(ui::CursorType::kPointer);

  // TODO(michaelpg): Replace with a capture client supporting multiple
  // WindowTreeHosts.
  capture_client_ = base::MakeUnique<aura::client::DefaultCaptureClient>(
      GetPrimaryHost()->window());
}

void ShellDesktopControllerAura::TearDownWindowManager() {
  GetPrimaryHost()->window()->RemovePreTargetHandler(
      root_window_event_filter_.get());
  root_window_event_filter_.reset();

  // The DefaultCaptureClient must be destroyed before the root window.
  capture_client_.reset();

  GetPrimaryHost()->window()->RemovePreTargetHandler(focus_controller_.get());
  wm::SetActivationClient(GetPrimaryHost()->window(), nullptr);
  root_window_controller_.reset();

  focus_controller_.reset();
  cursor_manager_.reset();
#if defined(OS_CHROMEOS)
  user_activity_notifier_.reset();
#endif
  user_activity_detector_.reset();
  display::Screen::SetScreenInstance(nullptr);
  screen_.reset();
}

gfx::Size ShellDesktopControllerAura::GetStartingWindowSize() {
  gfx::Size size;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kAppShellHostWindowSize)) {
    const std::string size_str =
        command_line->GetSwitchValueASCII(switches::kAppShellHostWindowSize);
    int width, height;
    CHECK_EQ(2, sscanf(size_str.c_str(), "%dx%d", &width, &height));
    size = gfx::Size(width, height);
  } else {
    size = GetPrimaryDisplaySize();
  }
  if (size.IsEmpty())
    size = gfx::Size(1920, 1080);
  return size;
}

gfx::Size ShellDesktopControllerAura::GetPrimaryDisplaySize() {
#if defined(OS_CHROMEOS)
  const display::DisplayConfigurator::DisplayStateList& displays =
      display_configurator_->cached_displays();
  if (displays.empty())
    return gfx::Size();
  const display::DisplayMode* mode = displays[0]->current_mode();
  return mode ? mode->size() : gfx::Size();
#else
  return gfx::Size();
#endif
}

}  // namespace extensions
