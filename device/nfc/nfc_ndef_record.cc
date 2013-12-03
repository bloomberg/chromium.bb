// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/nfc/nfc_ndef_record.h"

#include <map>

#include "base/logging.h"

using base::DictionaryValue;
using base::ListValue;

namespace device {

namespace {

typedef std::map<std::string, base::Value::Type> FieldValueMap;

bool CheckFieldsAreValid(
    const FieldValueMap& required_fields,
    const FieldValueMap& optional_fields,
    const DictionaryValue* data) {
  size_t required_count = 0;
  for (DictionaryValue::Iterator iter(*data);
       !iter.IsAtEnd(); iter.Advance()) {
    FieldValueMap::const_iterator field_iter =
        required_fields.find(iter.key());
    if (field_iter == required_fields.end()) {
      // Field wasn't one of the required fields. Check if optional.
      field_iter = optional_fields.find(iter.key());

      if (field_iter == optional_fields.end()) {
        // If the field isn't one of the optional fields either, then it's
        // invalid.
        VLOG(1) << "Tried to populate record with invalid field: "
                << iter.key();
        return false;
      }
    } else {
      required_count++;
    }
    // The field is invalid, if the type of its value is incorrect.
    if (field_iter->second != iter.value().GetType()) {
      VLOG(1) << "Provided value for field \"" << iter.key() << "\" has type "
              << iter.value().GetType() << ", expected: "
              << field_iter->second;
      return false;
    }
  }
  // Check for required fields.
  if (required_count != required_fields.size()) {
    VLOG(1) << "Provided data did not contain all required fields for "
            << "requested NDEF type.";
    return false;
  }
  return true;
}

// Verifies that the contents of |data| conform to the fields of NDEF type
// "Text".
bool HandleTypeText(const DictionaryValue* data) {
  VLOG(1) << "Populating record with type \"Text\".";
  FieldValueMap required_fields;
  required_fields[NfcNdefRecord::kFieldText] = base::Value::TYPE_STRING;
  FieldValueMap optional_fields;
  optional_fields[NfcNdefRecord::kFieldEncoding] = base::Value::TYPE_STRING;
  optional_fields[NfcNdefRecord::kFieldLanguageCode] = base::Value::TYPE_STRING;
  if (!CheckFieldsAreValid(required_fields, optional_fields, data)) {
    VLOG(1) << "Failed to populate record.";
    return false;
  }
  return true;
}

// Verifies that the contents of |data| conform to the fields of NDEF type
// "SmartPoster".
bool HandleTypeSmartPoster(const DictionaryValue* data) {
  VLOG(1) << "Populating record with type \"SmartPoster\".";
  FieldValueMap required_fields;
  required_fields[NfcNdefRecord::kFieldURI] = base::Value::TYPE_STRING;
  FieldValueMap optional_fields;
  optional_fields[NfcNdefRecord::kFieldAction] = base::Value::TYPE_STRING;
  optional_fields[NfcNdefRecord::kFieldMimeType] = base::Value::TYPE_STRING;
  // base::Value restricts the number types to BOOL, INTEGER, and DOUBLE only.
  // uint32 will automatically get converted to a double. "target size" is
  // really a uint32 but we define it as a double for this reason.
  // (See dbus/values_util.h).
  optional_fields[NfcNdefRecord::kFieldTargetSize] = base::Value::TYPE_DOUBLE;
  optional_fields[NfcNdefRecord::kFieldTitles] = base::Value::TYPE_LIST;
  if (!CheckFieldsAreValid(required_fields, optional_fields, data)) {
    VLOG(1) << "Failed to populate record.";
    return false;
  }
  // Verify that the "titles" field was formatted correctly, if it exists.
  const ListValue* titles = NULL;
  if (data->GetList(NfcNdefRecord::kFieldTitles, &titles)) {
    if (titles->empty()) {
      VLOG(1) << "\"titles\" field of SmartPoster is empty.";
      return false;
    }
    for (ListValue::const_iterator iter = titles->begin();
         iter != titles->end(); ++iter) {
      const DictionaryValue* title_data = NULL;
      if (!(*iter)->GetAsDictionary(&title_data)) {
        VLOG(1) << "\"title\" entry for SmartPoster contains an invalid value "
                << "type";
        return false;
      }
      if (!HandleTypeText(title_data)) {
        VLOG(1) << "Badly formatted \"title\" entry for SmartPoster.";
        return false;
      }
    }
  }
  return true;
}

// Verifies that the contents of |data| conform to the fields of NDEF type
// "URI".
bool HandleTypeUri(const DictionaryValue* data) {
  VLOG(1) << "Populating record with type \"URI\".";
  FieldValueMap required_fields;
  required_fields[NfcNdefRecord::kFieldURI] = base::Value::TYPE_STRING;
  FieldValueMap optional_fields;
  optional_fields[NfcNdefRecord::kFieldMimeType] = base::Value::TYPE_STRING;
  optional_fields[NfcNdefRecord::kFieldTargetSize] = base::Value::TYPE_DOUBLE;
  if (!CheckFieldsAreValid(required_fields, optional_fields, data)) {
    VLOG(1) << "Failed to populate record.";
    return false;
  }
  return true;
}

}  // namespace

// static
const char NfcNdefRecord::kFieldEncoding[] = "encoding";
// static
const char NfcNdefRecord::kFieldLanguageCode[] = "languageCode";
// static
const char NfcNdefRecord::kFieldText[] = "text";
// static
const char NfcNdefRecord::kFieldURI[] = "uri";
// static
const char NfcNdefRecord::kFieldMimeType[] = "mimeType";
// static
const char NfcNdefRecord::kFieldTargetSize[] = "targetSize";
// static
const char NfcNdefRecord::kFieldTitles[] = "titles";
// static
const char NfcNdefRecord::kFieldAction[] = "action";
// static
const char NfcNdefRecord::kEncodingUtf8[] = "UTF-8";
// static
const char NfcNdefRecord::kEncodingUtf16[] = "UTF-16";
// static
const char NfcNdefRecord::kSmartPosterActionDo[] = "do";
// static
const char NfcNdefRecord::kSmartPosterActionSave[] = "save";
// static
const char NfcNdefRecord::kSmartPosterActionOpen[] = "open";

NfcNdefRecord::NfcNdefRecord() : type_(kTypeUnknown) {
}

NfcNdefRecord::~NfcNdefRecord() {
}

bool NfcNdefRecord::IsPopulated() const {
  return type_ != kTypeUnknown;
}

bool NfcNdefRecord::Populate(Type type, const DictionaryValue* data) {
  if (IsPopulated())
    return false;

  DCHECK(data_.empty());

  // At this time, only "Text", "URI", and "SmartPoster" are supported.
  bool result = false;
  switch (type) {
    case kTypeText:
      result = HandleTypeText(data);
      break;
    case kTypeSmartPoster:
      result = HandleTypeSmartPoster(data);
      break;
    case kTypeURI:
      result = HandleTypeUri(data);
      break;
    default:
      VLOG(1) << "Unsupported NDEF type: " << type;
      break;
  }
  if (!result)
    return false;
  type_ = type;
  data_.MergeDictionary(data);
  return true;
}

NfcNdefMessage::NfcNdefMessage() {
}

NfcNdefMessage::~NfcNdefMessage() {
}

void NfcNdefMessage::AddRecord(NfcNdefRecord* record) {
  records_.push_back(record);
}

}  // namespace device
