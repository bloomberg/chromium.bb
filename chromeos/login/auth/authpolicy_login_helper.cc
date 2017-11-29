// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/auth/authpolicy_login_helper.h"

#include "base/files/file_util.h"
#include "base/task_scheduler/post_task.h"
#include "chromeos/cryptohome/cryptohome_util.h"
#include "chromeos/dbus/auth_policy_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/upstart_client.h"

namespace chromeos {

namespace cu = cryptohome_util;

namespace {

constexpr char kAttrMode[] = "enterprise.mode";
constexpr char kDeviceModeEnterpriseAD[] = "enterprise_ad";

base::ScopedFD GetDataReadPipe(const std::string& data) {
  int pipe_fds[2];
  if (!base::CreateLocalNonBlockingPipe(pipe_fds)) {
    DLOG(ERROR) << "Failed to create pipe";
    return base::ScopedFD();
  }
  base::ScopedFD pipe_read_end(pipe_fds[0]);
  base::ScopedFD pipe_write_end(pipe_fds[1]);

  if (!base::WriteFileDescriptor(pipe_write_end.get(), data.c_str(),
                                 data.size())) {
    DLOG(ERROR) << "Failed to write to pipe";
    return base::ScopedFD();
  }
  return pipe_read_end;
}

void AuthCallbackDoNothing(
    authpolicy::ErrorType /* error */,
    const authpolicy::ActiveDirectoryAccountInfo& /* account_info */) {
  // Do nothing.
}

}  // namespace

AuthPolicyLoginHelper::AuthPolicyLoginHelper() : weak_factory_(this) {}

// static
void AuthPolicyLoginHelper::TryAuthenticateUser(const std::string& username,
                                                const std::string& object_guid,
                                                const std::string& password) {
  authpolicy::AuthenticateUserRequest request;
  request.set_user_principal_name(username);
  request.set_account_id(object_guid);
  chromeos::DBusThreadManager::Get()->GetAuthPolicyClient()->AuthenticateUser(
      request, GetDataReadPipe(password).get(),
      base::BindOnce(&AuthCallbackDoNothing));
}

// static
void AuthPolicyLoginHelper::Restart() {
  chromeos::DBusThreadManager::Get()
      ->GetUpstartClient()
      ->RestartAuthPolicyService();
}

bool AuthPolicyLoginHelper::IsAdLocked() {
  std::string mode;
  return chromeos::cryptohome_util::InstallAttributesGet(kAttrMode, &mode) &&
         mode == kDeviceModeEnterpriseAD;
}

// static
bool AuthPolicyLoginHelper::LockDeviceActiveDirectoryForTesting(
    const std::string& realm) {
  return cu::InstallAttributesSet("enterprise.owned", "true") &&
         cu::InstallAttributesSet("enterprise.mode", "enterprise_ad") &&
         cu::InstallAttributesSet("enterprise.realm", realm) &&
         cu::InstallAttributesFinalize();
}

void AuthPolicyLoginHelper::JoinAdDomain(const std::string& machine_name,
                                         const std::string& username,
                                         const std::string& password,
                                         JoinCallback callback) {
  DCHECK(!IsAdLocked());
  DCHECK(!weak_factory_.HasWeakPtrs()) << "Another operation is in progress";
  authpolicy::JoinDomainRequest request;
  request.set_machine_name(machine_name);
  request.set_user_principal_name(username);
  chromeos::DBusThreadManager::Get()->GetAuthPolicyClient()->JoinAdDomain(
      request, GetDataReadPipe(password).get(),
      base::BindOnce(&AuthPolicyLoginHelper::OnJoinCallback,
                     weak_factory_.GetWeakPtr(), base::Passed(&callback)));
}

void AuthPolicyLoginHelper::AuthenticateUser(const std::string& username,
                                             const std::string& object_guid,
                                             const std::string& password,
                                             AuthCallback callback) {
  DCHECK(!weak_factory_.HasWeakPtrs()) << "Another operation is in progress";
  authpolicy::AuthenticateUserRequest request;
  request.set_user_principal_name(username);
  request.set_account_id(object_guid);
  chromeos::DBusThreadManager::Get()->GetAuthPolicyClient()->AuthenticateUser(
      request, GetDataReadPipe(password).get(),
      base::BindOnce(&AuthPolicyLoginHelper::OnAuthCallback,
                     weak_factory_.GetWeakPtr(), base::Passed(&callback)));
}

void AuthPolicyLoginHelper::CancelRequestsAndRestart() {
  weak_factory_.InvalidateWeakPtrs();
  AuthPolicyLoginHelper::Restart();
}

void AuthPolicyLoginHelper::OnJoinCallback(JoinCallback callback,
                                           authpolicy::ErrorType error) {
  DCHECK(!IsAdLocked());
  if (error != authpolicy::ERROR_NONE) {
    std::move(callback).Run(error);
    return;
  }
  chromeos::DBusThreadManager::Get()
      ->GetAuthPolicyClient()
      ->RefreshDevicePolicy(
          base::BindOnce(&AuthPolicyLoginHelper::OnFirstPolicyRefreshCallback,
                         weak_factory_.GetWeakPtr(), base::Passed(&callback)));
}

void AuthPolicyLoginHelper::OnFirstPolicyRefreshCallback(
    JoinCallback callback,
    authpolicy::ErrorType error) {
  DCHECK(!IsAdLocked());
  // First policy refresh happens before device is locked. So policy store
  // should not succeed. The error means that authpolicyd cached device policy
  // and stores it in the next call to RefreshDevicePolicy in STEP_STORE_POLICY.
  DCHECK(error != authpolicy::ERROR_NONE);
  if (error == authpolicy::ERROR_DEVICE_POLICY_CACHED_BUT_NOT_SENT)
    error = authpolicy::ERROR_NONE;
  std::move(callback).Run(error);
}

void AuthPolicyLoginHelper::OnAuthCallback(
    AuthCallback callback,
    authpolicy::ErrorType error,
    const authpolicy::ActiveDirectoryAccountInfo& account_info) {
  std::move(callback).Run(error, account_info);
}

AuthPolicyLoginHelper::~AuthPolicyLoginHelper() = default;

}  // namespace chromeos
