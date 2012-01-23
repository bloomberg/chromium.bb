// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/policy_status_info.h"

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
  DictionaryValue* result = new DictionaryValue();
  result->SetString(kNameDictPath, name);
  result->SetBoolean(kSetDictPath, status != STATUS_UNDEFINED);

  if (status == STATUS_UNDEFINED) {
    result->SetString(kLevelDictPath, "");
    result->SetString(kScopeDictPath, "");
    result->SetString(kValueDictPath, "");
    result->SetString(kStatusDictPath,
                      l10n_util::GetStringUTF16(IDS_POLICY_NOT_SET));
  } else {
    result->SetString(kLevelDictPath, GetPolicyLevelString(level));
    result->SetString(kScopeDictPath, GetPolicyScopeString(scope));
    result->Set(kValueDictPath, value->DeepCopy());
    if (status == ENFORCED)
      result->SetString(kStatusDictPath, l10n_util::GetStringUTF16(IDS_OK));
    else
      result->SetString(kStatusDictPath, error_message);
  }

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
  switch (scope) {
    case POLICY_SCOPE_USER:
      return l10n_util::GetStringUTF16(IDS_POLICY_SCOPE_USER);
    case POLICY_SCOPE_MACHINE:
      return l10n_util::GetStringUTF16(IDS_POLICY_SCOPE_MACHINE);
  }
  NOTREACHED();
  return string16();
}

// static
string16 PolicyStatusInfo::GetPolicyLevelString(PolicyLevel level) {
  switch (level) {
    case POLICY_LEVEL_RECOMMENDED:
      return l10n_util::GetStringUTF16(IDS_POLICY_LEVEL_RECOMMENDED);
    case POLICY_LEVEL_MANDATORY:
      return l10n_util::GetStringUTF16(IDS_POLICY_LEVEL_MANDATORY);
  }
  NOTREACHED();
  return string16();
}

}  // namespace policy
