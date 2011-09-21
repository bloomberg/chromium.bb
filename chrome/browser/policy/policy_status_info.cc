// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_status_info.h"

#include <string>

#include "base/logging.h"
#include "base/utf_string_conversions.h"

namespace policy {

const char PolicyStatusInfo::kLevelDictPath[] = "level";
const char PolicyStatusInfo::kNameDictPath[] = "name";
const char PolicyStatusInfo::kSetDictPath[] = "set";
const char PolicyStatusInfo::kSourceTypeDictPath[] = "sourceType";
const char PolicyStatusInfo::kStatusDictPath[] = "status";
const char PolicyStatusInfo::kValueDictPath[] = "value";

// PolicyStatusInfo
PolicyStatusInfo::PolicyStatusInfo()
    : source_type(SOURCE_TYPE_UNDEFINED),
      level(LEVEL_UNDEFINED),
      status(STATUS_UNDEFINED) {
}

PolicyStatusInfo::PolicyStatusInfo(
    string16 name,
    PolicySourceType source_type,
    PolicyLevel level,
    Value* value,
    PolicyStatus status,
    string16 error_message)
    : name(name),
      source_type(source_type),
      level(level),
      value(value),
      status(status),
      error_message(error_message) {
}

PolicyStatusInfo::~PolicyStatusInfo() {
}

DictionaryValue* PolicyStatusInfo::GetDictionaryValue() const {
  string16 level_string = GetPolicyLevelString(level);
  string16 source_type_string = GetSourceTypeString(source_type);
  string16 status_message = status == ENFORCED ? ASCIIToUTF16("ok")
                                               : error_message;
  DictionaryValue* result = new DictionaryValue();
  result->SetString(std::string(kNameDictPath), name);
  result->SetString(std::string(kLevelDictPath), level_string);
  result->SetString(std::string(kSourceTypeDictPath), source_type_string);
  result->Set(std::string(kValueDictPath), value->DeepCopy());
  result->SetBoolean(std::string(kSetDictPath), level != LEVEL_UNDEFINED);
  result->SetString(std::string(kStatusDictPath), status_message);

  return result;
}

bool PolicyStatusInfo::Equals(const PolicyStatusInfo* other_info) const {
  return name == other_info->name &&
         source_type == other_info->source_type &&
         level == other_info->level &&
         value->Equals(other_info->value.get()) &&
         status == other_info->status &&
         error_message == other_info->error_message;
}

// static
string16 PolicyStatusInfo::GetSourceTypeString(
    PolicySourceType source_type) {
  static const char* strings[] = { "user", "device", "undefined" };
  DCHECK(static_cast<size_t>(source_type) < arraysize(strings));
  return ASCIIToUTF16(strings[source_type]);
}

// static
string16 PolicyStatusInfo::GetPolicyLevelString(PolicyLevel level) {
  static const char* strings[] = { "mandatory", "recommended", "undefined" };
  DCHECK(static_cast<size_t>(level) < arraysize(strings));
  return ASCIIToUTF16(strings[level]);
}

}  // namespace policy
