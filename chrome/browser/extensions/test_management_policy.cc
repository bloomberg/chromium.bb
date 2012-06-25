// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_management_policy.h"

#include "base/utf_string_conversions.h"

namespace extensions {
TestManagementPolicyProvider::TestManagementPolicyProvider()
    : may_load_(true),
      may_modify_status_(true),
      must_remain_enabled_(false) {
  error_message_ = UTF8ToUTF16(expected_error());
}

TestManagementPolicyProvider::TestManagementPolicyProvider(
    int prohibited_actions) {
  SetProhibitedActions(prohibited_actions);
  error_message_ = UTF8ToUTF16(expected_error());
}

void TestManagementPolicyProvider::SetProhibitedActions(
    int prohibited_actions) {
  may_load_ = (prohibited_actions & PROHIBIT_LOAD) == 0;
  may_modify_status_ = (prohibited_actions & PROHIBIT_MODIFY_STATUS) == 0;
  must_remain_enabled_ = (prohibited_actions & MUST_REMAIN_ENABLED) != 0;
}

std::string TestManagementPolicyProvider::GetDebugPolicyProviderName() const {
  return "the test management policy provider";
}

bool TestManagementPolicyProvider::UserMayLoad(const Extension* extension,
                                               string16* error) const {
  if (error && !may_load_)
    *error = error_message_;
  return may_load_;
}

bool TestManagementPolicyProvider::UserMayModifySettings(
    const Extension* extension, string16* error) const {
  if (error && !may_modify_status_)
    *error = error_message_;
  return may_modify_status_;
}

bool TestManagementPolicyProvider::MustRemainEnabled(const Extension* extension,
                                                     string16* error) const {
  if (error && must_remain_enabled_)
    *error = error_message_;
  return must_remain_enabled_;
}
}  // namespace
