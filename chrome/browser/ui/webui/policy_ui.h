// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_POLICY_UI_H_
#define CHROME_BROWSER_UI_WEBUI_POLICY_UI_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "base/values.h"
#include "chrome/browser/policy/cloud_policy_subsystem.h"
#include "chrome/browser/policy/policy_map.h"
#include "chrome/browser/policy/policy_service.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "policy/policy_constants.h"

namespace policy {
class CloudPolicyDataStore;
}

// The base class handler of Javascript messages of the about:policy page.
class PolicyUIHandler : public content::WebUIMessageHandler,
                        public policy::PolicyService::Observer {
 public:
  // Keys expected in a DictionaryValue representing the status of a policy.
  // Public for testing.
  static const char kLevel[];
  static const char kName[];
  static const char kScope[];
  static const char kSet[];
  static const char kStatus[];
  static const char kValue[];

  PolicyUIHandler();
  virtual ~PolicyUIHandler();

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // policy::PolicyService::Observer implementation.
  virtual void OnPolicyUpdated(policy::PolicyDomain domain,
                               const std::string& component_id,
                               const policy::PolicyMap& previous,
                               const policy::PolicyMap& current) OVERRIDE;

  // Returns a ListValue pointer containing the status information of all
  // policies defined in |policies|. |any_policies_set| is set to true if
  // there are policies in the list that were valid, otherwise it's false.
  static scoped_ptr<base::ListValue> GetPolicyStatusList(
      const policy::PolicyMap& policies,
      bool* any_policies_set);

 private:
  // Callback for the "requestData" message. The parameter |args| is unused.
  void HandleRequestData(const base::ListValue* args);

  // Callback for the "fetchPolicy" message. The parameter |args| is unused.
  void HandleFetchPolicy(const base::ListValue* args);

  // Callback for completion of a RefreshPolicies call.
  void OnRefreshDone();

  // Sends policy data to UI.
  void SendDataToUI();

  // Converts a PolicyScope to a string16 representation.
  static string16 GetPolicyScopeString(policy::PolicyScope scope);

  // Converts a PolicyLevel to a string16 representation.
  static string16 GetPolicyLevelString(policy::PolicyLevel level);

  // Fills a DictionaryValue with the details of the given policy, in the
  // format expected by the about:policy webui at
  // src/chrome/browser/resources/policy.js
  static base::DictionaryValue* GetPolicyDetails(
      const policy::PolicyDefinitionList::Entry* policy_definition,
      const policy::PolicyMap::Entry* policy_value,
      const string16& error_message);

  // Fills a DictionaryValue with the error status of the given |policy_name|.
  // If |is_set| is true, this is a policy that was set but is not recognized;
  // if |is_set| is false, this is a known policy that is not configured.
  static base::DictionaryValue* GetPolicyErrorDetails(
      const std::string& policy_name,
      bool is_set);

  // Returns a status message based on |error_details|.
  static string16 CreateStatusMessageString(
      policy::CloudPolicySubsystem::ErrorDetails error_details);

  // Returns a DictionaryValue pointer containing information about the status
  // of the policy system. The caller acquires ownership of the returned
  // DictionaryValue pointer.
  base::DictionaryValue* GetStatusData();

  // Returns the time at which policy was last fetched by the
  // CloudPolicySubsystem |subsystem| in string form.
  string16 GetLastFetchTime(policy::CloudPolicySubsystem* subsystem);

  // Reads the device id from |data_store| and returns it as a string16.
  string16 GetDeviceId(const policy::CloudPolicyDataStore* data_store);

  // Reads the policy fetch interval from the preferences specified by
  // |refresh_pref| and returns it as a string16.
  string16 GetPolicyFetchInterval(const char* refresh_pref);

  // Used to post a callback to RefreshPolicies with a WeakPtr to |this|.
  base::WeakPtrFactory<PolicyUIHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PolicyUIHandler);
};

// The Web UI handler for about:policy.
class PolicyUI : public content::WebUIController {
 public:
  explicit PolicyUI(content::WebUI* web_ui);
  virtual ~PolicyUI();

 private:
  DISALLOW_COPY_AND_ASSIGN(PolicyUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_POLICY_UI_H_
