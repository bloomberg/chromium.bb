// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_SHELL_DELEGATE_H_
#define ASH_TEST_TEST_SHELL_DELEGATE_H_

#include <string>

#include "ash/shell_delegate.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"

namespace keyboard {
class KeyboardControllerProxy;
}

namespace ash {
namespace test {

class TestSessionStateDelegate;

class TestShellDelegate : public ShellDelegate {
 public:
  TestShellDelegate();
  virtual ~TestShellDelegate();

  // Overridden from ShellDelegate:
  virtual bool IsFirstRunAfterBoot() const OVERRIDE;
  virtual bool IsMultiProfilesEnabled() const OVERRIDE;
  virtual bool IsRunningInForcedAppMode() const OVERRIDE;
  virtual void PreInit() OVERRIDE;
  virtual void Shutdown() OVERRIDE;
  virtual void Exit() OVERRIDE;
  virtual void NewTab() OVERRIDE;
  virtual void NewWindow(bool incognito) OVERRIDE;
  virtual void ToggleFullscreen() OVERRIDE;
  virtual void ToggleMaximized() OVERRIDE;
  virtual void OpenFileManager(bool as_dialog) OVERRIDE;
  virtual void OpenCrosh() OVERRIDE;
  virtual void OpenMobileSetup(const std::string& service_path) OVERRIDE;
  virtual void RestoreTab() OVERRIDE;
  virtual void ShowKeyboardOverlay() OVERRIDE;
  virtual keyboard::KeyboardControllerProxy*
      CreateKeyboardControllerProxy() OVERRIDE;
  virtual void ShowTaskManager() OVERRIDE;
  virtual content::BrowserContext* GetCurrentBrowserContext() OVERRIDE;
  virtual void ToggleSpokenFeedback(
      AccessibilityNotificationVisibility notify) OVERRIDE;
  virtual bool IsSpokenFeedbackEnabled() const OVERRIDE;
  virtual void ToggleHighContrast() OVERRIDE;
  virtual bool IsHighContrastEnabled() const OVERRIDE;
  virtual void SetMagnifierEnabled(bool enabled) OVERRIDE;
  virtual void SetMagnifierType(MagnifierType type) OVERRIDE;
  virtual bool IsMagnifierEnabled() const OVERRIDE;
  virtual MagnifierType GetMagnifierType() const OVERRIDE;
  virtual bool ShouldAlwaysShowAccessibilityMenu() const OVERRIDE;
  virtual void SilenceSpokenFeedback() const OVERRIDE;
  virtual app_list::AppListViewDelegate* CreateAppListViewDelegate() OVERRIDE;
  virtual LauncherDelegate* CreateLauncherDelegate(
      ash::LauncherModel* model) OVERRIDE;
  virtual SystemTrayDelegate* CreateSystemTrayDelegate() OVERRIDE;
  virtual UserWallpaperDelegate* CreateUserWallpaperDelegate() OVERRIDE;
  virtual CapsLockDelegate* CreateCapsLockDelegate() OVERRIDE;
  virtual SessionStateDelegate* CreateSessionStateDelegate() OVERRIDE;
  virtual aura::client::UserActionClient* CreateUserActionClient() OVERRIDE;
  virtual void OpenFeedbackPage() OVERRIDE;
  virtual void RecordUserMetricsAction(UserMetricsAction action) OVERRIDE;
  virtual void HandleMediaNextTrack() OVERRIDE;
  virtual void HandleMediaPlayPause() OVERRIDE;
  virtual void HandleMediaPrevTrack() OVERRIDE;
  virtual base::string16 GetTimeRemainingString(base::TimeDelta delta) OVERRIDE;
  virtual base::string16 GetTimeDurationLongString(
      base::TimeDelta delta) OVERRIDE;
  virtual void SaveScreenMagnifierScale(double scale) OVERRIDE;
  virtual double GetSavedScreenMagnifierScale() OVERRIDE;
  virtual ui::MenuModel* CreateContextMenu(aura::RootWindow* root) OVERRIDE;
  virtual RootWindowHostFactory* CreateRootWindowHostFactory() OVERRIDE;
  virtual base::string16 GetProductName() const OVERRIDE;

  int num_exit_requests() const { return num_exit_requests_; }

  TestSessionStateDelegate* test_session_state_delegate();

 private:
  bool spoken_feedback_enabled_;
  bool high_contrast_enabled_;
  bool screen_magnifier_enabled_;
  MagnifierType screen_magnifier_type_;
  int num_exit_requests_;

  scoped_ptr<content::BrowserContext> current_browser_context_;

  TestSessionStateDelegate* test_session_state_delegate_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(TestShellDelegate);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_SHELL_DELEGATE_H_
