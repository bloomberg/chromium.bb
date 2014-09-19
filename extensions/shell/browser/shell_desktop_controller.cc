// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_desktop_controller.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/shell/browser/shell_app_delegate.h"
#include "extensions/shell/browser/shell_app_window_client.h"
#include "extensions/shell/common/switches.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/test/test_screen.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/cursor/image_cursors.h"
#include "ui/base/ime/input_method_initializer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/screen.h"
#include "ui/wm/core/base_focus_rules.h"
#include "ui/wm/core/compound_event_filter.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/focus_controller.h"
#include "ui/wm/core/input_method_event_filter.h"
#include "ui/wm/core/native_cursor_manager.h"
#include "ui/wm/core/native_cursor_manager_delegate.h"
#include "ui/wm/core/user_activity_detector.h"

#if defined(OS_CHROMEOS)
#include "chromeos/dbus/dbus_thread_manager.h"
#include "ui/chromeos/user_activity_power_manager_notifier.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#endif

namespace extensions {
namespace {

// A simple layout manager that makes each new window fill its parent.
class FillLayout : public aura::LayoutManager {
 public:
  FillLayout() {}
  virtual ~FillLayout() {}

 private:
  // aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE {}

  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE {
    if (!child->parent())
      return;

    // Create a rect at 0,0 with the size of the parent.
    gfx::Size parent_size = child->parent()->bounds().size();
    child->SetBounds(gfx::Rect(parent_size));
  }

  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE {}

  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE {}

  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visible) OVERRIDE {}

  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE {
    SetChildBoundsDirect(child, requested_bounds);
  }

  DISALLOW_COPY_AND_ASSIGN(FillLayout);
};

// A class that bridges the gap between CursorManager and Aura. It borrows
// heavily from AshNativeCursorManager.
class ShellNativeCursorManager : public wm::NativeCursorManager {
 public:
  explicit ShellNativeCursorManager(aura::WindowTreeHost* host)
      : host_(host), image_cursors_(new ui::ImageCursors) {}
  virtual ~ShellNativeCursorManager() {}

  // wm::NativeCursorManager overrides.
  virtual void SetDisplay(const gfx::Display& display,
                          wm::NativeCursorManagerDelegate* delegate) OVERRIDE {
    if (image_cursors_->SetDisplay(display, display.device_scale_factor()))
      SetCursor(delegate->GetCursor(), delegate);
  }

  virtual void SetCursor(gfx::NativeCursor cursor,
                         wm::NativeCursorManagerDelegate* delegate) OVERRIDE {
    image_cursors_->SetPlatformCursor(&cursor);
    cursor.set_device_scale_factor(image_cursors_->GetScale());
    delegate->CommitCursor(cursor);

    if (delegate->IsCursorVisible())
      ApplyCursor(cursor);
  }

  virtual void SetVisibility(
      bool visible,
      wm::NativeCursorManagerDelegate* delegate) OVERRIDE {
    delegate->CommitVisibility(visible);

    if (visible) {
      SetCursor(delegate->GetCursor(), delegate);
    } else {
      gfx::NativeCursor invisible_cursor(ui::kCursorNone);
      image_cursors_->SetPlatformCursor(&invisible_cursor);
      ApplyCursor(invisible_cursor);
    }
  }

  virtual void SetCursorSet(
      ui::CursorSetType cursor_set,
      wm::NativeCursorManagerDelegate* delegate) OVERRIDE {
    image_cursors_->SetCursorSet(cursor_set);
    delegate->CommitCursorSet(cursor_set);
    if (delegate->IsCursorVisible())
      SetCursor(delegate->GetCursor(), delegate);
  }

  virtual void SetMouseEventsEnabled(
      bool enabled,
      wm::NativeCursorManagerDelegate* delegate) OVERRIDE {
    delegate->CommitMouseEventsEnabled(enabled);
    SetVisibility(delegate->IsCursorVisible(), delegate);
  }

 private:
  // Sets |cursor| as the active cursor within Aura.
  void ApplyCursor(gfx::NativeCursor cursor) { host_->SetCursor(cursor); }

  aura::WindowTreeHost* host_;  // Not owned.

  scoped_ptr<ui::ImageCursors> image_cursors_;

  DISALLOW_COPY_AND_ASSIGN(ShellNativeCursorManager);
};

class AppsFocusRules : public wm::BaseFocusRules {
 public:
  AppsFocusRules() {}
  virtual ~AppsFocusRules() {}

  virtual bool SupportsChildActivation(aura::Window* window) const OVERRIDE {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AppsFocusRules);
};

}  // namespace

ShellDesktopController::ShellDesktopController()
    : app_window_client_(new ShellAppWindowClient), app_window_(NULL) {
  extensions::AppWindowClient::Set(app_window_client_.get());

#if defined(OS_CHROMEOS)
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      AddObserver(this);
  display_configurator_.reset(new ui::DisplayConfigurator);
  display_configurator_->Init(false);
  display_configurator_->ForceInitialConfigure(0);
  display_configurator_->AddObserver(this);
#endif
  CreateRootWindow();
}

ShellDesktopController::~ShellDesktopController() {
  CloseAppWindows();
  DestroyRootWindow();
#if defined(OS_CHROMEOS)
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      RemoveObserver(this);
#endif
  extensions::AppWindowClient::Set(NULL);
}

aura::WindowTreeHost* ShellDesktopController::GetHost() {
  return host_.get();
}

AppWindow* ShellDesktopController::CreateAppWindow(
    content::BrowserContext* context,
    const Extension* extension) {
  app_window_ = new AppWindow(context, new ShellAppDelegate, extension);
  return app_window_;
}

