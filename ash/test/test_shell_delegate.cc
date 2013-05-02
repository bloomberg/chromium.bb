// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_shell_delegate.h"

#include <limits>

#include "ash/caps_lock_delegate_stub.h"
#include "ash/host/root_window_host_factory.h"
#include "ash/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/test_launcher_delegate.h"
#include "ash/test/test_session_state_delegate.h"
#include "ash/wm/window_util.h"
#include "base/logging.h"
#include "content/public/test/test_browser_context.h"
#include "ui/aura/window.h"

namespace ash {
namespace test {

TestShellDelegate::TestShellDelegate()
    : spoken_feedback_enabled_(false),
      high_contrast_enabled_(false),
      screen_magnifier_enabled_(false),
      screen_magnifier_type_(kDefaultMagnifierType),
      num_exit_requests_(0),
      test_session_state_delegate_(NULL) {
}

TestShellDelegate::~TestShellDelegate() {
}

bool TestShellDelegate::IsFirstRunAfterBoot() const {
  return false;
}

bool TestShellDelegate::IsMultiProfilesEnabled() const {
  return false;
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

void TestShellDelegate::ToggleMaximized() {
  aura::Window* window = ash::wm::GetActiveWindow();
  if (window)
    ash::wm::ToggleMaximizedWindow(window);
}

void TestShellDelegate::ToggleFullscreen() {
}

void TestShellDelegate::OpenFileManager(bool as_dialog) {
}

void TestShellDelegate::OpenCrosh() {
}

void TestShellDelegate::OpenMobileSetup(const std::string& service_path) {
}

void TestShellDelegate::RestoreTab() {
}

void TestShellDelegate::ShowKeyboardOverlay() {
}

keyboard::KeyboardControllerProxy*
    TestShellDelegate::CreateKeyboardControllerProxy() {
  return NULL;
}

void TestShellDelegate::ShowTaskManager() {
}

content::BrowserContext* TestShellDelegate::GetCurrentBrowserContext() {
  current_browser_context_.reset(new content::TestBrowserContext());
  return current_browser_context_.get();
}

void TestShellDelegate::ToggleSpokenFeedback(
    AccessibilityNotificationVisibility notify) {
  spoken_feedback_enabled_ = !spoken_feedback_enabled_;
}

bool TestShellDelegate::IsSpokenFeedbackEnabled() const {
  return spoken_feedback_enabled_;
}

void TestShellDelegate::ToggleHighContrast() {
  high_contrast_enabled_ = !high_contrast_enabled_;
}

bool TestShellDelegate::IsHighContrastEnabled() const {
  return high_contrast_enabled_;
}

void TestShellDelegate::SetMagnifierEnabled(bool enabled) {
  screen_magnifier_enabled_ = enabled;
}

void TestShellDelegate::SetMagnifierType(MagnifierType type) {
  screen_magnifier_type_ = type;
}

bool TestShellDelegate::IsMagnifierEnabled() const {
  return screen_magnifier_enabled_;
}

MagnifierType TestShellDelegate::GetMagnifierType() const {
  return screen_magnifier_type_;
}

bool TestShellDelegate::ShouldAlwaysShowAccessibilityMenu() const {
  return false;
}

void TestShellDelegate::SilenceSpokenFeedback() const {
}

app_list::AppListViewDelegate* TestShellDelegate::CreateAppListViewDelegate() {
  return NULL;
}

LauncherDelegate* TestShellDelegate::CreateLauncherDelegate(
    ash::LauncherModel* model) {
  return new TestLauncherDelegate(model);
}

SystemTrayDelegate* TestShellDelegate::CreateSystemTrayDelegate() {
  return NULL;
}

UserWallpaperDelegate* TestShellDelegate::CreateUserWallpaperDelegate() {
  return NULL;
}

CapsLockDelegate* TestShellDelegate::CreateCapsLockDelegate() {
  return new CapsLockDelegateStub;
}

SessionStateDelegate* TestShellDelegate::CreateSessionStateDelegate() {
  DCHECK(!test_session_state_delegate_);
  test_session_state_delegate_ = new TestSessionStateDelegate();
  return test_session_state_delegate_;
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

base::string16 TestShellDelegate::GetTimeRemainingString(
    base::TimeDelta delta) {
  return base::string16();
}

base::string16 TestShellDelegate::GetTimeDurationLongString(
    base::TimeDelta delta) {
  return base::string16();
}

void TestShellDelegate::SaveScreenMagnifierScale(double scale) {
}

ui::MenuModel* TestShellDelegate::CreateContextMenu(aura::RootWindow* root) {
  return NULL;
}

double TestShellDelegate::GetSavedScreenMagnifierScale() {
  return std::numeric_limits<double>::min();
}

RootWindowHostFactory* TestShellDelegate::CreateRootWindowHostFactory() {
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
