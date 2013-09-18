// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/form_data.h"

#include "base/pickle.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/form_field_data.h"

namespace autofill {

namespace {

const int kPickleVersion = 1;

bool ReadGURL(PickleIterator* iter, GURL* url) {
  std::string spec;
  if (!iter->ReadString(&spec))
    return false;

  *url = GURL(spec);
  return true;
}

void SerializeFormFieldDataVector(const std::vector<FormFieldData> fields,
                                  Pickle* pickle) {
  pickle->WriteInt(static_cast<int>(fields.size()));
  for (size_t i = 0; i < fields.size(); ++i) {
    SerializeFormFieldData(fields[i], pickle);
  }
}

bool DeserializeFormFieldDataVector(PickleIterator* iter,
                                    std::vector<FormFieldData>* fields) {
  int size;
  if (!iter->ReadInt(&size))
    return false;

  FormFieldData temp;
  for (int i = 0; i < size; ++i) {
    if (!DeserializeFormFieldData(iter, &temp))
      return false;

    fields->push_back(temp);
  }
  return true;
}

}  // namespace

FormData::FormData()
    : user_submitted(false) {
}

FormData::FormData(const FormData& data)
    : name(data.name),
      method(data.method),
      origin(data.origin),
      action(data.action),
      user_submitted(data.user_submitted),
      fields(data.fields) {
}

FormData::~FormData() {
}

bool FormData::operator==(const FormData& form) const {
  return name == form.name &&
         StringToLowerASCII(method) == StringToLowerASCII(form.method) &&
         origin == form.origin &&
         action == form.action &&
         user_submitted == form.user_submitted &&
         fields == form.fields;
}

bool FormData::operator!=(const FormData& form) const {
  return !operator==(form);
}

std::ostream& operator<<(std::ostream& os, const FormData& form) {
  os << UTF16ToUTF8(form.name) << " "
     << UTF16ToUTF8(form.method) << " "
     << form.origin << " "
     << form.action << " "
     << form.user_submitted << " "
     << "Fields:";
  for (size_t i = 0; i < form.fields.size(); ++i) {
    os << form.fields[i] << ",";
  }
  return os;
}

void SerializeFormData(const FormData& form_data, Pickle* pickle) {
  pickle->WriteInt(kPickleVersion);
  pickle->WriteString16(form_data.name);
  pickle->WriteString16(form_data.method);
  pickle->WriteString(form_data.origin.spec());
  pickle->WriteString(form_data.action.spec());
  pickle->WriteBool(form_data.user_submitted);
  SerializeFormFieldDataVector(form_data.fields, pickle);
}

bool DeserializeFormData(PickleIterator* iter, FormData* form_data) {
  int version;
  if (!iter->ReadInt(&version)) {
    LOG(ERROR) << "Bad pickle of FormData, no version present";
    return false;
  }

  switch (version) {
    case 1: {
      if (!iter->ReadString16(&form_data->name) ||
          !iter->ReadString16(&form_data->method) ||
          !ReadGURL(iter, &form_data->origin) ||
          !ReadGURL(iter, &form_data->action) ||
          !iter->ReadBool(&form_data->user_submitted) ||
          !DeserializeFormFieldDataVector(iter, &form_data->fields)) {
        LOG(ERROR) << "Could not deserialize FormData from pickle";
        return false;
      }
      break;
    }
    default: {
      LOG(ERROR) << "Unknown FormData pickle version " << version;
      return false;
    }
  }
  return true;
}

}  // namespace autofill
