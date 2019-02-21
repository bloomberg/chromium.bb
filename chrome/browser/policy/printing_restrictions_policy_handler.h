// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_PRINTING_RESTRICTIONS_POLICY_HANDLER_H_
#define CHROME_BROWSER_POLICY_PRINTING_RESTRICTIONS_POLICY_HANDLER_H_

#include <memory>

#include "components/policy/core/browser/configuration_policy_handler.h"
#include "printing/backend/printing_restrictions.h"

class PrefValueMap;

namespace policy {

class PolicyMap;

class PrintingAllowedColorModesPolicyHandler
    : public TypeCheckingPolicyHandler {
 public:
  PrintingAllowedColorModesPolicyHandler();
  ~PrintingAllowedColorModesPolicyHandler() override;

  // ConfigurationPolicyHandler implementation:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  bool GetValue(const PolicyMap& policies,
                PolicyErrorMap* errors,
                printing::ColorModeRestriction* result);
};

class PrintingAllowedDuplexModesPolicyHandler
    : public TypeCheckingPolicyHandler {
 public:
  PrintingAllowedDuplexModesPolicyHandler();
  ~PrintingAllowedDuplexModesPolicyHandler() override;

  // ConfigurationPolicyHandler implementation:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  bool GetValue(const PolicyMap& policies,
                PolicyErrorMap* errors,
                printing::DuplexModeRestriction* result);
};

class PrintingAllowedPinModesPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  PrintingAllowedPinModesPolicyHandler();
  ~PrintingAllowedPinModesPolicyHandler() override;

  // ConfigurationPolicyHandler implementation:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  bool GetValue(const PolicyMap& policies,
                PolicyErrorMap* errors,
                printing::PinModeRestriction* result);
};

class PrintingAllowedPageSizesPolicyHandler : public ListPolicyHandler {
 public:
  PrintingAllowedPageSizesPolicyHandler();
  ~PrintingAllowedPageSizesPolicyHandler() override;

  // ListPolicyHandler implementation:
  bool CheckListEntry(const base::Value& value) override;
  void ApplyList(std::unique_ptr<base::ListValue> filtered_list,
                 PrefValueMap* prefs) override;
};

class PrintingColorDefaultPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  PrintingColorDefaultPolicyHandler();
  ~PrintingColorDefaultPolicyHandler() override;

  // ConfigurationPolicyHandler implementation:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  bool GetValue(const PolicyMap& policies,
                PolicyErrorMap* errors,
                printing::ColorModeRestriction* result);
};

class PrintingDuplexDefaultPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  PrintingDuplexDefaultPolicyHandler();
  ~PrintingDuplexDefaultPolicyHandler() override;

  // ConfigurationPolicyHandler implementation:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  bool GetValue(const PolicyMap& policies,
                PolicyErrorMap* errors,
                printing::DuplexModeRestriction* result);
};

class PrintingPinDefaultPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  PrintingPinDefaultPolicyHandler();
  ~PrintingPinDefaultPolicyHandler() override;

  // ConfigurationPolicyHandler implementation:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  bool GetValue(const PolicyMap& policies,
                PolicyErrorMap* errors,
                printing::PinModeRestriction* result);
};

class PrintingSizeDefaultPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  PrintingSizeDefaultPolicyHandler();
  ~PrintingSizeDefaultPolicyHandler() override;

  // ConfigurationPolicyHandler implementation:
  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override;
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;

 private:
  bool CheckIntSubkey(const base::Value* dict,
                      const std::string& key,
                      PolicyErrorMap* errors);
  bool GetValue(const PolicyMap& policies,
                PolicyErrorMap* errors,
                const base::Value** result);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_PRINTING_RESTRICTIONS_POLICY_HANDLER_H_
