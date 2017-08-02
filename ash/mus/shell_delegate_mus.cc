// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/shell_delegate_mus.h"

#include <utility>

#include "ash/gpu_support_stub.h"
#include "ash/mus/accessibility_delegate_mus.h"
#include "ash/mus/context_menu_mus.h"
#include "ash/mus/system_tray_delegate_mus.h"
#include "ash/mus/wallpaper_delegate_mus.h"
#include "ash/palette_delegate.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "components/user_manager/user_info_impl.h"
#include "ui/gfx/image/image.h"
#include "ui/keyboard/keyboard_ui.h"

#if defined(USE_OZONE)
#include "services/ui/public/cpp/input_devices/input_device_controller_client.h"
#endif

namespace ash {

ShellDelegateMus::ShellDelegateMus(service_manager::Connector* connector)
    : connector_(connector) {}

ShellDelegateMus::~ShellDelegateMus() {}

service_manager::Connector* ShellDelegateMus::GetShellConnector() const {
  return connector_;
}

bool ShellDelegateMus::IsIncognitoAllowed() const {
  NOTIMPLEMENTED();
  return false;
}

bool ShellDelegateMus::IsMultiProfilesEnabled() const {
  NOTIMPLEMENTED();
  return true;  // For manual testing of multi-profile under mash.
}

bool ShellDelegateMus::IsRunningInForcedAppMode() const {
  NOTIMPLEMENTED();
  return false;
}

bool ShellDelegateMus::CanShowWindowForUser(aura::Window* window) const {
  NOTIMPLEMENTED();
  return true;
}

bool ShellDelegateMus::IsForceMaximizeOnFirstRun() const {
  NOTIMPLEMENTED();
  return false;
}

void ShellDelegateMus::PreInit() {
  NOTIMPLEMENTED();
}

void ShellDelegateMus::PreShutdown() {
  NOTIMPLEMENTED();
}

void ShellDelegateMus::Exit() {
  NOTIMPLEMENTED();
}

std::unique_ptr<keyboard::KeyboardUI> ShellDelegateMus::CreateKeyboardUI() {
  NOTIMPLEMENTED();
  return nullptr;
}

void ShellDelegateMus::OpenUrlFromArc(const GURL& url) {
  NOTIMPLEMENTED();
}

void ShellDelegateMus::ShelfInit() {
  NOTIMPLEMENTED();
}

void ShellDelegateMus::ShelfShutdown() {
  NOTIMPLEMENTED();
}

SystemTrayDelegate* ShellDelegateMus::CreateSystemTrayDelegate() {
  return new SystemTrayDelegateMus();
}

std::unique_ptr<WallpaperDelegate> ShellDelegateMus::CreateWallpaperDelegate() {
  return base::MakeUnique<WallpaperDelegateMus>();
}

AccessibilityDelegate* ShellDelegateMus::CreateAccessibilityDelegate() {
  return new AccessibilityDelegateMus(connector_);
}

std::unique_ptr<PaletteDelegate> ShellDelegateMus::CreatePaletteDelegate() {
  // TODO: http://crbug.com/647417.
  NOTIMPLEMENTED();
  return nullptr;
}

ui::MenuModel* ShellDelegateMus::CreateContextMenu(Shelf* shelf,
                                                   const ShelfItem* item) {
  return new ContextMenuMus(shelf);
}

GPUSupport* ShellDelegateMus::CreateGPUSupport() {
  // TODO: http://crbug.com/647421.
  NOTIMPLEMENTED() << " Using a stub GPUSupport implementation";
  return new GPUSupportStub();
}

base::string16 ShellDelegateMus::GetProductName() const {
  NOTIMPLEMENTED();
  return base::string16();
}

gfx::Image ShellDelegateMus::GetDeprecatedAcceleratorImage() const {
  NOTIMPLEMENTED();
  return gfx::Image();
}

PrefService* ShellDelegateMus::GetActiveUserPrefService() const {
  // This code should never be called in the case of Config::MASH. Rather, the
  // PrefService instance is stored by Shell when it manages to connect to the
  // pref service in Chrome.
  NOTREACHED();
  return nullptr;
}

PrefService* ShellDelegateMus::GetLocalStatePrefService() const {
  // This code should never be called in the case of Config::MASH. Rather, the
  // PrefService instance is stored by Shell when it manages to connect to the
  // pref service in Chrome.
  NOTREACHED();
  return nullptr;
}

bool ShellDelegateMus::IsTouchscreenEnabledInPrefs(bool use_local_state) const {
  NOTIMPLEMENTED();
  return true;
}

void ShellDelegateMus::SetTouchscreenEnabledInPrefs(bool enabled,
                                                    bool use_local_state) {
  NOTIMPLEMENTED();
}

void ShellDelegateMus::UpdateTouchscreenStatusFromPrefs() {
  NOTIMPLEMENTED();
}

#if defined(USE_OZONE)
ui::InputDeviceControllerClient*
ShellDelegateMus::GetInputDeviceControllerClient() {
  if (!connector_)
    return nullptr;  // Happens in tests.

  if (!input_device_controller_client_) {
    input_device_controller_client_ =
        base::MakeUnique<ui::InputDeviceControllerClient>(connector_);
  }
  return input_device_controller_client_.get();
}
#endif

}  // namespace ash
