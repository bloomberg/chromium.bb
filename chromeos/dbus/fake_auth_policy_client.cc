// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/fake_auth_policy_client.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/task_runner_util.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/chromeos/policy/proto/chrome_device_policy.pb.h"
#include "chromeos/chromeos_paths.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace {

// Create minimal stub device policy file and drop it at the place where
// SessionManagerClientStubImpl is looking for it.
bool WriteDevicePolicyFile() {
  em::ChromeDeviceSettingsProto policy;
  em::PolicyData data;
  policy.SerializeToString(data.mutable_policy_value());
  data.set_policy_type("google/chromeos/device");

  em::PolicyFetchResponse response;
  data.SerializeToString(response.mutable_policy_data());
  std::string serialized_response;
  response.SerializeToString(&serialized_response);

  base::FilePath owner_key_path;
  if (!PathService::Get(chromeos::FILE_OWNER_KEY, &owner_key_path))
    return false;

  const base::FilePath device_policy_path =
      owner_key_path.DirName().AppendASCII("stub_device_policy");

  // Note that in theory there could be a short time window in which a
  // concurrent reader sees a partial (and thus invalid) file, but given the
  // small file size that seems very unlikely in practice.
  const int bytes_written =
      base::WriteFile(device_policy_path, serialized_response.c_str(),
                      serialized_response.size());
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
                                        const std::string& user,
                                        int password_fd,
                                        const JoinCallback& callback) {
  callback.Run(authpolicy::AD_JOIN_ERROR_NONE);
}

void FakeAuthPolicyClient::RefreshDevicePolicy(
    const RefreshPolicyCallback& callback) {
  if (!base::PostTaskAndReplyWithResult(
          base::WorkerPool::GetTaskRunner(false /* task_is_slow */).get(),
          FROM_HERE, base::Bind(&WriteDevicePolicyFile), callback)) {
    callback.Run(false);
  }
}

void FakeAuthPolicyClient::RefreshUserPolicy(
    const std::string& account_id,
    const RefreshPolicyCallback& callback) {
  callback.Run(true);
}

}  // namespace chromeos
