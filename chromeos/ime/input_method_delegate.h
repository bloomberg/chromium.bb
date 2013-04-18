// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_IME_INPUT_METHOD_DELEGATE_H_
#define CHROMEOS_IME_INPUT_METHOD_DELEGATE_H_

#include <string>

#include "base/basictypes.h"
#include "base/string16.h"

namespace chromeos {
namespace input_method {

// Provides access to read/persist Input Method-related properties.
class InputMethodDelegate {
 public:
  InputMethodDelegate() {}
  virtual ~InputMethodDelegate() {}

  // Retrieves the hardware keyboard layout ID. May return an empty string if
  // the ID is unknown.
  virtual std::string GetHardwareKeyboardLayout() const = 0;

  // Retrieves localized string for |resource_id|.
  virtual string16 GetLocalizedString(int resource_id) const = 0;

  // Converts a language code to a language display name, using the
  // current application locale.
  // Examples: "fi"    => "Finnish"
  //           "en-US" => "English (United States)"
  virtual string16 GetDisplayLanguageName(
      const std::string& language_code) const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(InputMethodDelegate);
};

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROMEOS_IME_INPUT_METHOD_DELEGATE_H_
