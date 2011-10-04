// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_POLICY_UI_H_
#define CHROME_BROWSER_UI_WEBUI_POLICY_UI_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/policy/cloud_policy_subsystem.h"
#include "chrome/browser/policy/configuration_policy_reader.h"
#include "chrome/browser/ui/webui/chrome_web_ui.h"

namespace policy {
class CloudPolicyDataStore;
}

// The base class handler of Javascript messages of the about:policy page.
class PolicyUIHandler : public WebUIMessageHandler,
                        public policy::PolicyStatus::Observer {
 public:
  PolicyUIHandler();
  virtual ~PolicyUIHandler();

  // WebUIMessageHandler implementation.
  virtual WebUIMessageHandler* Attach(WebUI* web_ui) OVERRIDE;
  virtual void RegisterMessages() OVERRIDE;

  // policy::PolicyStatus::Observer implementation.
  virtual void OnPolicyValuesChanged() OVERRIDE;

 private:

  // Callback for the "requestData" message. The parameter |args| is unused.
  void HandleRequestData(const ListValue* args);

  // Callback for the "fetchPolicy" message. The parameter |args| is unused.
  void HandleFetchPolicy(const ListValue* args);

  // Send requested data to UI. |is_policy_update| should be set to true when
  // policy data is pushed to the UI without having been requested by a
  // javascript message and to false otherwise.
  void SendDataToUI(bool is_policy_update);

  // Returns a DictionaryValue pointer containing information about the status
  // of the policy system. The caller acquires ownership of the returned
  // DictionaryValue pointer.
  DictionaryValue* GetStatusData();

  // Returns the time at which policy was last fetched by the
  // CloudPolicySubsystem |subsystem| in string form.
  string16 GetLastFetchTime(policy::CloudPolicySubsystem* subsystem);

  // Reads the device id from |data_store| and returns it as a string16.
  string16 GetDeviceId(const policy::CloudPolicyDataStore* data_store);

  // Reads the policy fetch interval from the preferences specified by
  // |refresh_pref| and returns it as a string16.
  string16 GetPolicyFetchInterval(const char* refresh_pref);

  // Returns the string corresponding to the CloudPolicySubsystem::ErrorDetails
  // enum value |error_details|.
  static string16 CreateStatusMessageString(
      policy::CloudPolicySubsystem::ErrorDetails error_details);

  scoped_ptr<policy::PolicyStatus> policy_status_;

  DISALLOW_COPY_AND_ASSIGN(PolicyUIHandler);
};

// The Web UI handler for about:policy.
class PolicyUI : public ChromeWebUI {
 public:
  explicit PolicyUI(TabContents* contents);
  virtual ~PolicyUI();

 private:
  DISALLOW_COPY_AND_ASSIGN(PolicyUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_POLICY_UI_H_
