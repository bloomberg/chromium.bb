// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_POLICY_HANDLERS_H_
#define CHROME_BROWSER_EXTENSIONS_POLICY_HANDLERS_H_

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {
class PolicyMap;
class PolicyErrorMap;
}  // namespace policy

namespace extensions {

// Implements additional checks for policies that are lists of extension IDs.
class ExtensionListPolicyHandler
    : public policy::TypeCheckingPolicyHandler {
 public:
  ExtensionListPolicyHandler(const char* policy_name,
                             const char* pref_path,
                             bool allow_wildcards);
  virtual ~ExtensionListPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual bool CheckPolicySettings(const policy::PolicyMap& policies,
                                   policy::PolicyErrorMap* errors) OVERRIDE;
  virtual void ApplyPolicySettings(const policy::PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 protected:
  const char* pref_path() const;

  // Runs sanity checks on the policy value and returns it in |extension_ids|.
  bool CheckAndGetList(const policy::PolicyMap& policies,
                       policy::PolicyErrorMap* errors,
                       scoped_ptr<base::ListValue>* extension_ids);

 private:
  const char* pref_path_;
  bool allow_wildcards_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionListPolicyHandler);
};

class ExtensionInstallForcelistPolicyHandler
    : public policy::TypeCheckingPolicyHandler {
 public:
  ExtensionInstallForcelistPolicyHandler();
  virtual ~ExtensionInstallForcelistPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual bool CheckPolicySettings(const policy::PolicyMap& policies,
                                   policy::PolicyErrorMap* errors) OVERRIDE;
  virtual void ApplyPolicySettings(const policy::PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  // Parses the data in |policy_value| and writes them to |extension_dict|.
  bool ParseList(const base::Value* policy_value,
                 base::DictionaryValue* extension_dict,
                 policy::PolicyErrorMap* errors);

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstallForcelistPolicyHandler);
};

// Implements additional checks for policies that are lists of extension
// URLPatterns.
class ExtensionURLPatternListPolicyHandler
    : public policy::TypeCheckingPolicyHandler {
 public:
  ExtensionURLPatternListPolicyHandler(const char* policy_name,
                                       const char* pref_path);
  virtual ~ExtensionURLPatternListPolicyHandler();

  // ConfigurationPolicyHandler methods:
  virtual bool CheckPolicySettings(const policy::PolicyMap& policies,
                                   policy::PolicyErrorMap* errors) OVERRIDE;
  virtual void ApplyPolicySettings(const policy::PolicyMap& policies,
                                   PrefValueMap* prefs) OVERRIDE;

 private:
  const char* pref_path_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionURLPatternListPolicyHandler);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_POLICY_HANDLERS_H_
