// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_SHELL_DELEGATE_IMPL_H_
#define ASH_SHELL_SHELL_DELEGATE_IMPL_H_

#include <string>

#include "ash/shell_delegate.h"
#include "base/compiler_specific.h"

namespace ash {
namespace shell {

class LauncherDelegateImpl;
class WindowWatcher;

class ShellDelegateImpl : public ash::ShellDelegate {
 public:
  ShellDelegateImpl();
  virtual ~ShellDelegateImpl();

  void SetWatcher(WindowWatcher* watcher);

  virtual bool IsUserLoggedIn() const OVERRIDE;
  virtual bool IsSessionStarted() const OVERRIDE;
  virtual bool IsFirstRunAfterBoot() const OVERRIDE;
  virtual bool IsRunningInForcedAppMode() const OVERRIDE;
  virtual bool CanLockScreen() const OVERRIDE;
  virtual void LockScreen() OVERRIDE;
  virtual void UnlockScreen() OVERRIDE;
  virtual bool IsScreenLocked() const OVERRIDE;
  virtual void Shutdown() OVERRIDE;
  virtual void Exit() OVERRIDE;
  virtual void NewTab() OVERRIDE;
  virtual void NewWindow(bool incognito) OVERRIDE;
  virtual void ToggleMaximized() OVERRIDE;
  virtual void OpenFileManager(bool as_dialog) OVERRIDE;
  virtual void OpenCrosh() OVERRIDE;
  virtual void OpenMobileSetup(const std::string& service_path) OVERRIDE;
  virtual void RestoreTab() OVERRIDE;
  virtual bool RotatePaneFocus(Shell::Direction direction) OVERRIDE;
  virtual void ShowKeyboardOverlay() OVERRIDE;
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
  virtual app_list::AppListViewDelegate* CreateAppListViewDelegate() OVERRIDE;
  virtual ash::LauncherDelegate* CreateLauncherDelegate(
      ash::LauncherModel* model) OVERRIDE;
  virtual ash::SystemTrayDelegate* CreateSystemTrayDelegate() OVERRIDE;
  virtual ash::UserWallpaperDelegate* CreateUserWallpaperDelegate() OVERRIDE;
  virtual ash::CapsLockDelegate* CreateCapsLockDelegate() OVERRIDE;
  virtual aura::client::UserActionClient* CreateUserActionClient() OVERRIDE;
  virtual void OpenFeedbackPage() OVERRIDE;
  virtual void RecordUserMetricsAction(UserMetricsAction action) OVERRIDE;
  virtual void HandleMediaNextTrack() OVERRIDE;
  virtual void HandleMediaPlayPause() OVERRIDE;
  virtual void HandleMediaPrevTrack() OVERRIDE;
  virtual string16 GetTimeRemainingString(base::TimeDelta delta) OVERRIDE;
  virtual string16 GetTimeDurationLongString(base::TimeDelta delta) OVERRIDE;
  virtual void SaveScreenMagnifierScale(double scale) OVERRIDE;
  virtual double GetSavedScreenMagnifierScale() OVERRIDE;
  virtual ui::MenuModel* CreateContextMenu(
      aura::RootWindow* root_window) OVERRIDE;
  virtual RootWindowHostFactory* CreateRootWindowHostFactory() OVERRIDE;
  virtual string16 GetProductName() const OVERRIDE;

 private:
  // Used to update Launcher. Owned by main.
  WindowWatcher* watcher_;

  LauncherDelegateImpl* launcher_delegate_;

  bool locked_;
  bool spoken_feedback_enabled_;
  bool high_contrast_enabled_;
  bool screen_magnifier_enabled_;
  MagnifierType screen_magnifier_type_;

  DISALLOW_COPY_AND_ASSIGN(ShellDelegateImpl);
};

}  // namespace shell
}  // namespace ash

#endif  // ASH_SHELL_SHELL_DELEGATE_IMPL_H_
