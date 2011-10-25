// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_HANDLER_H_
#pragma once

#include <vector>

#include "base/basictypes.h"

class PrefValueMap;

namespace policy {

class PolicyErrorMap;
class PolicyMap;

// An abstract super class that subclasses should implement to map policies to
// their corresponding preferences, and to check whether the policies are valid.
class ConfigurationPolicyHandler {
 public:
  typedef std::vector<ConfigurationPolicyHandler*> HandlerList;

  ConfigurationPolicyHandler() {}
  virtual ~ConfigurationPolicyHandler() {}

  // Returns true if the policy settings handled by this
  // ConfigurationPolicyHandler can be applied and false otherwise. Fills
  // |errors| with error messages or warnings. |errors| may contain error
  // messages even when |CheckPolicySettings()| returns true.
  virtual bool CheckPolicySettings(const PolicyMap* policies,
                                   PolicyErrorMap* errors) = 0;

  // Processes the policies handled by this ConfigurationPolicyHandler and sets
  // the appropriate preferences in |prefs|.
  virtual void ApplyPolicySettings(const PolicyMap* policies,
                                   PrefValueMap* prefs) = 0;

  // Creates a new HandlerList with all the known handlers and returns it.
  // The new list and its elements are owned by the caller.
  static HandlerList* CreateHandlerList();

 private:
  DISALLOW_COPY_AND_ASSIGN(ConfigurationPolicyHandler);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_CONFIGURATION_POLICY_HANDLER_H_
