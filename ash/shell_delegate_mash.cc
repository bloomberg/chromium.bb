// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell_delegate_mash.h"

#include <memory>
#include <utility>

#include "ash/accessibility/default_accessibility_delegate.h"
#include "ash/screenshot_delegate.h"
#include "ash/shell.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "components/user_manager/user_info_impl.h"
#include "services/ws/public/cpp/input_devices/input_device_controller_client.h"
#include "ui/gfx/image/image.h"
#include "ui/keyboard/keyboard_ui.h"

namespace ash {
namespace {

// TODO(jamescook): Replace with a mojo-compatible ScreenshotClient.
class ScreenshotDelegateMash : public ScreenshotDelegate {
 public:
  ScreenshotDelegateMash() = default;
  ~ScreenshotDelegateMash() override = default;

  // ScreenshotDelegate:
  void HandleTakeScreenshotForAllRootWindows() override { NOTIMPLEMENTED(); }
  void HandleTakePartialScreenshot(aura::Window* window,
                                   const gfx::Rect& rect) override {
    NOTIMPLEMENTED();
  }
  void HandleTakeWindowScreenshot(aura::Window* window) override {
    NOTIMPLEMENTED();
  }
  bool CanTakeScreenshot() override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenshotDelegateMash);
};

}  // namespace

ShellDelegateMash::ShellDelegateMash() = default;

ShellDelegateMash::~ShellDelegateMash() = default;

bool ShellDelegateMash::CanShowWindowForUser(aura::Window* window) const {
  NOTIMPLEMENTED_LOG_ONCE();
  return true;
}

std::unique_ptr<keyboard::KeyboardUI> ShellDelegateMash::CreateKeyboardUI() {
  NOTIMPLEMENTED_LOG_ONCE();
  return nullptr;
}

std::unique_ptr<ScreenshotDelegate>
ShellDelegateMash::CreateScreenshotDelegate() {
  return std::make_unique<ScreenshotDelegateMash>();
}

AccessibilityDelegate* ShellDelegateMash::CreateAccessibilityDelegate() {
  return new DefaultAccessibilityDelegate;
}

ws::InputDeviceControllerClient*
ShellDelegateMash::GetInputDeviceControllerClient() {
  if (!Shell::Get()->connector())
    return nullptr;  // Happens in tests.

  if (!input_device_controller_client_) {
    input_device_controller_client_ =
        std::make_unique<ws::InputDeviceControllerClient>(
            Shell::Get()->connector());
  }
  return input_device_controller_client_.get();
}

}  // namespace ash
