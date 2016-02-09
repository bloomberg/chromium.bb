// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/form_field_data.h"

#include "base/pickle.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

namespace autofill {

namespace {

// Increment this anytime pickle format is modified as well as provide
// deserialization routine from previous kPickleVersion format.
const int kPickleVersion = 2;

void AddVectorToPickle(std::vector<base::string16> strings,
                       base::Pickle* pickle) {
  pickle->WriteInt(static_cast<int>(strings.size()));
  for (size_t i = 0; i < strings.size(); ++i) {
    pickle->WriteString16(strings[i]);
  }
}

bool ReadStringVector(base::PickleIterator* iter,
                      std::vector<base::string16>* strings) {
  int size;
  if (!iter->ReadInt(&size))
    return false;

  base::string16 pickle_data;
  for (int i = 0; i < size; i++) {
    if (!iter->ReadString16(&pickle_data))
      return false;

    strings->push_back(pickle_data);
  }
  return true;
}

template <typename T>
bool ReadAsInt(base::PickleIterator* iter, T* target_value) {
  int pickle_data;
  if (!iter->ReadInt(&pickle_data))
    return false;

  *target_value = static_cast<T>(pickle_data);
  return true;
}

bool DeserializeCommonSection1(base::PickleIterator* iter,
                               FormFieldData* field_data) {
  return iter->ReadString16(&field_data->label) &&
         iter->ReadString16(&field_data->name) &&
         iter->ReadString16(&field_data->value) &&
         iter->ReadString(&field_data->form_control_type) &&
         iter->ReadString(&field_data->autocomplete_attribute) &&
         iter->ReadUInt32(&field_data->max_length) &&
         iter->ReadBool(&field_data->is_autofilled) &&
         iter->ReadBool(&field_data->is_checked) &&
         iter->ReadBool(&field_data->is_checkable) &&
         iter->ReadBool(&field_data->is_focusable) &&
         iter->ReadBool(&field_data->should_autocomplete);
}

bool DeserializeCommonSection2(base::PickleIterator* iter,
                               FormFieldData* field_data) {
  return ReadAsInt(iter, &field_data->text_direction) &&
         ReadStringVector(iter, &field_data->option_values) &&
         ReadStringVector(iter, &field_data->option_contents);
}

bool DeserializeVersion2Specific(base::PickleIterator* iter,
                                 FormFieldData* field_data) {
  return ReadAsInt(iter, &field_data->role);
}

}  // namespace

FormFieldData::FormFieldData()
    : max_length(0),
      is_autofilled(false),
      is_checked(false),
      is_checkable(false),
      is_focusable(false),
      should_autocomplete(true),
      role(ROLE_ATTRIBUTE_OTHER),
      text_direction(base::i18n::UNKNOWN_DIRECTION) {
}

FormFieldData::~FormFieldData() {
}

bool FormFieldData::SameFieldAs(const FormFieldData& field) const {
  // A FormFieldData stores a value, but the value is not part of the identity
  // of the field, so we don't want to compare the values.
  return label == field.label && name == field.name &&
         form_control_type == field.form_control_type &&
         autocomplete_attribute == field.autocomplete_attribute &&
         max_length == field.max_length &&
         // is_checked and is_autofilled counts as "value" since these change
         // when we fill things in.
         is_checkable == field.is_checkable &&
         is_focusable == field.is_focusable &&
         should_autocomplete == field.should_autocomplete &&
         role == field.role && text_direction == field.text_direction;
         // The option values/contents which are the list of items in the list
         // of a drop-down are currently not considered part of the identity of
         // a form element. This is debatable, since one might base heuristics
         // on the types of elements that are available. Alternatively, one
         // could imagine some forms that dynamically change the element
         // contents (say, insert years starting from the current year) that
         // should not be considered changes in the structure of the form.
}

bool FormFieldData::operator<(const FormFieldData& field) const {
  // This does not use std::tie() as that generates more implicit variables
  // than the max-vartrack-size for var-tracking-assignments when compiling
  // for Android, producing build warnings. (See https://crbug.com/555171 for
  // context.)

  // Like operator==, this ignores the value.
  if (label < field.label) return true;
  if (label > field.label) return false;
  if (name < field.name) return true;
  if (name > field.name) return false;
  if (form_control_type < field.form_control_type) return true;
  if (form_control_type > field.form_control_type) return false;
  if (autocomplete_attribute < field.autocomplete_attribute) return true;
  if (autocomplete_attribute > field.autocomplete_attribute) return false;
  if (max_length < field.max_length) return true;
  if (max_length > field.max_length) return false;
  // Skip |is_checked| and |is_autofilled| as in SameFieldAs.
  if (is_checkable < field.is_checkable) return true;
  if (is_checkable > field.is_checkable) return false;
  if (is_focusable < field.is_focusable) return true;
  if (is_focusable > field.is_focusable) return false;
  if (should_autocomplete < field.should_autocomplete) return true;
  if (should_autocomplete > field.should_autocomplete) return false;
  if (role < field.role) return true;
  if (role > field.role) return false;
  if (text_direction < field.text_direction) return true;
  if (text_direction > field.text_direction) return false;
  // See operator== above for why we don't check option_values/contents.
  return false;
}

void SerializeFormFieldData(const FormFieldData& field_data,
                            base::Pickle* pickle) {
  pickle->WriteInt(kPickleVersion);
  pickle->WriteString16(field_data.label);
  pickle->WriteString16(field_data.name);
  pickle->WriteString16(field_data.value);
  pickle->WriteString(field_data.form_control_type);
  pickle->WriteString(field_data.autocomplete_attribute);
  pickle->WriteUInt32(field_data.max_length);
  pickle->WriteBool(field_data.is_autofilled);
  pickle->WriteBool(field_data.is_checked);
  pickle->WriteBool(field_data.is_checkable);
  pickle->WriteBool(field_data.is_focusable);
  pickle->WriteBool(field_data.should_autocomplete);
  pickle->WriteInt(field_data.role);
  pickle->WriteInt(field_data.text_direction);
  AddVectorToPickle(field_data.option_values, pickle);
  AddVectorToPickle(field_data.option_contents, pickle);
}

bool DeserializeFormFieldData(base::PickleIterator* iter,
                              FormFieldData* field_data) {
  int version;
  FormFieldData temp_form_field_data;
  if (!iter->ReadInt(&version)) {
    LOG(ERROR) << "Bad pickle of FormFieldData, no version present";
    return false;
  }

  switch (version) {
    case 1: {
      if (!DeserializeCommonSection1(iter, &temp_form_field_data) ||
          !DeserializeCommonSection2(iter, &temp_form_field_data)) {
        LOG(ERROR) << "Could not deserialize FormFieldData from pickle";
        return false;
      }
      break;
    }
    case 2: {
      if (!DeserializeCommonSection1(iter, &temp_form_field_data) ||
          !DeserializeVersion2Specific(iter, &temp_form_field_data) ||
          !DeserializeCommonSection2(iter, &temp_form_field_data)) {
        LOG(ERROR) << "Could not deserialize FormFieldData from pickle";
        return false;
      }
      break;
    }
    default: {
      LOG(ERROR) << "Unknown FormFieldData pickle version " << version;
      return false;
    }
  }
  *field_data = temp_form_field_data;
  return true;
}

std::ostream& operator<<(std::ostream& os, const FormFieldData& field) {
  return os << base::UTF16ToUTF8(field.label) << " "
            << base::UTF16ToUTF8(field.name) << " "
            << base::UTF16ToUTF8(field.value) << " " << field.form_control_type
            << " " << field.autocomplete_attribute << " " << field.max_length
            << " " << (field.is_autofilled ? "true" : "false") << " "
            << (field.is_checked ? "true" : "false") << " "
            << (field.is_checkable ? "true" : "false") << " "
            << (field.is_focusable ? "true" : "false") << " "
            << (field.should_autocomplete ? "true" : "false") << " "
            << field.role << " " << field.text_direction;
}

}  // namespace autofill
