// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/test_shell_delegate.h"

#include <limits>

#include "ash/gpu_support_stub.h"
#include "ash/palette_delegate.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/test/test_accessibility_delegate.h"
#include "ash/test/test_keyboard_ui.h"
#include "ash/test/test_wallpaper_delegate.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "ui/aura/window.h"
#include "ui/gfx/image/image.h"

namespace ash {
namespace test {

// A ShellObserver that sets the shelf alignment and auto hide behavior when the
// shelf is created, to simulate ChromeLauncherController's behavior.
class ShelfInitializer : public ShellObserver {
 public:
  ShelfInitializer() { Shell::Get()->AddShellObserver(this); }
  ~ShelfInitializer() override { Shell::Get()->RemoveShellObserver(this); }

  // ShellObserver:
  void OnShelfCreatedForRootWindow(aura::Window* root_window) override {
    Shelf* shelf = RootWindowController::ForWindow(root_window)->shelf();
    // Do not override the custom initialization performed by some unit tests.
    if (shelf->alignment() == SHELF_ALIGNMENT_BOTTOM_LOCKED &&
        shelf->auto_hide_behavior() == SHELF_AUTO_HIDE_ALWAYS_HIDDEN) {
      shelf->SetAlignment(SHELF_ALIGNMENT_BOTTOM);
      shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
      shelf->UpdateVisibilityState();
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ShelfInitializer);
};

TestShellDelegate::TestShellDelegate() = default;

TestShellDelegate::~TestShellDelegate() = default;

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

bool TestShellDelegate::CanShowWindowForUser(aura::Window* window) const {
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

void TestShellDelegate::ShelfInit() {
  // Create a separate shelf initializer that mimics ChromeLauncherController.
  if (!shelf_initializer_)
    shelf_initializer_ = base::MakeUnique<ShelfInitializer>();
}

void TestShellDelegate::ShelfShutdown() {}

SystemTrayDelegate* TestShellDelegate::CreateSystemTrayDelegate() {
  return new SystemTrayDelegate;
}

std::unique_ptr<WallpaperDelegate>
TestShellDelegate::CreateWallpaperDelegate() {
  return base::MakeUnique<TestWallpaperDelegate>();
}

AccessibilityDelegate* TestShellDelegate::CreateAccessibilityDelegate() {
  return new TestAccessibilityDelegate();
}

std::unique_ptr<PaletteDelegate> TestShellDelegate::CreatePaletteDelegate() {
  return nullptr;
}

ui::MenuModel* TestShellDelegate::CreateContextMenu(Shelf* shelf,
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

PrefService* TestShellDelegate::GetActiveUserPrefService() const {
  return active_user_pref_service_;
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

void TestShellDelegate::SuspendMediaSessions() {
  media_sessions_suspended_ = true;
}

#if defined(USE_OZONE)
ui::InputDeviceControllerClient*
TestShellDelegate::GetInputDeviceControllerClient() {
  return nullptr;
}
#endif

}  // namespace test
}  // namespace ash
