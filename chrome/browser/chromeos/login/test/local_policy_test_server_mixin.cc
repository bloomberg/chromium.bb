// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/local_policy_test_server_mixin.h"

#include <utility>

#include "base/guid.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/cloud/policy_builder.h"
#include "components/policy/core/common/policy_switches.h"

namespace chromeos {

namespace {

base::Value GetDefaultConfig() {
  base::Value config(base::Value::Type::DICTIONARY);

  base::Value managed_users(base::Value::Type::LIST);
  managed_users.GetList().emplace_back("*");
  config.SetKey("managed_users", std::move(managed_users));

  return config;
}

}  // namespace

LocalPolicyTestServerMixin::LocalPolicyTestServerMixin(
    InProcessBrowserTestMixinHost* host)
    : InProcessBrowserTestMixin(host) {
  server_config_ = GetDefaultConfig();
}

void LocalPolicyTestServerMixin::SetUp() {
  policy_test_server_ = std::make_unique<policy::LocalPolicyTestServer>();
  policy_test_server_->SetConfig(server_config_);
  policy_test_server_->RegisterClient(policy::PolicyBuilder::kFakeToken,
                                      policy::PolicyBuilder::kFakeDeviceId,
                                      {} /* state_keys */);

  ASSERT_TRUE(policy_test_server_->SetSigningKeyAndSignature(
      policy::PolicyBuilder::CreateTestSigningKey().get(),
      policy::PolicyBuilder::GetTestSigningKeySignature()));

  ASSERT_TRUE(policy_test_server_->Start());
}

void LocalPolicyTestServerMixin::SetUpCommandLine(
    base::CommandLine* command_line) {
  // Specify device management server URL.
  command_line->AppendSwitchASCII(policy::switches::kDeviceManagementUrl,
                                  policy_test_server_->GetServiceURL().spec());
}

bool LocalPolicyTestServerMixin::UpdateDevicePolicy(
    const enterprise_management::ChromeDeviceSettingsProto& policy) {
  DCHECK(policy_test_server_);
  return policy_test_server_->UpdatePolicy(
      policy::dm_protocol::kChromeDevicePolicyType,
      std::string() /* entity_id */, policy.SerializeAsString());
}

bool LocalPolicyTestServerMixin::SetDeviceStateRetrievalResponse(
    policy::ServerBackedStateKeysBroker* keys_broker,
    enterprise_management::DeviceStateRetrievalResponse::RestoreMode
        restore_mode,
    const std::string& managemement_domain) {
  std::vector<std::string> keys;
  base::RunLoop loop;
  keys_broker->RequestStateKeys(base::BindOnce(
      [](std::vector<std::string>* keys, base::OnceClosure quit,
         const std::vector<std::string>& state_keys) {
        *keys = state_keys;
        std::move(quit).Run();
      },
      &keys, loop.QuitClosure()));
  loop.Run();
  if (keys.empty())
    return false;
  if (!policy_test_server_->RegisterClient("dm_token", base::GenerateGUID(),
                                           keys)) {
    return false;
  }

  base::Value device_state(base::Value::Type::DICTIONARY);
  device_state.SetKey("management_domain", base::Value(managemement_domain));
  device_state.SetKey("restore_mode",
                      base::Value(static_cast<int>(restore_mode)));
  server_config_.SetKey("device_state", std::move(device_state));
  return policy_test_server_->SetConfig(server_config_);
}

LocalPolicyTestServerMixin::~LocalPolicyTestServerMixin() = default;

}  // namespace chromeos
