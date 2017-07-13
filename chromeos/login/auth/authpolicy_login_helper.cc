// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/auth/authpolicy_login_helper.h"

#include "base/files/file_util.h"
#include "base/task_scheduler/post_task.h"
#include "chromeos/dbus/auth_policy_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/upstart_client.h"

namespace chromeos {
namespace {

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
  chromeos::DBusThreadManager::Get()->GetAuthPolicyClient()->AuthenticateUser(
      username, object_guid, GetDataReadPipe(password).get(),
      base::BindOnce(&AuthCallbackDoNothing));
}

// static
void AuthPolicyLoginHelper::Restart() {
  chromeos::DBusThreadManager::Get()
      ->GetUpstartClient()
      ->RestartAuthPolicyService();
}

void AuthPolicyLoginHelper::JoinAdDomain(const std::string& machine_name,
                                         const std::string& username,
                                         const std::string& password,
                                         JoinCallback callback) {
  DCHECK(!weak_factory_.HasWeakPtrs()) << "Another operation is in progress";
  chromeos::DBusThreadManager::Get()->GetAuthPolicyClient()->JoinAdDomain(
      machine_name, username, GetDataReadPipe(password).get(),
      base::BindOnce(&AuthPolicyLoginHelper::OnJoinCallback,
                     weak_factory_.GetWeakPtr(), base::Passed(&callback)));
}

void AuthPolicyLoginHelper::AuthenticateUser(const std::string& username,
                                             const std::string& object_guid,
                                             const std::string& password,
                                             AuthCallback callback) {
  DCHECK(!weak_factory_.HasWeakPtrs()) << "Another operation is in progress";
  chromeos::DBusThreadManager::Get()->GetAuthPolicyClient()->AuthenticateUser(
      username, object_guid, GetDataReadPipe(password).get(),
      base::BindOnce(&AuthPolicyLoginHelper::OnAuthCallback,
                     weak_factory_.GetWeakPtr(), base::Passed(&callback)));
}

void AuthPolicyLoginHelper::CancelRequestsAndRestart() {
  weak_factory_.InvalidateWeakPtrs();
  AuthPolicyLoginHelper::Restart();
}

void AuthPolicyLoginHelper::OnJoinCallback(JoinCallback callback,
                                           authpolicy::ErrorType error) {
  std::move(callback).Run(error);
}

void AuthPolicyLoginHelper::OnAuthCallback(
    AuthCallback callback,
    authpolicy::ErrorType error,
    const authpolicy::ActiveDirectoryAccountInfo& account_info) {
  std::move(callback).Run(error, account_info);
}

AuthPolicyLoginHelper::~AuthPolicyLoginHelper() {}

}  // namespace chromeos
