// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_auth_policy_client.h"

#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/md5.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/chromeos/policy/proto/chrome_device_policy.pb.h"
#include "chromeos/chromeos_paths.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/signin/core/account_id/account_id.h"
#include "dbus/message.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace em = enterprise_management;

namespace {

const size_t kMaxMachineNameLength = 15;
const char kInvalidMachineNameCharacters[] = "\\/:*?\"<>|";

// Drop stub policy file of |policy_type| at |policy_path| containing
// |serialized_payload|.
bool WritePolicyFile(const base::FilePath& policy_path,
                     const std::string& serialized_payload,
                     const std::string& policy_type,
                     const base::TimeDelta& delay) {
  base::PlatformThread::Sleep(delay);

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

// Posts |closure| on the ThreadTaskRunner with |delay|.
void PostDelayedClosure(base::OnceClosure closure,
                        const base::TimeDelta& delay) {
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, std::move(closure), delay);
}

// Runs |signal_callback| with Signal*. Needed to own Signal object.
void RunSignalCallback(const std::string& interface_name,
                       const std::string& method_name,
                       dbus::ObjectProxy::SignalCallback signal_callback) {
  signal_callback.Run(
      base::MakeUnique<dbus::Signal>(interface_name, method_name).get());
}

}  // namespace

namespace chromeos {

FakeAuthPolicyClient::FakeAuthPolicyClient() {}

FakeAuthPolicyClient::~FakeAuthPolicyClient() {}

void FakeAuthPolicyClient::Init(dbus::Bus* bus) {}

void FakeAuthPolicyClient::JoinAdDomain(const std::string& machine_name,
                                        const std::string& user_principal_name,
                                        int password_fd,
                                        JoinCallback callback) {
  authpolicy::ErrorType error = authpolicy::ERROR_NONE;
  if (!started_) {
    LOG(ERROR) << "authpolicyd not started";
    error = authpolicy::ERROR_DBUS_FAILURE;
  } else if (machine_name.size() > kMaxMachineNameLength) {
    error = authpolicy::ERROR_MACHINE_NAME_TOO_LONG;
  } else if (machine_name.empty() ||
             machine_name.find_first_of(kInvalidMachineNameCharacters) !=
                 std::string::npos) {
    error = authpolicy::ERROR_INVALID_MACHINE_NAME;
  } else {
    std::vector<std::string> parts = base::SplitString(
        user_principal_name, "@", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    if (parts.size() != 2 || parts[0].empty() || parts[1].empty()) {
      error = authpolicy::ERROR_PARSE_UPN_FAILED;
    }
  }
  PostDelayedClosure(base::BindOnce(std::move(callback), error),
                     dbus_operation_delay_);
}

void FakeAuthPolicyClient::AuthenticateUser(
    const std::string& user_principal_name,
    const std::string& object_guid,
    int password_fd,
    AuthCallback callback) {
  authpolicy::ErrorType error = authpolicy::ERROR_NONE;
  authpolicy::ActiveDirectoryAccountInfo account_info;
  if (!started_) {
    LOG(ERROR) << "authpolicyd not started";
    error = authpolicy::ERROR_DBUS_FAILURE;
  } else {
    if (auth_error_ == authpolicy::ERROR_NONE) {
      if (object_guid.empty())
        account_info.set_account_id(base::MD5String(user_principal_name));
      else
        account_info.set_account_id(object_guid);
    }
    error = auth_error_;
  }
  PostDelayedClosure(base::BindOnce(std::move(callback), error, account_info),
                     dbus_operation_delay_);
}

void FakeAuthPolicyClient::GetUserStatus(const std::string& object_guid,
                                         GetUserStatusCallback callback) {
  authpolicy::ActiveDirectoryUserStatus user_status;
  user_status.set_password_status(password_status_);
  user_status.set_tgt_status(tgt_status_);

  authpolicy::ActiveDirectoryAccountInfo* const account_info =
      user_status.mutable_account_info();
  account_info->set_account_id(object_guid);
  if (!display_name_.empty())
    account_info->set_display_name(display_name_);
  if (!given_name_.empty())
    account_info->set_given_name(given_name_);

  PostDelayedClosure(
      base::BindOnce(std::move(callback), authpolicy::ERROR_NONE, user_status),
      dbus_operation_delay_);
  if (!on_get_status_closure_.is_null())
    PostDelayedClosure(std::move(on_get_status_closure_),
                       dbus_operation_delay_);
}

void FakeAuthPolicyClient::GetUserKerberosFiles(
    const std::string& object_guid,
    GetUserKerberosFilesCallback callback) {
  authpolicy::KerberosFiles files;
  files.set_krb5cc("credentials");
  files.set_krb5conf("configuration");
  PostDelayedClosure(
      base::BindOnce(std::move(callback), authpolicy::ERROR_NONE, files),
      dbus_operation_delay_);
}

void FakeAuthPolicyClient::RefreshDevicePolicy(RefreshPolicyCallback callback) {
  if (!started_) {
    LOG(ERROR) << "authpolicyd not started";
    std::move(callback).Run(false);
    return;
  }
  base::FilePath policy_path;
  if (!PathService::Get(chromeos::FILE_OWNER_KEY, &policy_path)) {
    std::move(callback).Run(false);
    return;
  }
  policy_path = policy_path.DirName().AppendASCII("stub_device_policy");

  em::ChromeDeviceSettingsProto policy;
  std::string payload;
  CHECK(policy.SerializeToString(&payload));

  // Drop file for SessionManagerClientStubImpl to read.
  base::PostTaskWithTraitsAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BACKGROUND,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&WritePolicyFile, policy_path, payload,
                     "google/chromeos/device", disk_operation_delay_),
      std::move(callback));
}

void FakeAuthPolicyClient::RefreshUserPolicy(const AccountId& account_id,
                                             RefreshPolicyCallback callback) {
  if (!started_) {
    LOG(ERROR) << "authpolicyd not started";
    std::move(callback).Run(false);
    return;
  }
  base::FilePath policy_path;
  if (!PathService::Get(chromeos::DIR_USER_POLICY_KEYS, &policy_path)) {
    std::move(callback).Run(false);
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
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BACKGROUND,
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&WritePolicyFile, policy_path, payload,
                     "google/chromeos/user", disk_operation_delay_),
      std::move(callback));
}

void FakeAuthPolicyClient::ConnectToSignal(
    const std::string& signal_name,
    dbus::ObjectProxy::SignalCallback signal_callback,
    dbus::ObjectProxy::OnConnectedCallback on_connected_callback) {
  std::move(on_connected_callback)
      .Run(authpolicy::kAuthPolicyInterface, signal_name, true /* success */);
  PostDelayedClosure(
      base::BindOnce(RunSignalCallback, authpolicy::kAuthPolicyInterface,
                     signal_name, signal_callback),
      dbus_operation_delay_);
}

}  // namespace chromeos
