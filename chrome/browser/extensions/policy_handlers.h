// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_POLICY_HANDLERS_H_
#define CHROME_BROWSER_EXTENSIONS_POLICY_HANDLERS_H_

#include <memory>

#include "base/macros.h"
#include "base/values.h"
#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {
class PolicyMap;
class PolicyErrorMap;
class Schema;
}  // namespace policy

namespace extensions {

// Implements additional checks for policies that are lists of extension IDs.
class ExtensionListPolicyHandler
    : public policy::TypeCheckingPolicyHandler {
 public:
  ExtensionListPolicyHandler(const char* policy_name,
                             const char* pref_path,
                             bool allow_wildcards);
  ~ExtensionListPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;

 protected:
  const char* pref_path() const;

  // Runs sanity checks on the policy value and returns it in |extension_ids|.
  bool CheckAndGetList(const policy::PolicyMap& policies,
                       policy::PolicyErrorMap* errors,
                       std::unique_ptr<base::ListValue>* extension_ids);

 private:
  const char* pref_path_;
  bool allow_wildcards_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionListPolicyHandler);
};

class ExtensionInstallForcelistPolicyHandler
    : public policy::TypeCheckingPolicyHandler {
 public:
  ExtensionInstallForcelistPolicyHandler();
  ~ExtensionInstallForcelistPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;

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
  ~ExtensionURLPatternListPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  const char* pref_path_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionURLPatternListPolicyHandler);
};

class ExtensionSettingsPolicyHandler
    : public policy::SchemaValidatingPolicyHandler {
 public:
  explicit ExtensionSettingsPolicyHandler(const policy::Schema& chrome_schema);
  ~ExtensionSettingsPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  bool CheckPolicySettings(const policy::PolicyMap& policies,
                           policy::PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionSettingsPolicyHandler);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_POLICY_HANDLERS_H_
