// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test_shell_delegate.h"

#include "ash/accessibility/default_accessibility_delegate.h"
#include "ash/keyboard/test_keyboard_ui.h"
#include "ash/system/tray/system_tray_notifier.h"
#include "ash/test_screenshot_delegate.h"
#include "base/logging.h"
#include "ui/gfx/image/image.h"

namespace ash {

TestShellDelegate::TestShellDelegate() = default;

TestShellDelegate::~TestShellDelegate() = default;

bool TestShellDelegate::CanShowWindowForUser(aura::Window* window) const {
  return true;
}

std::unique_ptr<keyboard::KeyboardUI> TestShellDelegate::CreateKeyboardUI() {
  return std::make_unique<TestKeyboardUI>();
}

std::unique_ptr<ScreenshotDelegate>
TestShellDelegate::CreateScreenshotDelegate() {
  return std::make_unique<TestScreenshotDelegate>();
}

AccessibilityDelegate* TestShellDelegate::CreateAccessibilityDelegate() {
  return new DefaultAccessibilityDelegate;
}

ws::InputDeviceControllerClient*
TestShellDelegate::GetInputDeviceControllerClient() {
  return nullptr;
}

}  // namespace ash
