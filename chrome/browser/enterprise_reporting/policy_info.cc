// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise_reporting/policy_info.h"

#include "base/json/json_writer.h"
#include "base/optional.h"
#include "chrome/browser/policy/policy_conversions.h"
#include "components/policy/core/common/policy_types.h"

namespace enterprise_reporting {

using namespace policy;

namespace {

em::Policy_PolicyLevel GetLevel(const base::Value& policy) {
  switch (static_cast<PolicyLevel>(*policy.FindIntKey("level"))) {
    case POLICY_LEVEL_RECOMMENDED:
      return em::Policy_PolicyLevel_LEVEL_RECOMMENDED;
    case POLICY_LEVEL_MANDATORY:
      return em::Policy_PolicyLevel_LEVEL_MANDATORY;
  }
  NOTREACHED() << "Invalid policy level: " << *policy.FindIntKey("level");
  return em::Policy_PolicyLevel_LEVEL_UNKNOWN;
}

em::Policy_PolicyScope GetScope(const base::Value& policy) {
  switch (static_cast<PolicyScope>(*policy.FindIntKey("scope"))) {
    case POLICY_SCOPE_USER:
      return em::Policy_PolicyScope_SCOPE_USER;
    case POLICY_SCOPE_MACHINE:
      return em::Policy_PolicyScope_SCOPE_MACHINE;
  }
  NOTREACHED() << "Invalid policy scope: " << *policy.FindIntKey("scope");
  return em::Policy_PolicyScope_SCOPE_UNKNOWN;
}

em::Policy_PolicySource GetSource(const base::Value& policy) {
  switch (static_cast<PolicySource>(*policy.FindIntKey("source"))) {
    case POLICY_SOURCE_ENTERPRISE_DEFAULT:
      return em::Policy_PolicySource_SOURCE_ENTERPRISE_DEFAULT;
    case POLICY_SOURCE_CLOUD:
      return em::Policy_PolicySource_SOURCE_CLOUD;
    case POLICY_SOURCE_ACTIVE_DIRECTORY:
      return em::Policy_PolicySource_SOURCE_ACTIVE_DIRECTORY;
    case POLICY_SOURCE_DEVICE_LOCAL_ACCOUNT_OVERRIDE:
      return em::Policy_PolicySource_SOURCE_DEVICE_LOCAL_ACCOUNT_OVERRIDE;
    case POLICY_SOURCE_PLATFORM:
      return em::Policy_PolicySource_SOURCE_PLATFORM;
    case POLICY_SOURCE_PRIORITY_CLOUD:
      return em::Policy_PolicySource_SOURCE_PRIORITY_CLOUD;
    case POLICY_SOURCE_MERGED:
      return em::Policy_PolicySource_SOURCE_MERGED;
    case POLICY_SOURCE_COUNT:
      NOTREACHED();
      return em::Policy_PolicySource_SOURCE_UNKNOWN;
  }
  NOTREACHED() << "Invalid policy source: " << *policy.FindIntKey("source");
  return em::Policy_PolicySource_SOURCE_UNKNOWN;
}

}  // namespace

void AppendChromePolicyInfoIntoProfileReport(
    const base::Value& policies,
    em::ChromeUserProfileInfo* profile_info) {
  for (const auto& item : policies.FindKey("chromePolicies")->DictItems()) {
    const base::Value& policy = item.second;
    auto* policy_info = profile_info->add_chrome_policies();
    policy_info->set_name(item.first);
    policy_info->set_level(GetLevel(policy));
    policy_info->set_scope(GetScope(policy));
    policy_info->set_source(GetSource(policy));
    base::JSONWriter::Write(*policy.FindKey("value"),
                            policy_info->mutable_value());
    const std::string* error = policy.FindStringKey("error");
    if (error)
      policy_info->set_error(*error);
  }
}

}  // namespace enterprise_reporting
