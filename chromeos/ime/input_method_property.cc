// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ime/input_method_property.h"

#include <sstream>

#include "base/logging.h"

namespace chromeos {
namespace input_method {

InputMethodProperty::InputMethodProperty(const std::string& in_key,
                                         const std::string& in_label,
                                         bool in_is_selection_item,
                                         bool in_is_selection_item_checked)
    : key(in_key),
      label(in_label),
      is_selection_item(in_is_selection_item),
      is_selection_item_checked(in_is_selection_item_checked) {
  DCHECK(!key.empty());
}

InputMethodProperty::InputMethodProperty()
    : is_selection_item(false),
      is_selection_item_checked(false) {
}

InputMethodProperty::~InputMethodProperty() {
}

bool InputMethodProperty::operator==(const InputMethodProperty& other) const {
  return key == other.key &&
      label == other.label &&
      is_selection_item == other.is_selection_item &&
      is_selection_item_checked == other.is_selection_item_checked;
}

bool InputMethodProperty::operator!=(const InputMethodProperty& other) const {
  return !(*this == other);
}

std::string InputMethodProperty::ToString() const {
  std::stringstream stream;
  stream << "key=" << key
         << ", label=" << label
         << ", is_selection_item=" << is_selection_item
         << ", is_selection_item_checked=" << is_selection_item_checked;
  return stream.str();
}

}  // namespace input_method
}  // namespace chromeos
