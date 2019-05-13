// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_LOCAL_POLICY_TEST_SERVER_MIXIN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_LOCAL_POLICY_TEST_SERVER_MIXIN_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/mixin_based_in_process_browser_test.h"
#include "chrome/browser/chromeos/policy/server_backed_state_keys_broker.h"
#include "chrome/browser/policy/test/local_policy_test_server.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace chromeos {

// This test mixin covers setting up LocalPolicyTestServer and adding a
// command-line flag to use it. Please see SetUp function for default settings.
// Server is started after SetUp execution.
class LocalPolicyTestServerMixin : public InProcessBrowserTestMixin {
 public:
  explicit LocalPolicyTestServerMixin(InProcessBrowserTestMixinHost* host);
  ~LocalPolicyTestServerMixin() override;

  policy::LocalPolicyTestServer* server() { return policy_test_server_.get(); }

  // InProcessBrowserTestMixin:
  void SetUp() override;
  void SetUpCommandLine(base::CommandLine* command_line) override;

  // Updates device policy blob served by the local policy test server.
  bool UpdateDevicePolicy(
      const enterprise_management::ChromeDeviceSettingsProto& policy);

  // Updates user policy blob served by the local policy test server.
  // |policy_user| - the policy user's email.
  bool UpdateUserPolicy(
      const enterprise_management::CloudPolicySettings& policy,
      const std::string& policy_user);

  // Updates user policies served by the local policy test server, by
  // configuring ser of mandatory and recommended policies that should be
  // returned for the policy user.
  // |policy_user| - the policy user's email.
  bool UpdateUserPolicy(const base::Value& mandatory_policy,
                        const base::Value& recommended_policy,
                        const std::string& policy_user);

  // Configures and sets expectations for enrollment flow with license
  // selection. Non-negative values indicate number of available licenses.
  // There should be at least one license type.
  void ExpectAvailableLicenseCount(int perpetual, int annual, int kiosk);

  void ExpectTokenEnrollment(const std::string& enrollment_token,
                             const std::string& token_creator);

  void SetUpdateDeviceAttributesPermission(bool allowed);

  // Configures fake attestation flow so that we can test attestation-based
  // enrollment flows.
  void SetFakeAttestationFlow();

  // Configures server to respond with particular error code during requests.
  // |net_error_code| - error code from device_management_service.cc.
  void SetExpectedDeviceEnrollmentError(int net_error_code);
  void SetExpectedDeviceAttributeUpdateError(int net_error_code);
  void SetExpectedPolicyFetchError(int net_error_code);

  // Set response for DeviceStateRetrievalRequest. Returns that if finds state
  // key passed in the request. State keys could be set by RegisterClient call
  // on policy test server.
  bool SetDeviceStateRetrievalResponse(
      policy::ServerBackedStateKeysBroker* keys_broker,
      enterprise_management::DeviceStateRetrievalResponse::RestoreMode
          restore_mode,
      const std::string& managemement_domain);

 private:
  std::unique_ptr<policy::LocalPolicyTestServer> policy_test_server_;
  base::Value server_config_;

  DISALLOW_COPY_AND_ASSIGN(LocalPolicyTestServerMixin);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_TEST_LOCAL_POLICY_TEST_SERVER_MIXIN_H_