void ShellDesktopController::AddAppWindow(aura::Window* window) {
  aura::Window* root_window = GetHost()->window();
  root_window->AddChild(window);
}

void ShellDesktopController::CloseAppWindows() {
  if (app_window_) {
    app_window_->GetBaseWindow()->Close();  // Close() deletes |app_window_|.
    app_window_ = NULL;
  }
}

aura::Window* ShellDesktopController::GetDefaultParent(
    aura::Window* context,
    aura::Window* window,
    const gfx::Rect& bounds) {
  return host_->window();
}

#if defined(OS_CHROMEOS)
void ShellDesktopController::PowerButtonEventReceived(
    bool down,
    const base::TimeTicks& timestamp) {
  if (down) {
    chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
        RequestShutdown();
  }
}

void ShellDesktopController::OnDisplayModeChanged(
    const std::vector<ui::DisplayConfigurator::DisplayState>& displays) {
  gfx::Size size = GetPrimaryDisplaySize();
  if (!size.IsEmpty())
    host_->UpdateRootWindowSize(size);
}
#endif

void ShellDesktopController::OnHostCloseRequested(
    const aura::WindowTreeHost* host) {
  DCHECK_EQ(host_.get(), host);
  CloseAppWindows();
  base::MessageLoop::current()->PostTask(FROM_HERE,
                                         base::MessageLoop::QuitClosure());
}

void ShellDesktopController::InitWindowManager() {
  wm::FocusController* focus_controller =
      new wm::FocusController(new AppsFocusRules());
  aura::client::SetFocusClient(host_->window(), focus_controller);
  host_->window()->AddPreTargetHandler(focus_controller);
  aura::client::SetActivationClient(host_->window(), focus_controller);
  focus_client_.reset(focus_controller);

  input_method_filter_.reset(
      new wm::InputMethodEventFilter(host_->GetAcceleratedWidget()));
  input_method_filter_->SetInputMethodPropertyInRootWindow(host_->window());
  root_window_event_filter_->AddHandler(input_method_filter_.get());

  capture_client_.reset(
      new aura::client::DefaultCaptureClient(host_->window()));

  // Ensure new windows fill the display.
  host_->window()->SetLayoutManager(new FillLayout);

  cursor_manager_.reset(
      new wm::CursorManager(scoped_ptr<wm::NativeCursorManager>(
          new ShellNativeCursorManager(host_.get()))));
  cursor_manager_->SetDisplay(
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay());
  cursor_manager_->SetCursor(ui::kCursorPointer);
  aura::client::SetCursorClient(host_->window(), cursor_manager_.get());

  user_activity_detector_.reset(new wm::UserActivityDetector);
  host_->event_processor()->GetRootTarget()->AddPreTargetHandler(
      user_activity_detector_.get());
#if defined(OS_CHROMEOS)
  user_activity_notifier_.reset(
      new ui::UserActivityPowerManagerNotifier(user_activity_detector_.get()));
#endif
}

void ShellDesktopController::CreateRootWindow() {
  // Set up basic pieces of ui::wm.
  gfx::Size size;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kAppShellHostWindowBounds)) {
    const std::string size_str =
        command_line->GetSwitchValueASCII(switches::kAppShellHostWindowBounds);
    int width, height;
    CHECK_EQ(2, sscanf(size_str.c_str(), "%dx%d", &width, &height));
    size = gfx::Size(width, height);
  } else {
    size = GetPrimaryDisplaySize();
  }
  if (size.IsEmpty())
    size = gfx::Size(1280, 720);

  test_screen_.reset(aura::TestScreen::Create(size));
  // TODO(jamescook): Replace this with a real Screen implementation.
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, test_screen_.get());
  // TODO(mukai): Set up input method.

  host_.reset(test_screen_->CreateHostForPrimaryDisplay());
  host_->InitHost();
  aura::client::SetWindowTreeClient(host_->window(), this);
  root_window_event_filter_.reset(new wm::CompoundEventFilter);
  host_->window()->AddPreTargetHandler(root_window_event_filter_.get());
  InitWindowManager();

  host_->AddObserver(this);

  // Ensure the X window gets mapped.
  host_->Show();
}

void ShellDesktopController::DestroyRootWindow() {
  host_->RemoveObserver(this);
  if (input_method_filter_)
    root_window_event_filter_->RemoveHandler(input_method_filter_.get());
  if (user_activity_detector_) {
    host_->event_processor()->GetRootTarget()->RemovePreTargetHandler(
        user_activity_detector_.get());
  }
  wm::FocusController* focus_controller =
      static_cast<wm::FocusController*>(focus_client_.get());
  if (focus_controller) {
    host_->window()->RemovePreTargetHandler(focus_controller);
    aura::client::SetActivationClient(host_->window(), NULL);
  }
  root_window_event_filter_.reset();
  capture_client_.reset();
  input_method_filter_.reset();
  focus_client_.reset();
  cursor_manager_.reset();
#if defined(OS_CHROMEOS)
  user_activity_notifier_.reset();
#endif
  user_activity_detector_.reset();
  host_.reset();
}

gfx::Size ShellDesktopController::GetPrimaryDisplaySize() {
#if defined(OS_CHROMEOS)
  const std::vector<ui::DisplayConfigurator::DisplayState>& displays =
      display_configurator_->cached_displays();
  if (displays.empty())
    return gfx::Size();
  const ui::DisplayMode* mode = displays[0].display->current_mode();
  return mode ? mode->size() : gfx::Size();
#else
  return gfx::Size();
#endif
}

}  // namespace extensions
