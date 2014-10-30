// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_IME_FAKE_IME_KEYBOARD_H_
#define CHROMEOS_IME_FAKE_IME_KEYBOARD_H_

#include "chromeos/ime/ime_keyboard.h"

#include <string>

#include "base/compiler_specific.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {
namespace input_method {

class ImeKeyboardOzone : public ImeKeyboard {
 public:
  ImeKeyboardOzone();
  virtual ~ImeKeyboardOzone();

  virtual bool SetCurrentKeyboardLayoutByName(const std::string& layout_name)
      override;
  virtual bool SetAutoRepeatRate(const AutoRepeatRate& rate) override;
  virtual bool SetAutoRepeatEnabled(bool enabled) override;
  virtual bool ReapplyCurrentKeyboardLayout() override;
  virtual void ReapplyCurrentModifierLockStatus() override;
  virtual void DisableNumLock() override;
  virtual void SetCapsLockEnabled(bool enable_caps_lock) override;
  virtual bool CapsLockIsEnabled() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ImeKeyboardOzone);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROMEOS_IME_FAKE_IME_KEYBOARD_H_
