// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_IME_FAKE_IME_KEYBOARD_H_
#define CHROMEOS_IME_FAKE_IME_KEYBOARD_H_

#include "chromeos/ime/ime_keyboard.h"

#include <string>

#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {
namespace input_method {

class CHROMEOS_EXPORT FakeImeKeyboard : public ImeKeyboard {
 public:
  FakeImeKeyboard();
  virtual ~FakeImeKeyboard();

  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual bool SetCurrentKeyboardLayoutByName(const std::string& layout_name)
      OVERRIDE;
  virtual bool ReapplyCurrentKeyboardLayout() OVERRIDE;
  virtual void ReapplyCurrentModifierLockStatus() OVERRIDE;
  virtual void DisableNumLock() OVERRIDE;
  virtual void SetCapsLockEnabled(bool enable_caps_lock) OVERRIDE;
  virtual bool CapsLockIsEnabled() OVERRIDE;
  virtual bool IsISOLevel5ShiftAvailable() const OVERRIDE;
  virtual bool IsAltGrAvailable() const OVERRIDE;
  virtual bool SetAutoRepeatEnabled(bool enabled) OVERRIDE;
  virtual bool SetAutoRepeatRate(const AutoRepeatRate& rate) OVERRIDE;

  int set_current_keyboard_layout_by_name_count_;
  std::string last_layout_;
  bool caps_lock_is_enabled_;
  bool auto_repeat_is_enabled_;
  AutoRepeatRate last_auto_repeat_rate_;
  // TODO(yusukes): Add more variables for counting the numbers of the API calls

 private:
  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(FakeImeKeyboard);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROMEOS_IME_FAKE_IME_KEYBOARD_H_
