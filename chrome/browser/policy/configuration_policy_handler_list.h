// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_HANDLER_LIST_H_
#define CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_HANDLER_LIST_H_

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

class PrefValueMap;

namespace policy {

class ConfigurationPolicyHandler;
class PolicyErrorMap;
class PolicyMap;
struct PolicyToPreferenceMapEntry;

// Converts policies to their corresponding preferences by applying a list of
// ConfigurationPolicyHandler objects. This includes error checking and
// cleaning up policy values for displaying.
class ConfigurationPolicyHandlerList {
 public:
  ConfigurationPolicyHandlerList();
  ~ConfigurationPolicyHandlerList();

  // Adds a policy handler to the list.
  void AddHandler(scoped_ptr<ConfigurationPolicyHandler> handler);

  // Translates |policies| to their corresponding preferences in |prefs|.
  // Any errors found while processing the policies are stored in |errors|.
  // |prefs| or |errors| can be NULL, and won't be filled in that case.
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs,
                           PolicyErrorMap* errors) const;

  // Converts sensitive policy values to others more appropriate for displaying.
  void PrepareForDisplaying(PolicyMap* policies) const;

 private:
  std::vector<ConfigurationPolicyHandler*> handlers_;

  DISALLOW_COPY_AND_ASSIGN(ConfigurationPolicyHandlerList);
};

// Builds a platform-specific handler list.
scoped_ptr<ConfigurationPolicyHandlerList> BuildHandlerList();

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_HANDLER_LIST_H_
