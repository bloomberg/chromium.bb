// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_POLICY_ERROR_MAP_H_
#define CHROME_BROWSER_POLICY_POLICY_ERROR_MAP_H_
#pragma once

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/string16.h"
#include "base/values.h"
#include "policy/configuration_policy_type.h"

namespace policy {

// Collects error messages and their associated policies.
class PolicyErrorMap {
 public:
  typedef std::multimap<ConfigurationPolicyType, string16> PolicyMapType;
  typedef PolicyMapType::const_iterator const_iterator;

  PolicyErrorMap();
  virtual ~PolicyErrorMap();

  // Returns true when the errors logged are ready to be retrieved. It is always
  // safe to call AddError, but the other methods are only allowed once
  // IsReady is true. IsReady will be true once the UI message loop has started.
  bool IsReady() const;

  // Adds an entry with key |policy| and the error message corresponding to
  // |message_id| in grit/generated_resources.h to the map.
  void AddError(ConfigurationPolicyType policy, int message_id);

  // Adds an entry with key |policy| and the error message corresponding to
  // |message_id| in grit/generated_resources.h to the map and replaces the
  // placeholder within the error message with |replacement_string|.
  void AddError(ConfigurationPolicyType policy,
                int message_id,
                const std::string& replacement_string);

  // Returns a list of all the error messages stored for |policy|. Returns NULL
  // if there are no error messages for |policy. The caller acquires ownership
  // of the returned ListValue pointer.
  ListValue* GetErrors(ConfigurationPolicyType policy);

  bool empty();
  size_t size();

  const_iterator begin();
  const_iterator end();

  void Clear();

 private:
  struct PendingError;

  // Maps the error when ready, otherwise adds it to the pending errors list.
  void AddError(const PendingError& error);

  // Converts a PendingError into a |map_| entry.
  void Convert(const PendingError& error);

  // Converts all pending errors to |map_| entries.
  void CheckReadyAndConvert();

  std::vector<PendingError> pending_;
  PolicyMapType map_;

  DISALLOW_COPY_AND_ASSIGN(PolicyErrorMap);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_POLICY_ERROR_MAP_H_
