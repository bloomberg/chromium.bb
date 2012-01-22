// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_POLICY_STATUS_INFO_H_
#define CHROME_BROWSER_POLICY_POLICY_STATUS_INFO_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/values.h"
#include "chrome/browser/policy/policy_map.h"

namespace policy {

// Describes a policy's status on the client.
struct PolicyStatusInfo {

  // Defines the possible statuses a policy can have.
  enum PolicyStatus {
    ENFORCED,
    FAILED,
    STATUS_UNDEFINED,
  };

  PolicyStatusInfo();
  PolicyStatusInfo(const string16& name,
                   PolicyScope scope,
                   PolicyLevel level,
                   Value* value,
                   PolicyStatus status,
                   const string16& error_message);
  ~PolicyStatusInfo();

  // Returns a DictionaryValue pointer containing the information in the object
  // for UI purposes. The caller acquires ownership of the returned value.
  DictionaryValue* GetDictionaryValue() const;

  // Returns true if this PolicyStatusInfo object and |other_info| have equal
  // contents and false otherwise.
  bool Equals(const PolicyStatusInfo* other_info) const;

  // Returns the string corresponding to the PolicyScope enum value |scope|.
  static string16 GetPolicyScopeString(PolicyScope scope);

  // Returns the string corresponding to the PolicyLevel enum value |level|.
  static string16 GetPolicyLevelString(PolicyLevel level);

  // The name of the policy.
  string16 name;

  // The scope of the policy (user or device).
  PolicyScope scope;

  // The level of the policy (mandatory or recommended).
  PolicyLevel level;

  // The policy value.
  scoped_ptr<Value> value;

  // The policy status (details whether the policy was successfully enforced).
  PolicyStatus status;

  // An error message in cases where the policy could not be enforced.
  string16 error_message;

  // Paths for the DictionaryValue returned by GetDictionaryValue().
  static const char kLevelDictPath[];
  static const char kNameDictPath[];
  static const char kScopeDictPath[];
  static const char kSetDictPath[];
  static const char kStatusDictPath[];
  static const char kValueDictPath[];

  DISALLOW_COPY_AND_ASSIGN(PolicyStatusInfo);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_POLICY_STATUS_INFO_H_
