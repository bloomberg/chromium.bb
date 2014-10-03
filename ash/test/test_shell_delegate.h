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
  virtual bool IsFirstRunAfterBoot() const override;
  virtual bool IsIncognitoAllowed() const override;
  virtual bool IsMultiProfilesEnabled() const override;
  virtual bool IsRunningInForcedAppMode() const override;
  virtual bool IsMultiAccountEnabled() const override;
  virtual void PreInit() override;
  virtual void PreShutdown() override;
  virtual void Exit() override;
  virtual keyboard::KeyboardControllerProxy*
      CreateKeyboardControllerProxy() override;
  virtual void VirtualKeyboardActivated(bool activated) override;
  virtual void AddVirtualKeyboardStateObserver(
      VirtualKeyboardStateObserver* observer) override;
  virtual void RemoveVirtualKeyboardStateObserver(
      VirtualKeyboardStateObserver* observer) override;
  virtual content::BrowserContext* GetActiveBrowserContext() override;
  virtual app_list::AppListViewDelegate* GetAppListViewDelegate() override;
  virtual ShelfDelegate* CreateShelfDelegate(ShelfModel* model) override;
  virtual SystemTrayDelegate* CreateSystemTrayDelegate() override;
  virtual UserWallpaperDelegate* CreateUserWallpaperDelegate() override;
  virtual SessionStateDelegate* CreateSessionStateDelegate() override;
  virtual AccessibilityDelegate* CreateAccessibilityDelegate() override;
  virtual NewWindowDelegate* CreateNewWindowDelegate() override;
  virtual MediaDelegate* CreateMediaDelegate() override;
  virtual ui::MenuModel* CreateContextMenu(
      aura::Window* root,
      ash::ShelfItemDelegate* item_delegate,
      ash::ShelfItem* item) override;
  virtual GPUSupport* CreateGPUSupport() override;
  virtual base::string16 GetProductName() const override;

  int num_exit_requests() const { return num_exit_requests_; }

  TestSessionStateDelegate* test_session_state_delegate() {
    return test_session_state_delegate_;
  }

  void SetMediaCaptureState(MediaCaptureState state);

 private:
  int num_exit_requests_;
  bool multi_profiles_enabled_;

  scoped_ptr<content::BrowserContext> active_browser_context_;
  scoped_ptr<app_list::AppListViewDelegate> app_list_view_delegate_;

  ObserverList<ash::VirtualKeyboardStateObserver> keyboard_state_observer_list_;

  TestSessionStateDelegate* test_session_state_delegate_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(TestShellDelegate);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_TEST_SHELL_DELEGATE_H_
