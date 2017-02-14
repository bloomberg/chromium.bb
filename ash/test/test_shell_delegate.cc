// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_shell_delegate.h"

#include <limits>

#include "ash/common/default_accessibility_delegate.h"
#include "ash/common/gpu_support_stub.h"
#include "ash/common/palette_delegate.h"
#include "ash/common/session/session_state_delegate.h"
#include "ash/common/system/tray/system_tray_notifier.h"
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
#include "ui/aura/window.h"
#include "ui/gfx/image/image.h"

namespace ash {
namespace test {

TestShellDelegate::TestShellDelegate()
    : num_exit_requests_(0),
      multi_profiles_enabled_(false),
      force_maximize_on_first_run_(false),
      touchscreen_enabled_in_local_pref_(true) {}

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

ShelfDelegate* TestShellDelegate::CreateShelfDelegate(ShelfModel* model) {
  return new TestShelfDelegate();
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

bool TestShellDelegate::IsTouchscreenEnabledInPrefs(
    bool use_local_state) const {
  return use_local_state ? touchscreen_enabled_in_local_pref_ : true;
}

void TestShellDelegate::SetTouchscreenEnabledInPrefs(bool enabled,
                                                     bool use_local_state) {
  if (use_local_state)
    touchscreen_enabled_in_local_pref_ = enabled;
}

void TestShellDelegate::UpdateTouchscreenStatusFromPrefs() {}

}  // namespace test
}  // namespace ash
