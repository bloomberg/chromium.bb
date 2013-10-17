// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_POLICY_PREF_NAMES_H_
#define COMPONENTS_POLICY_CORE_COMMON_POLICY_PREF_NAMES_H_

#include "components/policy/policy_export.h"

namespace policy {
namespace prefs {

// Constants for the names of policy-related preferences.
// TODO(dconnelly): remove POLICY_EXPORT once the statistics collector moves
// to the policy component (crbug.com/271392).
POLICY_EXPORT extern const char kLastPolicyStatisticsUpdate[];

}  // prefs
}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_POLICY_PREF_NAMES_H_
