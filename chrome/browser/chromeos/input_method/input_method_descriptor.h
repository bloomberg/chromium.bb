// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_DESCRIPTOR_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_DESCRIPTOR_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"

namespace chromeos {
namespace input_method {

class InputMethodWhitelist;

// A structure which represents an input method.
class InputMethodDescriptor {
 public:
  InputMethodDescriptor();
  InputMethodDescriptor(const InputMethodWhitelist& whitelist,
                        const std::string& in_id,
                        const std::string& in_name,
                        const std::string& in_raw_layout,
                        const std::string& in_language_code);
  ~InputMethodDescriptor();

  bool operator==(const InputMethodDescriptor& other) const;
  bool operator!=(const InputMethodDescriptor& other) const;

  // Debug print function.
  std::string ToString() const;

  const std::string& id() const { return id_; }
  const std::string& name() const { return name_; }
  const std::string& keyboard_layout() const { return keyboard_layout_; }
  const std::vector<std::string>& virtual_keyboard_layouts() const {
    return virtual_keyboard_layouts_;
  }
  const std::string& language_code() const { return language_code_; }

  // Returns the fallback input method descriptor (the very basic US
  // keyboard). This function is mostly used for testing, but may be used
  // as the fallback, when there is no other choice.
  static InputMethodDescriptor GetFallbackInputMethodDescriptor();

 private:
  // For GetFallbackInputMethodDescriptor(). Use the public constructor instead.
  InputMethodDescriptor(const std::string& in_id,
                        const std::string& in_name,
                        const std::string& in_keyboard_layout,
                        const std::string& in_virtual_keyboard_layouts,
                        const std::string& in_language_code);

  // An ID that identifies an input method engine (e.g., "t:latn-post",
  // "pinyin", "hangul").
  std::string id_;
  // A name used to specify the user-visible name of this input method.  It is
  // only used by extension IMEs, and should be blank for internal IMEs.
  std::string name_;
  // A preferred physical keyboard layout for the input method (e.g., "us",
  // "us(dvorak)", "jp"). Comma separated layout names do NOT appear.
  std::string keyboard_layout_;
  // Preferred virtual keyboard layouts for the input method. Comma separated
  // layout names in order of priority, such as "handwriting,us", could appear.
  std::vector<std::string> virtual_keyboard_layouts_;
  // Language code like "ko", "ja", "en-US", and "zh-CN".
  std::string language_code_;
};

typedef std::vector<InputMethodDescriptor> InputMethodDescriptors;

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_DESCRIPTOR_H_
