// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_VIRTUAL_KEYBOARD_DELEGATE_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_VIRTUAL_KEYBOARD_DELEGATE_H_

#include <string>

#include "base/macros.h"
#include "extensions/browser/api/virtual_keyboard_private/virtual_keyboard_delegate.h"

namespace extensions {

class ShellVirtualKeyboardDelegate : public VirtualKeyboardDelegate {
 public:
  ShellVirtualKeyboardDelegate();
  ~ShellVirtualKeyboardDelegate() override = default;

 protected:
  // VirtualKeyboardDelegate impl:
  void GetKeyboardConfig(
      OnKeyboardSettingsCallback on_settings_callback) override;
  bool HideKeyboard() override;
  bool InsertText(const base::string16& text) override;
  bool OnKeyboardLoaded() override;
  void SetHotrodKeyboard(bool enable) override;
  void SetKeyboardRestricted(bool restricted) override;
  bool LockKeyboard(bool state) override;
  bool SendKeyEvent(const std::string& type,
                    int char_value,
                    int key_code,
                    const std::string& key_name,
                    int modifiers) override;
  bool ShowLanguageSettings() override;
  bool IsLanguageSettingsEnabled() override;
  bool SetVirtualKeyboardMode(int mode_enum) override;
  bool SetRequestedKeyboardState(int state_enum) override;

 private:
  bool is_hotrod_keyboard_ = false;
  bool is_keyboard_restricted_ = false;

  DISALLOW_COPY_AND_ASSIGN(ShellVirtualKeyboardDelegate);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_VIRTUAL_KEYBOARD_DELEGATE_H_
