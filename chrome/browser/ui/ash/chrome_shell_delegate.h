// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CHROME_SHELL_DELEGATE_H_
#define CHROME_BROWSER_UI_ASH_CHROME_SHELL_DELEGATE_H_

#include <memory>
#include <string>

#include "ash/shell_delegate.h"
#include "base/macros.h"
#include "build/build_config.h"

namespace keyboard {
class KeyboardUI;
}

class ChromeShellDelegate : public ash::ShellDelegate {
 public:
  ChromeShellDelegate();
  ~ChromeShellDelegate() override;

  // ash::ShellDelegate overrides;
  bool CanShowWindowForUser(aura::Window* window) const override;
  std::unique_ptr<keyboard::KeyboardUI> CreateKeyboardUI() override;
  std::unique_ptr<ash::ScreenshotDelegate> CreateScreenshotDelegate() override;
  ash::AccessibilityDelegate* CreateAccessibilityDelegate() override;
  void OpenKeyboardShortcutHelpPage() const override;
  ui::InputDeviceControllerClient* GetInputDeviceControllerClient() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeShellDelegate);
};

#endif  // CHROME_BROWSER_UI_ASH_CHROME_SHELL_DELEGATE_H_
