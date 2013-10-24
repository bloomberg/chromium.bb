// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_shell_delegate.h"

#include <limits>

#include "ash/caps_lock_delegate_stub.h"
#include "ash/default_accessibility_delegate.h"
#include "ash/host/root_window_host_factory.h"
#include "ash/keyboard_controller_proxy_stub.h"
#include "ash/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/test_launcher_delegate.h"
#include "ash/test/test_session_state_delegate.h"
#include "ash/test/test_system_tray_delegate.h"
#include "ash/test/test_user_wallpaper_delegate.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/logging.h"
#include "content/public/test/test_browser_context.h"
#include "ui/aura/window.h"
#include "ui/compositor/compositor.h"

namespace ash {
namespace test {

TestShellDelegate::TestShellDelegate()
    : num_exit_requests_(0),
      multi_profiles_enabled_(false),
      test_session_state_delegate_(NULL) {
}

TestShellDelegate::~TestShellDelegate() {
}

bool TestShellDelegate::IsFirstRunAfterBoot() const {
  return false;
}

bool TestShellDelegate::IsMultiProfilesEnabled() const {
  return multi_profiles_enabled_;
}

bool TestShellDelegate::IsRunningInForcedAppMode() const {
  return false;
}

void TestShellDelegate::PreInit() {
}

void TestShellDelegate::Shutdown() {
}

void TestShellDelegate::Exit() {
  num_exit_requests_++;
}

void TestShellDelegate::NewTab() {
}

void TestShellDelegate::NewWindow(bool incognito) {
}

void TestShellDelegate::ToggleFullscreen() {
}

void TestShellDelegate::OpenFileManager() {
}

void TestShellDelegate::OpenCrosh() {
}

void TestShellDelegate::RestoreTab() {
}

void TestShellDelegate::ShowKeyboardOverlay() {
}

keyboard::KeyboardControllerProxy*
    TestShellDelegate::CreateKeyboardControllerProxy() {
  return new KeyboardControllerProxyStub();
}

void TestShellDelegate::ShowTaskManager() {
}

content::BrowserContext* TestShellDelegate::GetCurrentBrowserContext() {
  current_browser_context_.reset(new content::TestBrowserContext());
  return current_browser_context_.get();
}

app_list::AppListViewDelegate* TestShellDelegate::CreateAppListViewDelegate() {
  return NULL;
}

LauncherDelegate* TestShellDelegate::CreateLauncherDelegate(
    ash::LauncherModel* model) {
  return new TestLauncherDelegate(model);
}

SystemTrayDelegate* TestShellDelegate::CreateSystemTrayDelegate() {
  return new TestSystemTrayDelegate;
}

UserWallpaperDelegate* TestShellDelegate::CreateUserWallpaperDelegate() {
  return new TestUserWallpaperDelegate();
}

CapsLockDelegate* TestShellDelegate::CreateCapsLockDelegate() {
  return new CapsLockDelegateStub;
}

SessionStateDelegate* TestShellDelegate::CreateSessionStateDelegate() {
  DCHECK(!test_session_state_delegate_);
  test_session_state_delegate_ = new TestSessionStateDelegate();
  return test_session_state_delegate_;
}

AccessibilityDelegate* TestShellDelegate::CreateAccessibilityDelegate() {
  return new internal::DefaultAccessibilityDelegate();
}

aura::client::UserActionClient* TestShellDelegate::CreateUserActionClient() {
  return NULL;
}

void TestShellDelegate::OpenFeedbackPage() {
}

void TestShellDelegate::RecordUserMetricsAction(UserMetricsAction action) {
}

void TestShellDelegate::HandleMediaNextTrack() {
}

void TestShellDelegate::HandleMediaPlayPause() {
}

void TestShellDelegate::HandleMediaPrevTrack() {
}

ui::MenuModel* TestShellDelegate::CreateContextMenu(aura::RootWindow* root) {
  return NULL;
}

RootWindowHostFactory* TestShellDelegate::CreateRootWindowHostFactory() {
  // The ContextFactory must exist before any Compositors are created.
  bool allow_test_contexts = true;
  ui::Compositor::InitializeContextFactoryForTests(allow_test_contexts);

  return RootWindowHostFactory::Create();
}

base::string16 TestShellDelegate::GetProductName() const {
  return base::string16();
}

TestSessionStateDelegate* TestShellDelegate::test_session_state_delegate() {
  return test_session_state_delegate_;
}

}  // namespace test
}  // namespace ash
