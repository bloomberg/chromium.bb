// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/shell_delegate_impl.h"

#include "ash/common/accessibility_delegate.h"
#include "ash/common/default_accessibility_delegate.h"
#include "ash/common/gpu_support_stub.h"
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
#include "ui/aura/window.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {
namespace shell {
namespace {

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
  SessionStateDelegateImpl() : user_info_(new user_manager::UserInfoImpl()) {}

  ~SessionStateDelegateImpl() override {}

  // SessionStateDelegate:
  bool ShouldShowAvatar(WmWindow* window) const override {
    return !user_info_->GetImage().isNull();
  }
  gfx::ImageSkia GetAvatarImageForWindow(WmWindow* window) const override {
    return gfx::ImageSkia();
  }

 private:
  // A pseudo user info.
  std::unique_ptr<user_manager::UserInfo> user_info_;

  DISALLOW_COPY_AND_ASSIGN(SessionStateDelegateImpl);
};

}  // namespace

ShellDelegateImpl::ShellDelegateImpl() {}

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

ShelfDelegate* ShellDelegateImpl::CreateShelfDelegate(ShelfModel* model) {
  return new test::TestShelfDelegate();
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

bool ShellDelegateImpl::IsTouchscreenEnabledInPrefs(
    bool use_local_state) const {
  return true;
}

void ShellDelegateImpl::SetTouchscreenEnabledInPrefs(bool enabled,
                                                     bool use_local_state) {}

void ShellDelegateImpl::UpdateTouchscreenStatusFromPrefs() {}

}  // namespace shell
}  // namespace ash
