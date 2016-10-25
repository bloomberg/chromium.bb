// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/shell_delegate_impl.h"

#include "ash/app_list/app_list_presenter_delegate_factory.h"
#include "ash/common/accessibility_delegate.h"
#include "ash/common/default_accessibility_delegate.h"
#include "ash/common/gpu_support_stub.h"
#include "ash/common/media_delegate.h"
#include "ash/common/new_window_delegate.h"
#include "ash/common/palette_delegate.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/system/tray/default_system_tray_delegate.h"
#include "ash/common/test/test_shelf_delegate.h"
#include "ash/common/wm/window_state.h"
#include "ash/default_wallpaper_delegate.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/shell/context_menu.h"
#include "ash/shell/example_factory.h"
#include "ash/shell/toplevel_window.h"
#include "ash/test/test_keyboard_ui.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "components/user_manager/user_info_impl.h"
#include "ui/app_list/app_list_view_delegate.h"
#include "ui/app_list/presenter/app_list_presenter_impl.h"
#include "ui/app_list/presenter/app_list_view_delegate_factory.h"
#include "ui/aura/window.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace shell {
namespace {

class NewWindowDelegateImpl : public NewWindowDelegate {
 public:
  NewWindowDelegateImpl() {}
  ~NewWindowDelegateImpl() override {}

  // NewWindowDelegate:
  void NewTab() override {}
  void NewWindow(bool incognito) override {
    ToplevelWindow::CreateParams create_params;
    create_params.can_resize = true;
    create_params.can_maximize = true;
    ToplevelWindow::CreateToplevelWindow(create_params);
  }
  void OpenFileManager() override {}
  void OpenCrosh() override {}
  void OpenGetHelp() override {}
  void RestoreTab() override {}
  void ShowKeyboardOverlay() override {}
  void ShowTaskManager() override {}
  void OpenFeedbackPage() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NewWindowDelegateImpl);
};

class MediaDelegateImpl : public MediaDelegate {
 public:
  MediaDelegateImpl() {}
  ~MediaDelegateImpl() override {}

  // MediaDelegate:
  void HandleMediaNextTrack() override {}
  void HandleMediaPlayPause() override {}
  void HandleMediaPrevTrack() override {}
  MediaCaptureState GetMediaCaptureState(UserIndex index) override {
    return MEDIA_CAPTURE_VIDEO;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaDelegateImpl);
};

class PaletteDelegateImpl : public PaletteDelegate {
 public:
  PaletteDelegateImpl() {}
  ~PaletteDelegateImpl() override {}

  // PaletteDelegate:
  std::unique_ptr<EnableListenerSubscription> AddPaletteEnableListener(
      const EnableListener& on_state_changed) override {
    on_state_changed.Run(false);
    return nullptr;
  }
  void CreateNote() override {}
  bool HasNoteApp() override { return false; }
  void SetPartialMagnifierState(bool enabled) override {}
  void SetStylusStateChangedCallback(
      const OnStylusStateChangedCallback& on_stylus_state_changed) override {}
  bool ShouldAutoOpenPalette() override { return false; }
  bool ShouldShowPalette() override { return false; }
  void TakeScreenshot() override {}
  void TakePartialScreenshot(const base::Closure& done) override {
    if (done)
      done.Run();
  }
  void CancelPartialScreenshot() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(PaletteDelegateImpl);
};

class SessionStateDelegateImpl : public SessionStateDelegate {
 public:
  SessionStateDelegateImpl()
      : screen_locked_(false), user_info_(new user_manager::UserInfoImpl()) {}

  ~SessionStateDelegateImpl() override {}

  // SessionStateDelegate:
  int GetMaximumNumberOfLoggedInUsers() const override { return 3; }
  int NumberOfLoggedInUsers() const override {
    // ash_shell has 2 users.
    return 2;
  }
  bool IsActiveUserSessionStarted() const override { return true; }
  bool CanLockScreen() const override { return true; }
  bool IsScreenLocked() const override { return screen_locked_; }
  bool ShouldLockScreenAutomatically() const override { return false; }
  void LockScreen() override {
    shell::CreateLockScreen();
    screen_locked_ = true;
    Shell::GetInstance()->UpdateShelfVisibility();
  }
  void UnlockScreen() override {
    screen_locked_ = false;
    Shell::GetInstance()->UpdateShelfVisibility();
  }
  bool IsUserSessionBlocked() const override {
    return !IsActiveUserSessionStarted() || IsScreenLocked();
  }
  session_manager::SessionState GetSessionState() const override {
    // Assume that if session is not active we're at login.
    return IsActiveUserSessionStarted()
               ? session_manager::SessionState::ACTIVE
               : session_manager::SessionState::LOGIN_PRIMARY;
  }
  const user_manager::UserInfo* GetUserInfo(UserIndex index) const override {
    return user_info_.get();
  }
  bool ShouldShowAvatar(WmWindow* window) const override {
    return !user_info_->GetImage().isNull();
  }
  gfx::ImageSkia GetAvatarImageForWindow(WmWindow* window) const override {
    return gfx::ImageSkia();
  }
  void SwitchActiveUser(const AccountId& account_id) override {}
  void CycleActiveUser(CycleUser cycle_user) override {}
  bool IsMultiProfileAllowedByPrimaryUserPolicy() const override {
    return true;
  }
  void AddSessionStateObserver(SessionStateObserver* observer) override {}
  void RemoveSessionStateObserver(SessionStateObserver* observer) override {}

