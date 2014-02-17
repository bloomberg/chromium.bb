// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_IME_FAKE_INPUT_METHOD_DELEGATE_H_
#define CHROMEOS_IME_FAKE_INPUT_METHOD_DELEGATE_H_

#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/ime/input_method_delegate.h"

namespace chromeos {
namespace input_method {

class CHROMEOS_EXPORT FakeInputMethodDelegate : public InputMethodDelegate {
 public:
  typedef base::Callback<base::string16 (const std::string& language_code)>
      LanguageNameLocalizationCallback;
  typedef base::Callback<base::string16 (int resource_id)>
      GetLocalizedStringCallback;

  FakeInputMethodDelegate();
  virtual ~FakeInputMethodDelegate();

  // InputMethodDelegate implementation:
  virtual std::string GetHardwareKeyboardLayouts() const OVERRIDE;
  virtual base::string16 GetLocalizedString(int resource_id) const OVERRIDE;
  virtual void SetHardwareKeyboardLayoutForTesting(
      const std::string& layout) OVERRIDE;
  virtual base::string16 GetDisplayLanguageName(
      const std::string& language_code) const OVERRIDE;

  void set_hardware_keyboard_layout(const std::string& value) {
    hardware_keyboard_layout_ = value;
  }

  void set_active_locale(const std::string& value) {
    active_locale_ = value;
  }

  void set_get_display_language_name_callback(
      const LanguageNameLocalizationCallback& callback) {
    get_display_language_name_callback_ = callback;
  }

  void set_get_localized_string_callback(
      const GetLocalizedStringCallback& callback) {
    get_localized_string_callback_ = callback;
  }

 private:
  std::string hardware_keyboard_layout_;
  std::string active_locale_;
  LanguageNameLocalizationCallback get_display_language_name_callback_;
  GetLocalizedStringCallback get_localized_string_callback_;
  DISALLOW_COPY_AND_ASSIGN(FakeInputMethodDelegate);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROMEOS_IME_FAKE_INPUT_METHOD_DELEGATE_H_
