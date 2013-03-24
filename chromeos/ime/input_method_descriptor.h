// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_IME_INPUT_METHOD_DESCRIPTOR_H_
#define CHROMEOS_IME_INPUT_METHOD_DESCRIPTOR_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {
namespace input_method {

class InputMethodWhitelist;

// A structure which represents an input method.
class CHROMEOS_EXPORT InputMethodDescriptor {
 public:
  InputMethodDescriptor();
  InputMethodDescriptor(const std::string& id,
                        const std::string& name,
                        const std::string& keyboard_layout,
                        const std::string& language_code,
                        bool third_party);
  ~InputMethodDescriptor();

  bool operator==(const InputMethodDescriptor& other) const;
  bool operator!=(const InputMethodDescriptor& other) const;

  // Debug print function.
  std::string ToString() const;

  const std::string& id() const { return id_; }
  const std::string& name() const { return name_; }
  const std::string& keyboard_layout() const { return keyboard_layout_; }
  const std::string& language_code() const { return language_code_; }
  bool third_party() const { return third_party_; }

  // Returns the fallback input method descriptor (the very basic US
  // keyboard). This function is mostly used for testing, but may be used
  // as the fallback, when there is no other choice.
  static InputMethodDescriptor GetFallbackInputMethodDescriptor();

 private:
  // An ID that identifies an input method engine (e.g., "t:latn-post",
  // "pinyin", "hangul").
  std::string id_;
  // A name used to specify the user-visible name of this input method.  It is
  // only used by extension IMEs, and should be blank for internal IMEs.
  std::string name_;
  // A preferred physical keyboard layout for the input method (e.g., "us",
  // "us(dvorak)", "jp"). Comma separated layout names do NOT appear.
  std::string keyboard_layout_;
  // Language code like "ko", "ja", "en-US", and "zh-CN".
  std::string language_code_;
  // Indicates if this is a third party ime
  bool third_party_;
};

typedef std::vector<InputMethodDescriptor> InputMethodDescriptors;

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROMEOS_IME_INPUT_METHOD_DESCRIPTOR_H_