 private:
  bool screen_locked_;

  // A pseudo user info.
  std::unique_ptr<user_manager::UserInfo> user_info_;

  DISALLOW_COPY_AND_ASSIGN(SessionStateDelegateImpl);
};

class AppListViewDelegateFactoryImpl
    : public app_list::AppListViewDelegateFactory {
 public:
  AppListViewDelegateFactoryImpl() {}
  ~AppListViewDelegateFactoryImpl() override {}

  // app_list::AppListViewDelegateFactory:
  app_list::AppListViewDelegate* GetDelegate() override {
    if (!app_list_view_delegate_.get())
      app_list_view_delegate_.reset(CreateAppListViewDelegate());
    return app_list_view_delegate_.get();
  }

 private:
  std::unique_ptr<app_list::AppListViewDelegate> app_list_view_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AppListViewDelegateFactoryImpl);
};

}  // namespace

ShellDelegateImpl::ShellDelegateImpl()
    : shelf_delegate_(nullptr),
      app_list_presenter_delegate_factory_(new AppListPresenterDelegateFactory(
          base::WrapUnique(new AppListViewDelegateFactoryImpl))) {}

ShellDelegateImpl::~ShellDelegateImpl() {}

::service_manager::Connector* ShellDelegateImpl::GetShellConnector() const {
  return nullptr;
}

bool ShellDelegateImpl::IsIncognitoAllowed() const {
  return true;
}

bool ShellDelegateImpl::IsMultiProfilesEnabled() const {
  return false;
}

bool ShellDelegateImpl::IsRunningInForcedAppMode() const {
  return false;
}

bool ShellDelegateImpl::CanShowWindowForUser(WmWindow* window) const {
  return true;
}

bool ShellDelegateImpl::IsForceMaximizeOnFirstRun() const {
  return false;
}

void ShellDelegateImpl::PreInit() {}

void ShellDelegateImpl::PreShutdown() {}

void ShellDelegateImpl::Exit() {
  base::MessageLoop::current()->QuitWhenIdle();
}

keyboard::KeyboardUI* ShellDelegateImpl::CreateKeyboardUI() {
  return new TestKeyboardUI;
}

void ShellDelegateImpl::OpenUrlFromArc(const GURL& url) {}

app_list::AppListPresenter* ShellDelegateImpl::GetAppListPresenter() {
  if (!app_list_presenter_) {
    app_list_presenter_.reset(new app_list::AppListPresenterImpl(
        app_list_presenter_delegate_factory_.get()));
  }
  return app_list_presenter_.get();
}

ShelfDelegate* ShellDelegateImpl::CreateShelfDelegate(ShelfModel* model) {
  shelf_delegate_ = new test::TestShelfDelegate(model);
  return shelf_delegate_;
}

SystemTrayDelegate* ShellDelegateImpl::CreateSystemTrayDelegate() {
  return new DefaultSystemTrayDelegate;
}

std::unique_ptr<WallpaperDelegate>
ShellDelegateImpl::CreateWallpaperDelegate() {
  return base::MakeUnique<DefaultWallpaperDelegate>();
}

SessionStateDelegate* ShellDelegateImpl::CreateSessionStateDelegate() {
  return new SessionStateDelegateImpl;
}

AccessibilityDelegate* ShellDelegateImpl::CreateAccessibilityDelegate() {
  return new DefaultAccessibilityDelegate;
}

NewWindowDelegate* ShellDelegateImpl::CreateNewWindowDelegate() {
  return new NewWindowDelegateImpl;
}

MediaDelegate* ShellDelegateImpl::CreateMediaDelegate() {
  return new MediaDelegateImpl;
}

std::unique_ptr<PaletteDelegate> ShellDelegateImpl::CreatePaletteDelegate() {
  return base::MakeUnique<PaletteDelegateImpl>();
}

ui::MenuModel* ShellDelegateImpl::CreateContextMenu(WmShelf* wm_shelf,
                                                    const ShelfItem* item) {
  return new ContextMenu(wm_shelf);
}

GPUSupport* ShellDelegateImpl::CreateGPUSupport() {
  // Real GPU support depends on src/content, so just use a stub.
  return new GPUSupportStub;
}

base::string16 ShellDelegateImpl::GetProductName() const {
  return base::string16();
}

gfx::Image ShellDelegateImpl::GetDeprecatedAcceleratorImage() const {
  return gfx::Image();
}

}  // namespace shell
}  // namespace ash
