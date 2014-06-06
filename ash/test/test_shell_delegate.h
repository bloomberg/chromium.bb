// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TEST_SHELL_DELEGATE_H_
#define ASH_TEST_TEST_SHELL_DELEGATE_H_

#include <string>

#include "ash/media_delegate.h"
#include "ash/shell_delegate.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"

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

  void set_multi_profiles_enabled(bool multi_profiles_enabled) {
    multi_profiles_enabled_ = multi_profiles_enabled;
  }

  // Overridden from ShellDelegate:
  virtual bool IsFirstRunAfterBoot() const OVERRIDE;
  virtual bool IsIncognitoAllowed() const OVERRIDE;
  virtual bool IsMultiProfilesEnabled() const OVERRIDE;
  virtual bool IsRunningInForcedAppMode() const OVERRIDE;
  virtual bool IsMultiAccountEnabled() const OVERRIDE;
  virtual void PreInit() OVERRIDE;
  virtual void PreShutdown() OVERRIDE;
  virtual void Exit() OVERRIDE;
  virtual keyboard::KeyboardControllerProxy*
      CreateKeyboardControllerProxy() OVERRIDE;
  virtual void VirtualKeyboardActivated(bool activated) OVERRIDE;
  virtual void AddVirtualKeyboardStateObserver(
      VirtualKeyboardStateObserver* observer) OVERRIDE;
  virtual void RemoveVirtualKeyboardStateObserver(
      VirtualKeyboardStateObserver* observer) OVERRIDE;
  virtual content::BrowserContext* GetActiveBrowserContext() OVERRIDE;
  virtual app_list::AppListViewDelegate* CreateAppListViewDelegate() OVERRIDE;
  virtual ShelfDelegate* CreateShelfDelegate(ShelfModel* model) OVERRIDE;
  virtual SystemTrayDelegate* CreateSystemTrayDelegate() OVERRIDE;
  virtual UserWallpaperDelegate* CreateUserWallpaperDelegate() OVERRIDE;
  virtual SessionStateDelegate* CreateSessionStateDelegate() OVERRIDE;
  virtual AccessibilityDelegate* CreateAccessibilityDelegate() OVERRIDE;
  virtual NewWindowDelegate* CreateNewWindowDelegate() OVERRIDE;
  virtual MediaDelegate* CreateMediaDelegate() OVERRIDE;
  virtual ui::MenuModel* CreateContextMenu(
      aura::Window* root,
      ash::ShelfItemDelegate* item_delegate,
      ash::ShelfItem* item) OVERRIDE;
  virtual GPUSupport* CreateGPUSupport() OVERRIDE;
  virtual base::string16 GetProductName() const OVERRIDE;

  int num_exit_requests() const { return num_exit_requests_; }

  TestSessionStateDelegate* test_session_state_delegate() {
    return test_session_state_delegate_;
  }

  void SetMediaCaptureState(MediaCaptureState state);

 private:
  int num_exit_requests_;
  bool multi_profiles_enabled_;

  scoped_ptr<content::BrowserContext> active_browser_context_;

  ObserverList<ash::VirtualKeyboardStateObserver> keyboard_state_observer_list_;

  TestSessionStateDelegate* test_session_state_delegate_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(TestShellDelegate);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_SHELL_DELEGATE_H_
