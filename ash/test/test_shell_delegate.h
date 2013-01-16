// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_SHELL_DELEGATE_H_
#define ASH_TEST_TEST_SHELL_DELEGATE_H_

#include "ash/shell_delegate.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"

namespace ash {
namespace test {

class AshTestBase;

class TestShellDelegate : public ShellDelegate {
 public:
  TestShellDelegate();
  virtual ~TestShellDelegate();

  // Overridden from ShellDelegate:
  virtual bool IsUserLoggedIn() const OVERRIDE;
  virtual bool IsSessionStarted() const OVERRIDE;
  virtual bool IsFirstRunAfterBoot() const OVERRIDE;
  virtual bool CanLockScreen() const OVERRIDE;
  virtual void LockScreen() OVERRIDE;
  virtual void UnlockScreen() OVERRIDE;
  virtual bool IsScreenLocked() const OVERRIDE;
  virtual void Shutdown() OVERRIDE;
  virtual void Exit() OVERRIDE;
  virtual void NewTab() OVERRIDE;
  virtual void NewWindow(bool incognito) OVERRIDE;
  virtual void ToggleMaximized() OVERRIDE;
  virtual void OpenFileManager() OVERRIDE;
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
  virtual LauncherDelegate* CreateLauncherDelegate(
      ash::LauncherModel* model) OVERRIDE;
  virtual SystemTrayDelegate* CreateSystemTrayDelegate() OVERRIDE;
  virtual UserWallpaperDelegate* CreateUserWallpaperDelegate() OVERRIDE;
  virtual CapsLockDelegate* CreateCapsLockDelegate() OVERRIDE;
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
  virtual ui::MenuModel* CreateContextMenu(aura::RootWindow* root) OVERRIDE;
  virtual RootWindowHostFactory* CreateRootWindowHostFactory() OVERRIDE;

  int num_exit_requests() const { return num_exit_requests_; }
 private:
  friend class ash::test::AshTestBase;

  // Given |session_started| will update internal state.
  // If |session_started| is true this method will also set
  // |user_logged_in_| to true.
  // When session is started it always means that user has logged in.
  // Possible situation is that user has already logged in but session has not
  // been started (user selects avatar and login window is still open).
  void SetSessionStarted(bool session_started);

  // Given |user_logged_in| will update internal state.
  // If |user_logged_in| is false this method will also set |session_started_|
  // to false. When user is not logged in it always means that session
  // hasn't been started too.
  void SetUserLoggedIn(bool user_logged_in);

  // Sets the internal state that indicates whether the user can lock the
  // screen.
  void SetCanLockScreen(bool can_lock_screen);

  bool locked_;
  bool session_started_;
  bool spoken_feedback_enabled_;
  bool high_contrast_enabled_;
  bool screen_magnifier_enabled_;
  MagnifierType screen_magnifier_type_;
  bool user_logged_in_;
  bool can_lock_screen_;
  int num_exit_requests_;

  scoped_ptr<content::BrowserContext> current_browser_context_;

  DISALLOW_COPY_AND_ASSIGN(TestShellDelegate);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_SHELL_DELEGATE_H_
