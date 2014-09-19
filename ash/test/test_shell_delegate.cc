// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_shell_delegate.h"

#include <limits>

#include "ash/default_accessibility_delegate.h"
#include "ash/gpu_support_stub.h"
#include "ash/media_delegate.h"
#include "ash/new_window_delegate.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/shell/keyboard_controller_proxy_stub.h"
#include "ash/shell_window_ids.h"
#include "ash/test/test_session_state_delegate.h"
#include "ash/test/test_shelf_delegate.h"
#include "ash/test/test_system_tray_delegate.h"
#include "ash/test/test_user_wallpaper_delegate.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/logging.h"
#include "content/public/test/test_browser_context.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/test/app_list_test_view_delegate.h"
#include "ui/aura/window.h"

#if defined(OS_CHROMEOS)
#include "ash/system/tray/system_tray_notifier.h"
#endif

namespace ash {
namespace test {
namespace {

class NewWindowDelegateImpl : public NewWindowDelegate {
 public:
  NewWindowDelegateImpl() {}
  virtual ~NewWindowDelegateImpl() {}

 private:
  // NewWindowDelegate:
  virtual void NewTab() OVERRIDE {}
  virtual void NewWindow(bool incognito) OVERRIDE {}
  virtual void OpenFileManager() OVERRIDE {}
  virtual void OpenCrosh() OVERRIDE {}
  virtual void RestoreTab() OVERRIDE {}
  virtual void ShowKeyboardOverlay() OVERRIDE {}
  virtual void ShowTaskManager() OVERRIDE {}
  virtual void OpenFeedbackPage() OVERRIDE {}

  DISALLOW_COPY_AND_ASSIGN(NewWindowDelegateImpl);
};

class MediaDelegateImpl : public MediaDelegate {
 public:
  MediaDelegateImpl() : state_(MEDIA_CAPTURE_NONE) {}
  virtual ~MediaDelegateImpl() {}

  void set_media_capture_state(MediaCaptureState state) { state_ = state; }

 private:
  // MediaDelegate:
  virtual void HandleMediaNextTrack() OVERRIDE {}
  virtual void HandleMediaPlayPause() OVERRIDE {}
  virtual void HandleMediaPrevTrack() OVERRIDE {}
  virtual MediaCaptureState GetMediaCaptureState(
      content::BrowserContext* context) OVERRIDE {
    return state_;
  }

  MediaCaptureState state_;

  DISALLOW_COPY_AND_ASSIGN(MediaDelegateImpl);
};

}  // namespace

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

bool TestShellDelegate::IsIncognitoAllowed() const {
  return true;
}

bool TestShellDelegate::IsMultiProfilesEnabled() const {
  return multi_profiles_enabled_;
}

bool TestShellDelegate::IsRunningInForcedAppMode() const {
  return false;
}

bool TestShellDelegate::IsMultiAccountEnabled() const {
  return false;
}

void TestShellDelegate::PreInit() {
}

void TestShellDelegate::PreShutdown() {
}

void TestShellDelegate::Exit() {
  num_exit_requests_++;
}

keyboard::KeyboardControllerProxy*
    TestShellDelegate::CreateKeyboardControllerProxy() {
  return new KeyboardControllerProxyStub();
}

void TestShellDelegate::VirtualKeyboardActivated(bool activated) {
  FOR_EACH_OBSERVER(ash::VirtualKeyboardStateObserver,
                    keyboard_state_observer_list_,
                    OnVirtualKeyboardStateChanged(activated));
}

void TestShellDelegate::AddVirtualKeyboardStateObserver(
    VirtualKeyboardStateObserver* observer) {
  keyboard_state_observer_list_.AddObserver(observer);
}

void TestShellDelegate::RemoveVirtualKeyboardStateObserver(
    VirtualKeyboardStateObserver* observer) {
  keyboard_state_observer_list_.RemoveObserver(observer);
}

content::BrowserContext* TestShellDelegate::GetActiveBrowserContext() {
  active_browser_context_.reset(new content::TestBrowserContext());
  return active_browser_context_.get();
}

app_list::AppListViewDelegate* TestShellDelegate::GetAppListViewDelegate() {
  if (!app_list_view_delegate_)
    app_list_view_delegate_.reset(new app_list::test::AppListTestViewDelegate);
  return app_list_view_delegate_.get();
}

ShelfDelegate* TestShellDelegate::CreateShelfDelegate(ShelfModel* model) {
  return new TestShelfDelegate(model);
}

SystemTrayDelegate* TestShellDelegate::CreateSystemTrayDelegate() {
  return new TestSystemTrayDelegate;
}

UserWallpaperDelegate* TestShellDelegate::CreateUserWallpaperDelegate() {
  return new TestUserWallpaperDelegate();
}

SessionStateDelegate* TestShellDelegate::CreateSessionStateDelegate() {
  DCHECK(!test_session_state_delegate_);
  test_session_state_delegate_ = new TestSessionStateDelegate();
  return test_session_state_delegate_;
}

AccessibilityDelegate* TestShellDelegate::CreateAccessibilityDelegate() {
  return new DefaultAccessibilityDelegate();
}

NewWindowDelegate* TestShellDelegate::CreateNewWindowDelegate() {
  return new NewWindowDelegateImpl;
}

MediaDelegate* TestShellDelegate::CreateMediaDelegate() {
  return new MediaDelegateImpl;
}

ui::MenuModel* TestShellDelegate::CreateContextMenu(
    aura::Window* root,
    ash::ShelfItemDelegate* item_delegate,
    ash::ShelfItem* item) {
  return NULL;
}

GPUSupport* TestShellDelegate::CreateGPUSupport() {
  // Real GPU support depends on src/content, so just use a stub.
  return new GPUSupportStub;
}

base::string16 TestShellDelegate::GetProductName() const {
  return base::string16();
}

void TestShellDelegate::SetMediaCaptureState(MediaCaptureState state) {
#if defined(OS_CHROMEOS)
  Shell* shell = Shell::GetInstance();
  static_cast<MediaDelegateImpl*>(shell->media_delegate())
      ->set_media_capture_state(state);
  shell->system_tray_notifier()->NotifyMediaCaptureChanged();
#endif
}

}  // namespace test
}  // namespace ash
