// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/form_field_data.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"

FormFieldData::FormFieldData()
    : max_length(0),
      is_autofilled(false),
      is_focusable(false),
      should_autocomplete(false) {
}

FormFieldData::~FormFieldData() {
}

bool FormFieldData::operator==(const FormFieldData& field) const {
  // A FormFieldData stores a value, but the value is not part of the identity
  // of the field, so we don't want to compare the values.
  return (label == field.label &&
          name == field.name &&
          form_control_type == field.form_control_type &&
          autocomplete_attribute == field.autocomplete_attribute &&
          max_length == field.max_length);
}

bool FormFieldData::operator!=(const FormFieldData& field) const {
  return !operator==(field);
}

bool FormFieldData::operator<(const FormFieldData& field) const {
  if (label == field.label)
    return name < field.name;

  return label < field.label;
}

std::ostream& operator<<(std::ostream& os, const FormFieldData& field) {
  return os
      << UTF16ToUTF8(field.label)
      << " "
      << UTF16ToUTF8(field.name)
      << " "
      << UTF16ToUTF8(field.value)
      << " "
      << field.form_control_type
      << " "
      << field.autocomplete_attribute
      << " "
      << field.max_length
      << " "
      << (field.is_autofilled ? "true" : "false")
      << " "
      << (field.is_focusable ? "true" : "false")
      << " "
      << (field.should_autocomplete ? "true" : "false");
}
