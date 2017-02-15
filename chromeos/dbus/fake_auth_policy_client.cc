// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_auth_policy_client.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/md5.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/chromeos/policy/proto/chrome_device_policy.pb.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/signin/core/account_id/account_id.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace em = enterprise_management;

namespace {

const size_t kMaxMachineNameLength = 15;
const char kInvalidMachineNameCharacters[] = "\\/:*?\"<>|";

// Delay policy fetch to be more realistic.
constexpr int kPolicyFetchDelaySeconds = 5;

// Drop stub policy file of |policy_type| at |policy_path| containing
// |serialized_payload|.
bool WritePolicyFile(const base::FilePath& policy_path,
                     const std::string& serialized_payload,
                     const std::string& policy_type) {
  base::PlatformThread::Sleep(
      base::TimeDelta::FromSeconds(kPolicyFetchDelaySeconds));

  em::PolicyData data;
  data.set_policy_value(serialized_payload);
  data.set_policy_type(policy_type);

  em::PolicyFetchResponse response;
  CHECK(data.SerializeToString(response.mutable_policy_data()));
  std::string serialized_response;
  CHECK(response.SerializeToString(&serialized_response));

  if (!base::CreateDirectory(policy_path.DirName()))
    return false;

  // Note that in theory there could be a short time window in which a
  // concurrent reader sees a partial (and thus invalid) file, but given the
  // small file size that seems very unlikely in practice.
  const int bytes_written = base::WriteFile(
      policy_path, serialized_response.c_str(), serialized_response.size());
  if (bytes_written < 0)
    return false;
  return bytes_written == static_cast<int>(serialized_response.size());
}

}  // namespace

namespace chromeos {

FakeAuthPolicyClient::FakeAuthPolicyClient() {}

FakeAuthPolicyClient::~FakeAuthPolicyClient() {}

void FakeAuthPolicyClient::Init(dbus::Bus* bus) {}

void FakeAuthPolicyClient::JoinAdDomain(const std::string& machine_name,
                                        const std::string& user_principal_name,
                                        int password_fd,
                                        const JoinCallback& callback) {
  if (!started_) {
    LOG(ERROR) << "authpolicyd not started";
    callback.Run(authpolicy::ERROR_DBUS_FAILURE);
    return;
  }
  if (machine_name.size() > kMaxMachineNameLength) {
    callback.Run(authpolicy::ERROR_MACHINE_NAME_TOO_LONG);
    return;
  }

  if (machine_name.empty() ||
      machine_name.find_first_of(kInvalidMachineNameCharacters) !=
          std::string::npos) {
    callback.Run(authpolicy::ERROR_BAD_MACHINE_NAME);
    return;
  }

  std::vector<std::string> parts = base::SplitString(
      user_principal_name, "@", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (parts.size() != 2 || parts[0].empty() || parts[1].empty()) {
    callback.Run(authpolicy::ERROR_PARSE_UPN_FAILED);
    return;
  }

  callback.Run(authpolicy::ERROR_NONE);
}

void FakeAuthPolicyClient::AuthenticateUser(
    const std::string& user_principal_name,
    int password_fd,
    const AuthCallback& callback) {
  authpolicy::ActiveDirectoryAccountData account_data;
  if (!started_) {
    LOG(ERROR) << "authpolicyd not started";
    callback.Run(authpolicy::ERROR_DBUS_FAILURE, account_data);
    return;
  }
  account_data.set_account_id(base::MD5String(user_principal_name));
  callback.Run(authpolicy::ERROR_NONE, account_data);
}

void FakeAuthPolicyClient::RefreshDevicePolicy(
    const RefreshPolicyCallback& callback) {
  if (!started_) {
    LOG(ERROR) << "authpolicyd not started";
    callback.Run(false);
    return;
  }
  base::FilePath policy_path;
  if (!PathService::Get(chromeos::FILE_OWNER_KEY, &policy_path)) {
    callback.Run(false);
    return;
  }
  policy_path = policy_path.DirName().AppendASCII("stub_device_policy");

  em::ChromeDeviceSettingsProto policy;
  std::string payload;
  CHECK(policy.SerializeToString(&payload));

  // Drop file for SessionManagerClientStubImpl to read.
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, base::TaskTraits()
                     .WithShutdownBehavior(
                         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN)
                     .WithPriority(base::TaskPriority::BACKGROUND)
                     .MayBlock(),
      Bind(&WritePolicyFile, policy_path, payload, "google/chromeos/device"),
      callback);
}

void FakeAuthPolicyClient::RefreshUserPolicy(
    const AccountId& account_id,
    const RefreshPolicyCallback& callback) {
  if (!started_) {
    LOG(ERROR) << "authpolicyd not started";
    callback.Run(false);
    return;
  }
  base::FilePath policy_path;
  if (!PathService::Get(chromeos::DIR_USER_POLICY_KEYS, &policy_path)) {
    callback.Run(false);
    return;
  }
  const cryptohome::Identification cryptohome_identification(account_id);
  const std::string sanitized_username =
      chromeos::CryptohomeClient::GetStubSanitizedUsername(
          cryptohome_identification);
  policy_path = policy_path.AppendASCII(sanitized_username);
  policy_path = policy_path.AppendASCII("stub_policy");

  em::CloudPolicySettings policy;
  std::string payload;
  CHECK(policy.SerializeToString(&payload));

  // Drop file for SessionManagerClientStubImpl to read.
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE, base::TaskTraits()
                     .WithShutdownBehavior(
                         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN)
                     .WithPriority(base::TaskPriority::BACKGROUND)
                     .MayBlock(),
      base::Bind(&WritePolicyFile, policy_path, payload,
                 "google/chromeos/user"),
      callback);
}

}  // namespace chromeos
