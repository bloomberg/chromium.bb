// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/configuration_policy_handler_list.h"

#include "base/prefs/pref_value_map.h"
#include "base/stl_util.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/configuration_policy_handler_parameters.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "grit/components_strings.h"

namespace policy {
ConfigurationPolicyHandlerList::ConfigurationPolicyHandlerList(
    const PopulatePolicyHandlerParametersCallback& parameters_callback,
    const GetChromePolicyDetailsCallback& details_callback)
    : parameters_callback_(parameters_callback),
      details_callback_(details_callback) {}

ConfigurationPolicyHandlerList::~ConfigurationPolicyHandlerList() {
  STLDeleteElements(&handlers_);
}

void ConfigurationPolicyHandlerList::AddHandler(
    scoped_ptr<ConfigurationPolicyHandler> handler) {
  handlers_.push_back(handler.release());
}

void ConfigurationPolicyHandlerList::ApplyPolicySettings(
    const PolicyMap& policies,
    PrefValueMap* prefs,
    PolicyErrorMap* errors) const {
  PolicyErrorMap scoped_errors;
  if (!errors)
    errors = &scoped_errors;

  policy::PolicyHandlerParameters parameters;
  parameters_callback_.Run(&parameters);

  std::vector<ConfigurationPolicyHandler*>::const_iterator handler;
  for (handler = handlers_.begin(); handler != handlers_.end(); ++handler) {
    if ((*handler)->CheckPolicySettings(policies, errors) && prefs)
      (*handler)
          ->ApplyPolicySettingsWithParameters(policies, parameters, prefs);
  }

  for (PolicyMap::const_iterator it = policies.begin();
       it != policies.end();
       ++it) {
    const PolicyDetails* details =
        details_callback_.is_null() ? NULL : details_callback_.Run(it->first);
    if (details && details->is_deprecated)
      errors->AddError(it->first, IDS_POLICY_DEPRECATED);
  }
}

void ConfigurationPolicyHandlerList::PrepareForDisplaying(
    PolicyMap* policies) const {
  std::vector<ConfigurationPolicyHandler*>::const_iterator handler;
  for (handler = handlers_.begin(); handler != handlers_.end(); ++handler)
    (*handler)->PrepareForDisplaying(policies);
}

}  // namespace policy
