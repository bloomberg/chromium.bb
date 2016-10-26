// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_shell_delegate.h"

#include <limits>

#include "ash/app_list/app_list_presenter_delegate.h"
#include "ash/app_list/app_list_presenter_delegate_factory.h"
#include "ash/common/default_accessibility_delegate.h"
#include "ash/common/gpu_support_stub.h"
#include "ash/common/media_delegate.h"
#include "ash/common/palette_delegate.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/test/test_session_state_delegate.h"
#include "ash/common/test/test_shelf_delegate.h"
#include "ash/common/test/test_system_tray_delegate.h"
#include "ash/common/wm/window_state.h"
#include "ash/common/wm_shell.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/test/test_keyboard_ui.h"
#include "ash/test/test_wallpaper_delegate.h"
#include "ash/wm/window_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "ui/app_list/presenter/app_list_presenter_impl.h"
#include "ui/app_list/presenter/app_list_view_delegate_factory.h"
#include "ui/app_list/test/app_list_test_view_delegate.h"
#include "ui/aura/window.h"
#include "ui/gfx/image/image.h"

#if defined(OS_CHROMEOS)
#include "ash/common/system/tray/system_tray_notifier.h"
#endif

namespace ash {
namespace test {
namespace {

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

class AppListViewDelegateFactoryImpl
    : public app_list::AppListViewDelegateFactory {
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

TestShellDelegate::~TestShellDelegate() {}

::service_manager::Connector* TestShellDelegate::GetShellConnector() const {
  return nullptr;
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

bool TestShellDelegate::CanShowWindowForUser(WmWindow* window) const {
  return true;
}

bool TestShellDelegate::IsForceMaximizeOnFirstRun() const {
  return force_maximize_on_first_run_;
}

void TestShellDelegate::PreInit() {}

void TestShellDelegate::PreShutdown() {}

void TestShellDelegate::Exit() {
  num_exit_requests_++;
}

keyboard::KeyboardUI* TestShellDelegate::CreateKeyboardUI() {
  return new TestKeyboardUI;
}

void TestShellDelegate::OpenUrlFromArc(const GURL& url) {}

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

std::unique_ptr<WallpaperDelegate>
TestShellDelegate::CreateWallpaperDelegate() {
  return base::MakeUnique<TestWallpaperDelegate>();
}

TestSessionStateDelegate* TestShellDelegate::CreateSessionStateDelegate() {
  return new TestSessionStateDelegate();
}

AccessibilityDelegate* TestShellDelegate::CreateAccessibilityDelegate() {
  return new DefaultAccessibilityDelegate();
}

MediaDelegate* TestShellDelegate::CreateMediaDelegate() {
  return new MediaDelegateImpl;
}

std::unique_ptr<PaletteDelegate> TestShellDelegate::CreatePaletteDelegate() {
  return nullptr;
}

ui::MenuModel* TestShellDelegate::CreateContextMenu(WmShelf* wm_shelf,
                                                    const ShelfItem* item) {
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
  static_cast<MediaDelegateImpl*>(WmShell::Get()->media_delegate())
      ->set_media_capture_state(state);
  WmShell::Get()->system_tray_notifier()->NotifyMediaCaptureChanged();
#endif
}

}  // namespace test
}  // namespace ash
