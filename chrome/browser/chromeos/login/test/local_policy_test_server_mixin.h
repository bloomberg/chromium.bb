// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_TEST_LOCAL_POLICY_TEST_SERVER_MIXIN_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_TEST_LOCAL_POLICY_TEST_SERVER_MIXIN_H_

#include <memory>

#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/mixin_based_in_process_browser_test.h"
#include "chrome/browser/chromeos/policy/server_backed_state_keys_broker.h"
#include "chrome/browser/policy/test/local_policy_test_server.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
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

  bool UpdateDevicePolicy(
      const enterprise_management::ChromeDeviceSettingsProto& policy);

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
