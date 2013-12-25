// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/form_field_data.h"

#include "base/pickle.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

namespace {

const int kPickleVersion = 1;

void AddVectorToPickle(std::vector<base::string16> strings,
                       Pickle* pickle) {
  pickle->WriteInt(static_cast<int>(strings.size()));
  for (size_t i = 0; i < strings.size(); ++i) {
    pickle->WriteString16(strings[i]);
  }
}

bool ReadStringVector(PickleIterator* iter,
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

bool ReadTextDirection(PickleIterator* iter,
                       base::i18n::TextDirection* direction) {
  int pickle_data;
  if (!iter->ReadInt(&pickle_data))
    return false;

  *direction = static_cast<base::i18n::TextDirection>(pickle_data);
  return true;
}

bool ReadSize(PickleIterator* iter, size_t* size) {
  uint64 pickle_data;
  if (!iter->ReadUInt64(&pickle_data))
    return false;

  *size = static_cast<size_t>(pickle_data);
  return true;
}

}  // namespace

namespace autofill {

FormFieldData::FormFieldData()
    : max_length(0),
      is_autofilled(false),
      is_checked(false),
      is_checkable(false),
      is_focusable(false),
      should_autocomplete(true),
      text_direction(base::i18n::UNKNOWN_DIRECTION) {
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

void SerializeFormFieldData(const FormFieldData& field_data,
                            Pickle* pickle) {
  pickle->WriteInt(kPickleVersion);
  pickle->WriteString16(field_data.label);
  pickle->WriteString16(field_data.name);
  pickle->WriteString16(field_data.value);
  pickle->WriteString(field_data.form_control_type);
  pickle->WriteString(field_data.autocomplete_attribute);
  pickle->WriteUInt64(static_cast<uint64>(field_data.max_length));
  pickle->WriteBool(field_data.is_autofilled);
  pickle->WriteBool(field_data.is_checked);
  pickle->WriteBool(field_data.is_checkable);
  pickle->WriteBool(field_data.is_focusable);
  pickle->WriteBool(field_data.should_autocomplete);
  pickle->WriteInt(field_data.text_direction);
  AddVectorToPickle(field_data.option_values, pickle);
  AddVectorToPickle(field_data.option_contents, pickle);
}

bool DeserializeFormFieldData(PickleIterator* iter,
                              FormFieldData* field_data) {
  int version;
  if (!iter->ReadInt(&version)) {
    LOG(ERROR) << "Bad pickle of FormFieldData, no version present";
    return false;
  }

  switch (version) {
    case 1: {
      if (!iter->ReadString16(&field_data->label) ||
          !iter->ReadString16(&field_data->name) ||
          !iter->ReadString16(&field_data->value) ||
          !iter->ReadString(&field_data->form_control_type) ||
          !iter->ReadString(&field_data->autocomplete_attribute) ||
          !ReadSize(iter, &field_data->max_length) ||
          !iter->ReadBool(&field_data->is_autofilled) ||
          !iter->ReadBool(&field_data->is_checked) ||
          !iter->ReadBool(&field_data->is_checkable) ||
          !iter->ReadBool(&field_data->is_focusable) ||
          !iter->ReadBool(&field_data->should_autocomplete) ||
          !ReadTextDirection(iter, &field_data->text_direction) ||
          !ReadStringVector(iter, &field_data->option_values) ||
          !ReadStringVector(iter, &field_data->option_contents)) {
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
  return true;
}

std::ostream& operator<<(std::ostream& os, const FormFieldData& field) {
  return os
      << base::UTF16ToUTF8(field.label)
      << " "
      << base::UTF16ToUTF8(field.name)
      << " "
      << base::UTF16ToUTF8(field.value)
      << " "
      << field.form_control_type
      << " "
      << field.autocomplete_attribute
      << " "
      << field.max_length
      << " "
      << (field.is_autofilled ? "true" : "false")
      << " "
      << (field.is_checked ? "true" : "false")
      << " "
      << (field.is_checkable ? "true" : "false")
      << " "
      << (field.is_focusable ? "true" : "false")
      << " "
      << (field.should_autocomplete ? "true" : "false")
      << " "
      << field.text_direction;
}

}  // namespace autofill
