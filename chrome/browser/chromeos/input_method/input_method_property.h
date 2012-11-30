// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_PROPERTY_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_PROPERTY_H_

#include <string>
#include <vector>

namespace chromeos {
namespace input_method {

// A structure which represents a property for an input method engine.
struct InputMethodProperty {
  InputMethodProperty(const std::string& in_key,
                      const std::string& in_label,
                      bool in_is_selection_item,
                      bool in_is_selection_item_checked);

  InputMethodProperty();
  ~InputMethodProperty();

  bool operator==(const InputMethodProperty& other) const;
  bool operator!=(const InputMethodProperty& other) const;

  // Debug print function.
  std::string ToString() const;

  std::string key;  // A key which identifies the property. Non-empty string.
                    // (e.g. "InputMode.HalfWidthKatakana")
  std::string label;  // A description of the property. Non-empty string.
                      // (e.g. "Switch to full punctuation mode", "Hiragana")
  bool is_selection_item;  // true if the property is a selection item.
  bool is_selection_item_checked;  // true if |is_selection_item| is true and
                                   // the selection_item is selected.
};
typedef std::vector<InputMethodProperty> InputMethodPropertyList;

}  // namespace input_method
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_PROPERTY_H_
