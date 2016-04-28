// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_shell_delegate.h"

#include <limits>

#include "ash/app_list/app_list_presenter_delegate.h"
#include "ash/app_list/app_list_presenter_delegate_factory.h"
#include "ash/app_list/app_list_view_delegate_factory.h"
#include "ash/default_accessibility_delegate.h"
#include "ash/gpu_support_stub.h"
#include "ash/media_delegate.h"
#include "ash/new_window_delegate.h"
#include "ash/pointer_watcher_delegate_aura.h"
#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/test/test_keyboard_ui.h"
#include "ash/test/test_session_state_delegate.h"
#include "ash/test/test_shelf_delegate.h"
#include "ash/test/test_system_tray_delegate.h"
#include "ash/test/test_user_wallpaper_delegate.h"
#include "ash/wm/common/window_state.h"
#include "ash/wm/window_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "ui/app_list/presenter/app_list_presenter_impl.h"
#include "ui/app_list/test/app_list_test_view_delegate.h"
#include "ui/aura/window.h"
#include "ui/gfx/image/image.h"

#if defined(OS_CHROMEOS)
#include "ash/system/tray/system_tray_notifier.h"
#endif

namespace ash {
namespace test {
namespace {

class NewWindowDelegateImpl : public NewWindowDelegate {
 public:
  NewWindowDelegateImpl() {}
  ~NewWindowDelegateImpl() override {}

 private:
  // NewWindowDelegate:
  void NewTab() override {}
  void NewWindow(bool incognito) override {}
  void OpenFileManager() override {}
  void OpenCrosh() override {}
  void OpenGetHelp() override {}
  void RestoreTab() override {}
  void ShowKeyboardOverlay() override {}
  void ShowTaskManager() override {}
  void OpenFeedbackPage() override {}

  DISALLOW_COPY_AND_ASSIGN(NewWindowDelegateImpl);
};

class MediaDelegateImpl : public MediaDelegate {
 public:
  MediaDelegateImpl() : state_(MEDIA_CAPTURE_NONE) {}
  ~MediaDelegateImpl() override {}

  void set_media_capture_state(MediaCaptureState state) { state_ = state; }

 private:
  // MediaDelegate:
  void HandleMediaNextTrack() override {}
  void HandleMediaPlayPause() override {}
  void HandleMediaPrevTrack() override {}
  MediaCaptureState GetMediaCaptureState(UserIndex index) override {
    return state_;
  }

  MediaCaptureState state_;

  DISALLOW_COPY_AND_ASSIGN(MediaDelegateImpl);
};

class AppListViewDelegateFactoryImpl : public ash::AppListViewDelegateFactory {
 public:
  AppListViewDelegateFactoryImpl() {}
  ~AppListViewDelegateFactoryImpl() override {}

  // app_list::AppListViewDelegateFactory:
  app_list::AppListViewDelegate* GetDelegate() override {
    if (!app_list_view_delegate_.get()) {
      app_list_view_delegate_.reset(
          new app_list::test::AppListTestViewDelegate);
    }
    return app_list_view_delegate_.get();
  }

 private:
  std::unique_ptr<app_list::AppListViewDelegate> app_list_view_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AppListViewDelegateFactoryImpl);
};

}  // namespace

TestShellDelegate::TestShellDelegate()
    : num_exit_requests_(0),
      multi_profiles_enabled_(false),
      force_maximize_on_first_run_(false),
      app_list_presenter_delegate_factory_(new AppListPresenterDelegateFactory(
          base::WrapUnique(new AppListViewDelegateFactoryImpl))) {}

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

bool TestShellDelegate::CanShowWindowForUser(aura::Window* window) const {
  return true;
}

bool TestShellDelegate::IsForceMaximizeOnFirstRun() const {
  return force_maximize_on_first_run_;
}

void TestShellDelegate::PreInit() {
}

void TestShellDelegate::PreShutdown() {
}

void TestShellDelegate::Exit() {
  num_exit_requests_++;
}

keyboard::KeyboardUI* TestShellDelegate::CreateKeyboardUI() {
  return new TestKeyboardUI;
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

void TestShellDelegate::OpenUrl(const GURL& url) {}

app_list::AppListPresenter* TestShellDelegate::GetAppListPresenter() {
  if (!app_list_presenter_) {
    app_list_presenter_.reset(new app_list::AppListPresenterImpl(
        app_list_presenter_delegate_factory_.get()));
  }
  return app_list_presenter_.get();
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

TestSessionStateDelegate* TestShellDelegate::CreateSessionStateDelegate() {
  return new TestSessionStateDelegate();
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

std::unique_ptr<PointerWatcherDelegate>
TestShellDelegate::CreatePointerWatcherDelegate() {
  return base::WrapUnique(new PointerWatcherDelegateAura);
}

ui::MenuModel* TestShellDelegate::CreateContextMenu(
    ash::Shelf* shelf,
    const ash::ShelfItem* item) {
  return nullptr;
}

GPUSupport* TestShellDelegate::CreateGPUSupport() {
  // Real GPU support depends on src/content, so just use a stub.
  return new GPUSupportStub;
}

base::string16 TestShellDelegate::GetProductName() const {
  return base::string16();
}

gfx::Image TestShellDelegate::GetDeprecatedAcceleratorImage() const {
  return gfx::Image();
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
