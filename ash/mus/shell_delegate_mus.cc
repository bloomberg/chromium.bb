// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/mus/shell_delegate_mus.h"

#include <memory>
#include <utility>

#include "ash/accessibility/default_accessibility_delegate.h"
#include "ash/gpu_support_stub.h"
#include "ash/mus/wallpaper_delegate_mus.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "components/user_manager/user_info_impl.h"
#include "services/ui/public/cpp/input_devices/input_device_controller_client.h"
#include "ui/gfx/image/image.h"
#include "ui/keyboard/keyboard_ui.h"

namespace ash {

ShellDelegateMus::ShellDelegateMus(service_manager::Connector* connector)
    : connector_(connector) {}

ShellDelegateMus::~ShellDelegateMus() {}

service_manager::Connector* ShellDelegateMus::GetShellConnector() const {
  return connector_;
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

std::unique_ptr<keyboard::KeyboardUI> ShellDelegateMus::CreateKeyboardUI() {
  NOTIMPLEMENTED();
  return nullptr;
}

void ShellDelegateMus::OpenUrlFromArc(const GURL& url) {
  NOTIMPLEMENTED();
}

NetworkingConfigDelegate* ShellDelegateMus::GetNetworkingConfigDelegate() {
  // TODO(mash): Provide a real implementation, perhaps by folding its behavior
  // into an ash-side network information cache. http://crbug.com/651157
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<WallpaperDelegate> ShellDelegateMus::CreateWallpaperDelegate() {
  return std::make_unique<WallpaperDelegateMus>();
}

AccessibilityDelegate* ShellDelegateMus::CreateAccessibilityDelegate() {
  return new DefaultAccessibilityDelegate;
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

ui::InputDeviceControllerClient*
ShellDelegateMus::GetInputDeviceControllerClient() {
  if (!connector_)
    return nullptr;  // Happens in tests.

  if (!input_device_controller_client_) {
    input_device_controller_client_ =
        std::make_unique<ui::InputDeviceControllerClient>(connector_);
  }
  return input_device_controller_client_.get();
}

}  // namespace ash
