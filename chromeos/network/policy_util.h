// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_POLICY_UTIL_H_
#define CHROMEOS_NETWORK_POLICY_UTIL_H_

#include <map>
#include <string>

#include "base/memory/scoped_ptr.h"

namespace base {
class DictionaryValue;
}

namespace chromeos {

struct NetworkProfile;

namespace policy_util {

typedef std::map<std::string, const base::DictionaryValue*> GuidToPolicyMap;

// Creates a Shill property dictionary from the given arguments. The resulting
// dictionary will be sent to Shill by the caller. Depending on the profile
// type, |policy| is interpreted as the user or device policy and |settings| as
// the user or shared settings. |policy| or |settings| can be NULL, but not
// both.
scoped_ptr<base::DictionaryValue> CreateShillConfiguration(
    const NetworkProfile& profile,
    const std::string& guid,
    const base::DictionaryValue* policy,
    const base::DictionaryValue* settings);

// Returns the policy from |policies| matching |actual_network|, if any exists.
// Returns NULL otherwise. |actual_network| must be part of a ONC
// NetworkConfiguration.
const base::DictionaryValue* FindMatchingPolicy(
    const GuidToPolicyMap& policies,
    const base::DictionaryValue& actual_network);

}  // namespace policy_util

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_POLICY_UTIL_H_
