// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/shell_delegate_impl.h"

#include <limits>

#include "ash/caps_lock_delegate_stub.h"
#include "ash/host/root_window_host_factory.h"
#include "ash/session_state_delegate.h"
#include "ash/session_state_delegate_stub.h"
#include "ash/shell/context_menu.h"
#include "ash/shell/example_factory.h"
#include "ash/shell/launcher_delegate_impl.h"
#include "ash/shell/toplevel_window.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_util.h"
#include "base/message_loop.h"
#include "ui/aura/window.h"
#include "ui/keyboard/keyboard_controller_proxy.h"
#include "ui/views/corewm/input_method_event_filter.h"

namespace ash {

namespace {

class DummyKeyboardControllerProxy : public keyboard::KeyboardControllerProxy {
 public:
  DummyKeyboardControllerProxy() {}
  virtual ~DummyKeyboardControllerProxy() {}

 private:
  // Overridden from keyboard::KeyboardControllerProxy:
  virtual content::BrowserContext* GetBrowserContext() OVERRIDE {
    return Shell::GetInstance()->browser_context();
  }

  virtual ui::InputMethod* GetInputMethod() OVERRIDE {
    return Shell::GetInstance()->input_method_filter()->input_method();
  }

  virtual void OnKeyboardBoundsChanged(const gfx::Rect& new_bounds) OVERRIDE {
  }

  DISALLOW_COPY_AND_ASSIGN(DummyKeyboardControllerProxy);
};

}  // namespace

namespace shell {

ShellDelegateImpl::ShellDelegateImpl()
    : watcher_(NULL),
      launcher_delegate_(NULL),
      spoken_feedback_enabled_(false),
      high_contrast_enabled_(false),
      screen_magnifier_enabled_(false),
      screen_magnifier_type_(kDefaultMagnifierType) {
}

ShellDelegateImpl::~ShellDelegateImpl() {
}

void ShellDelegateImpl::SetWatcher(WindowWatcher* watcher) {
  watcher_ = watcher;
  if (launcher_delegate_)
    launcher_delegate_->set_watcher(watcher);
}

bool ShellDelegateImpl::IsFirstRunAfterBoot() const {
  return false;
}

bool ShellDelegateImpl::IsMultiProfilesEnabled() const {
  return false;
}

bool ShellDelegateImpl::IsRunningInForcedAppMode() const {
  return false;
}

void ShellDelegateImpl::PreInit() {
}

void ShellDelegateImpl::Shutdown() {
}

void ShellDelegateImpl::Exit() {
  base::MessageLoopForUI::current()->Quit();
}

void ShellDelegateImpl::NewTab() {
}

void ShellDelegateImpl::NewWindow(bool incognito) {
  ash::shell::ToplevelWindow::CreateParams create_params;
  create_params.can_resize = true;
  create_params.can_maximize = true;
  ash::shell::ToplevelWindow::CreateToplevelWindow(create_params);
}

void ShellDelegateImpl::ToggleFullscreen() {
  ToggleMaximized();
}

void ShellDelegateImpl::ToggleMaximized() {
  aura::Window* window = ash::wm::GetActiveWindow();
  if (window)
    ash::wm::ToggleMaximizedWindow(window);
}

void ShellDelegateImpl::OpenFileManager(bool as_dialog) {
}

void ShellDelegateImpl::OpenCrosh() {
}

void ShellDelegateImpl::OpenMobileSetup(const std::string& service_path) {
}

void ShellDelegateImpl::RestoreTab() {
}

void ShellDelegateImpl::ShowKeyboardOverlay() {
}

keyboard::KeyboardControllerProxy*
    ShellDelegateImpl::CreateKeyboardControllerProxy() {
  return new DummyKeyboardControllerProxy();
}

void ShellDelegateImpl::ShowTaskManager() {
}

content::BrowserContext* ShellDelegateImpl::GetCurrentBrowserContext() {
  return Shell::GetInstance()->browser_context();
}

void ShellDelegateImpl::ToggleSpokenFeedback(
    AccessibilityNotificationVisibility notify) {
  spoken_feedback_enabled_ = !spoken_feedback_enabled_;
}

bool ShellDelegateImpl::IsSpokenFeedbackEnabled() const {
  return spoken_feedback_enabled_;
}

void ShellDelegateImpl::ToggleHighContrast() {
  high_contrast_enabled_ = !high_contrast_enabled_;
}

bool ShellDelegateImpl::IsHighContrastEnabled() const {
  return high_contrast_enabled_;
}

void ShellDelegateImpl::SetMagnifierEnabled(bool enabled) {
  screen_magnifier_enabled_ = enabled;
}

void ShellDelegateImpl::SetMagnifierType(MagnifierType type) {
  screen_magnifier_type_ = type;
}

bool ShellDelegateImpl::IsMagnifierEnabled() const {
  return screen_magnifier_enabled_;
}

MagnifierType ShellDelegateImpl::GetMagnifierType() const {
  return screen_magnifier_type_;
}

bool ShellDelegateImpl::ShouldAlwaysShowAccessibilityMenu() const {
  return false;
}

void ShellDelegateImpl::SilenceSpokenFeedback() const {
}

app_list::AppListViewDelegate* ShellDelegateImpl::CreateAppListViewDelegate() {
  return ash::shell::CreateAppListViewDelegate();
}

ash::LauncherDelegate* ShellDelegateImpl::CreateLauncherDelegate(
    ash::LauncherModel* model) {
  launcher_delegate_ = new LauncherDelegateImpl(watcher_);
  return launcher_delegate_;
}

ash::SystemTrayDelegate* ShellDelegateImpl::CreateSystemTrayDelegate() {
  return NULL;
}

ash::UserWallpaperDelegate* ShellDelegateImpl::CreateUserWallpaperDelegate() {
  return NULL;
}

ash::CapsLockDelegate* ShellDelegateImpl::CreateCapsLockDelegate() {
  return new CapsLockDelegateStub;
}

ash::SessionStateDelegate* ShellDelegateImpl::CreateSessionStateDelegate() {
  return new SessionStateDelegateStub;
}

aura::client::UserActionClient* ShellDelegateImpl::CreateUserActionClient() {
  return NULL;
}

void ShellDelegateImpl::OpenFeedbackPage() {
}

void ShellDelegateImpl::RecordUserMetricsAction(UserMetricsAction action) {
}

void ShellDelegateImpl::HandleMediaNextTrack() {
}

void ShellDelegateImpl::HandleMediaPlayPause() {
}

void ShellDelegateImpl::HandleMediaPrevTrack() {
}

base::string16 ShellDelegateImpl::GetTimeRemainingString(
    base::TimeDelta delta) {
  return base::string16();
}

base::string16 ShellDelegateImpl::GetTimeDurationLongString(
    base::TimeDelta delta) {
  return base::string16();
}

void ShellDelegateImpl::SaveScreenMagnifierScale(double scale) {
}

double ShellDelegateImpl::GetSavedScreenMagnifierScale() {
  return std::numeric_limits<double>::min();
}

ui::MenuModel* ShellDelegateImpl::CreateContextMenu(aura::RootWindow* root) {
  return new ContextMenu(root);
}

RootWindowHostFactory* ShellDelegateImpl::CreateRootWindowHostFactory() {
  return RootWindowHostFactory::Create();
}

base::string16 ShellDelegateImpl::GetProductName() const {
  return base::string16();
}

}  // namespace shell
}  // namespace ash
