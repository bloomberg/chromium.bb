// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_status_info.h"

#include <string>

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace policy {

const char PolicyStatusInfo::kLevelDictPath[] = "level";
const char PolicyStatusInfo::kNameDictPath[] = "name";
const char PolicyStatusInfo::kScopeDictPath[] = "scope";
const char PolicyStatusInfo::kSetDictPath[] = "set";
const char PolicyStatusInfo::kStatusDictPath[] = "status";
const char PolicyStatusInfo::kValueDictPath[] = "value";

// PolicyStatusInfo
PolicyStatusInfo::PolicyStatusInfo()
    : scope(POLICY_SCOPE_USER),
      level(POLICY_LEVEL_MANDATORY),
      status(STATUS_UNDEFINED) {
}

PolicyStatusInfo::PolicyStatusInfo(
    const string16& name,
    PolicyScope scope,
    PolicyLevel level,
    Value* value,
    PolicyStatus status,
    const string16& error_message)
    : name(name),
      scope(scope),
      level(level),
      value(value),
      status(status),
      error_message(error_message) {
}

PolicyStatusInfo::~PolicyStatusInfo() {
}

DictionaryValue* PolicyStatusInfo::GetDictionaryValue() const {
  string16 level_string = GetPolicyLevelString(level);
  string16 scope_string = GetPolicyScopeString(scope);
  string16 status_message =
      status == ENFORCED ? l10n_util::GetStringUTF16(IDS_OK) : error_message;
  DictionaryValue* result = new DictionaryValue();
  result->SetString(std::string(kNameDictPath), name);
  result->SetString(std::string(kLevelDictPath), level_string);
  result->SetString(std::string(kScopeDictPath), scope_string);
  result->Set(std::string(kValueDictPath), value->DeepCopy());
  result->SetBoolean(std::string(kSetDictPath), status != STATUS_UNDEFINED);
  result->SetString(std::string(kStatusDictPath), status_message);

  return result;
}

bool PolicyStatusInfo::Equals(const PolicyStatusInfo* other_info) const {
  return name == other_info->name &&
         scope == other_info->scope &&
         level == other_info->level &&
         value->Equals(other_info->value.get()) &&
         status == other_info->status &&
         error_message == other_info->error_message;
}

// static
string16 PolicyStatusInfo::GetPolicyScopeString(PolicyScope scope) {
  static const char* strings[] = { "User", "Machine" };
  DCHECK(static_cast<size_t>(scope) < arraysize(strings));
  return ASCIIToUTF16(strings[scope]);
}

// static
string16 PolicyStatusInfo::GetPolicyLevelString(PolicyLevel level) {
  static const char* strings[] = { "Recommended", "Mandatory" };
  DCHECK(static_cast<size_t>(level) < arraysize(strings));
  return ASCIIToUTF16(strings[level]);
}

}  // namespace policy
