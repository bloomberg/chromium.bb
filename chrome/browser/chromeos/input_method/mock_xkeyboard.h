// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_XKEYBOARD_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_XKEYBOARD_H_

#include "chrome/browser/chromeos/input_method/xkeyboard.h"

#include <string>

#include "base/compiler_specific.h"

namespace chromeos {
namespace input_method {

class MockXKeyboard : public XKeyboard {
 public:
  MockXKeyboard();
  virtual ~MockXKeyboard() {}

  virtual bool SetCurrentKeyboardLayoutByName(
      const std::string& layout_name) OVERRIDE;
  virtual bool ReapplyCurrentKeyboardLayout() OVERRIDE;
  virtual void ReapplyCurrentModifierLockStatus() OVERRIDE;
  virtual void SetLockedModifiers(
      ModifierLockStatus new_caps_lock_status,
      ModifierLockStatus new_num_lock_status) OVERRIDE;
  virtual void SetNumLockEnabled(bool enable_num_lock) OVERRIDE;
  virtual void SetCapsLockEnabled(bool enable_caps_lock) OVERRIDE;
  virtual bool NumLockIsEnabled() OVERRIDE;
  virtual bool CapsLockIsEnabled() OVERRIDE;
  virtual unsigned int GetNumLockMask() OVERRIDE;
  virtual void GetLockedModifiers(bool* out_caps_lock_enabled,
                                  bool* out_num_lock_enabled) OVERRIDE;

  int set_current_keyboard_layout_by_name_count_;
  std::string last_layout_;
  bool caps_lock_is_enabled_;
  bool num_lock_is_enabled_;
  // TODO(yusukes): Add more variables for counting the numbers of the API calls

 private:
  DISALLOW_COPY_AND_ASSIGN(MockXKeyboard);
};

// TODO(yusukes): Use the mock in
// chrome/browser/ui/ash/caps_lock_handler_browsertest.cc.

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_MOCK_XKEYBOARD_H_
