// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_status_info.h"

#include <algorithm>

#include "base/logging.h"
#include "base/utf_string_conversions.h"

namespace policy {

const std::string PolicyStatusInfo::kLevelDictPath = "level";
const std::string PolicyStatusInfo::kNameDictPath = "name";
const std::string PolicyStatusInfo::kSetDictPath = "set";
const std::string PolicyStatusInfo::kSourceTypeDictPath = "sourceType";
const std::string PolicyStatusInfo::kStatusDictPath = "status";
const std::string PolicyStatusInfo::kValueDictPath = "value";

// PolicyStatusInfo
PolicyStatusInfo::PolicyStatusInfo() {
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
  result->SetString(kNameDictPath, name);
  result->SetString(kLevelDictPath, level_string);
  result->SetString(kSourceTypeDictPath, source_type_string);
  result->Set(kValueDictPath, value->DeepCopy());
  result->SetBoolean(kSetDictPath, level != LEVEL_UNDEFINED);
  result->SetString(kStatusDictPath, status_message);

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
string16 PolicyStatusInfo::GetSourceTypeString(PolicySourceType source_type) {
  static string16 strings[] = { ASCIIToUTF16("user"),
                                ASCIIToUTF16("device"),
                                ASCIIToUTF16("undefined") };
  DCHECK(static_cast<size_t>(source_type) < arraysize(strings));
  return strings[source_type];
}

// static
string16 PolicyStatusInfo::GetPolicyLevelString(PolicyLevel level) {
  static string16 strings[] = { ASCIIToUTF16("mandatory"),
                                ASCIIToUTF16("recommended"),
                                ASCIIToUTF16("undefined") };
  DCHECK(static_cast<size_t>(level) < arraysize(strings));
  return strings[level];
}

} // namespace policy
