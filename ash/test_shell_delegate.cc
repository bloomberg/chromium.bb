// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test_shell_delegate.h"

#include <limits>

#include "ash/gpu_support_stub.h"
#include "ash/keyboard/test_keyboard_ui.h"
#include "ash/palette_delegate.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/touchscreen_enabled_source.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/test/test_accessibility_delegate.h"
#include "ash/wallpaper/test_wallpaper_delegate.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "ui/aura/window.h"
#include "ui/gfx/image/image.h"

namespace ash {

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

std::unique_ptr<keyboard::KeyboardUI> TestShellDelegate::CreateKeyboardUI() {
  return base::MakeUnique<TestKeyboardUI>();
}

void TestShellDelegate::OpenUrlFromArc(const GURL& url) {}

void TestShellDelegate::ShelfInit() {
  // Create a separate shelf initializer that mimics ChromeLauncherController.
  if (!shelf_initializer_)
    shelf_initializer_ = base::MakeUnique<ShelfInitializer>();
}

void TestShellDelegate::ShelfShutdown() {}

NetworkingConfigDelegate* TestShellDelegate::GetNetworkingConfigDelegate() {
  return nullptr;
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

bool TestShellDelegate::GetTouchscreenEnabled(
    TouchscreenEnabledSource source) const {
  return source == TouchscreenEnabledSource::GLOBAL
             ? global_touchscreen_enabled_
             : true;
}

void TestShellDelegate::SetTouchscreenEnabled(bool enabled,
                                              TouchscreenEnabledSource source) {
  DCHECK_EQ(TouchscreenEnabledSource::GLOBAL, source);
  global_touchscreen_enabled_ = enabled;
}

void TestShellDelegate::SuspendMediaSessions() {
  media_sessions_suspended_ = true;
}

ui::InputDeviceControllerClient*
TestShellDelegate::GetInputDeviceControllerClient() {
  return nullptr;
}

}  // namespace ash
