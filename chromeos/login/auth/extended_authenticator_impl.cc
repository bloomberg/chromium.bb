// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/auth/extended_authenticator_impl.h"

#include <stddef.h>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/cryptohome/homedir_methods.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/user_context.h"
#include "chromeos/login_event_recorder.h"
#include "components/signin/core/account_id/account_id.h"
#include "crypto/sha2.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace chromeos {

namespace {

void RecordStartMarker(const std::string& marker) {
  std::string full_marker = "Cryptohome-";
  full_marker.append(marker);
  full_marker.append("-Start");
  chromeos::LoginEventRecorder::Get()->AddLoginTimeMarker(full_marker, false);
}

void RecordEndMarker(const std::string& marker) {
  std::string full_marker = "Cryptohome-";
  full_marker.append(marker);
  full_marker.append("-End");
  chromeos::LoginEventRecorder::Get()->AddLoginTimeMarker(full_marker, false);
}

}  // namespace

ExtendedAuthenticatorImpl::ExtendedAuthenticatorImpl(
    NewAuthStatusConsumer* consumer)
    : salt_obtained_(false), consumer_(consumer), old_consumer_(NULL) {
  SystemSaltGetter::Get()->GetSystemSalt(
      base::Bind(&ExtendedAuthenticatorImpl::OnSaltObtained, this));
}

ExtendedAuthenticatorImpl::ExtendedAuthenticatorImpl(
    AuthStatusConsumer* consumer)
    : salt_obtained_(false), consumer_(NULL), old_consumer_(consumer) {
  SystemSaltGetter::Get()->GetSystemSalt(
      base::Bind(&ExtendedAuthenticatorImpl::OnSaltObtained, this));
}

void ExtendedAuthenticatorImpl::SetConsumer(AuthStatusConsumer* consumer) {
  old_consumer_ = consumer;
}

void ExtendedAuthenticatorImpl::AuthenticateToMount(
    const UserContext& context,
    const ResultCallback& success_callback) {
  TransformKeyIfNeeded(
      context,
      base::Bind(&ExtendedAuthenticatorImpl::DoAuthenticateToMount,
                 this,
                 success_callback));
}

void ExtendedAuthenticatorImpl::AuthenticateToCheck(
    const UserContext& context,
    const base::Closure& success_callback) {
  TransformKeyIfNeeded(
      context,
      base::Bind(&ExtendedAuthenticatorImpl::DoAuthenticateToCheck,
                 this,
                 success_callback));
}

void ExtendedAuthenticatorImpl::CreateMount(
    const AccountId& account_id,
    const std::vector<cryptohome::KeyDefinition>& keys,
    const ResultCallback& success_callback) {
  RecordStartMarker("MountEx");

  cryptohome::Identification id(account_id);
  cryptohome::Authorization auth(keys.front());
  cryptohome::MountRequest mount;
  for (size_t i = 0; i < keys.size(); i++) {
    KeyDefinitionToKey(keys[i], mount.mutable_create()->add_keys());
  }
  UserContext context(account_id);
  Key key(keys.front().secret);
  key.SetLabel(keys.front().label);
  context.SetKey(key);

  cryptohome::HomedirMethods::GetInstance()->MountEx(
      id,
      auth,
      mount,
      base::Bind(&ExtendedAuthenticatorImpl::OnMountComplete,
                 this,
                 "MountEx",
                 context,
                 success_callback));
}

void ExtendedAuthenticatorImpl::AddKey(const UserContext& context,
                                   const cryptohome::KeyDefinition& key,
                                   bool replace_existing,
                                   const base::Closure& success_callback) {
  TransformKeyIfNeeded(context,
                       base::Bind(&ExtendedAuthenticatorImpl::DoAddKey,
                                  this,
                                  key,
                                  replace_existing,
                                  success_callback));
}

void ExtendedAuthenticatorImpl::UpdateKeyAuthorized(
    const UserContext& context,
    const cryptohome::KeyDefinition& key,
    const std::string& signature,
    const base::Closure& success_callback) {
  TransformKeyIfNeeded(
      context,
      base::Bind(&ExtendedAuthenticatorImpl::DoUpdateKeyAuthorized,
                 this,
                 key,
                 signature,
                 success_callback));
}

void ExtendedAuthenticatorImpl::RemoveKey(const UserContext& context,
                                      const std::string& key_to_remove,
                                      const base::Closure& success_callback) {
  TransformKeyIfNeeded(context,
                       base::Bind(&ExtendedAuthenticatorImpl::DoRemoveKey,
                                  this,
                                  key_to_remove,
                                  success_callback));
}

void ExtendedAuthenticatorImpl::TransformKeyIfNeeded(
    const UserContext& user_context,
    const ContextCallback& callback) {
  if (user_context.GetKey()->GetKeyType() != Key::KEY_TYPE_PASSWORD_PLAIN) {
    callback.Run(user_context);
    return;
  }

  if (!salt_obtained_) {
    system_salt_callbacks_.push_back(
        base::Bind(&ExtendedAuthenticatorImpl::TransformKeyIfNeeded,
                   this,
                   user_context,
                   callback));
    return;
  }

  UserContext transformed_context = user_context;
  transformed_context.GetKey()->Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF,
                                          system_salt_);
  callback.Run(transformed_context);
}

ExtendedAuthenticatorImpl::~ExtendedAuthenticatorImpl() {
}

void ExtendedAuthenticatorImpl::OnSaltObtained(const std::string& system_salt) {
  salt_obtained_ = true;
  system_salt_ = system_salt;
  for (std::vector<base::Closure>::const_iterator it =
           system_salt_callbacks_.begin();
       it != system_salt_callbacks_.end();
       ++it) {
    it->Run();
  }
  system_salt_callbacks_.clear();
}

void ExtendedAuthenticatorImpl::DoAuthenticateToMount(
    const ResultCallback& success_callback,
    const UserContext& user_context) {
  RecordStartMarker("MountEx");

  cryptohome::Identification id(user_context.GetAccountId());
  const Key* const key = user_context.GetKey();
  cryptohome::Authorization auth(key->GetSecret(), key->GetLabel());
  cryptohome::MountRequest mount;

  cryptohome::HomedirMethods::GetInstance()->MountEx(
      id,
      auth,
      mount,
      base::Bind(&ExtendedAuthenticatorImpl::OnMountComplete,
                 this,
                 "MountEx",
                 user_context,
                 success_callback));
}

void ExtendedAuthenticatorImpl::DoAuthenticateToCheck(
    const base::Closure& success_callback,
    const UserContext& user_context) {
  RecordStartMarker("CheckKeyEx");

  cryptohome::Identification id(user_context.GetAccountId());
  const Key* const key = user_context.GetKey();
  cryptohome::Authorization auth(key->GetSecret(), key->GetLabel());

  cryptohome::HomedirMethods::GetInstance()->CheckKeyEx(
      id,
      auth,
      base::Bind(&ExtendedAuthenticatorImpl::OnOperationComplete,
                 this,
                 "CheckKeyEx",
                 user_context,
                 success_callback));
}

void ExtendedAuthenticatorImpl::DoAddKey(const cryptohome::KeyDefinition& key,
                                     bool replace_existing,
                                     const base::Closure& success_callback,
                                     const UserContext& user_context) {
  RecordStartMarker("AddKeyEx");

  cryptohome::Identification id(user_context.GetAccountId());
  const Key* const auth_key = user_context.GetKey();
  cryptohome::Authorization auth(auth_key->GetSecret(), auth_key->GetLabel());

  cryptohome::HomedirMethods::GetInstance()->AddKeyEx(
      id,
      auth,
      key,
      replace_existing,
      base::Bind(&ExtendedAuthenticatorImpl::OnOperationComplete,
                 this,
                 "AddKeyEx",
                 user_context,
                 success_callback));
}

void ExtendedAuthenticatorImpl::DoUpdateKeyAuthorized(
    const cryptohome::KeyDefinition& key,
    const std::string& signature,
    const base::Closure& success_callback,
    const UserContext& user_context) {
  RecordStartMarker("UpdateKeyAuthorized");

  cryptohome::Identification id(user_context.GetAccountId());
  const Key* const auth_key = user_context.GetKey();
  cryptohome::Authorization auth(auth_key->GetSecret(), auth_key->GetLabel());

  cryptohome::HomedirMethods::GetInstance()->UpdateKeyEx(
      id,
      auth,
      key,
      signature,
      base::Bind(&ExtendedAuthenticatorImpl::OnOperationComplete,
                 this,
                 "UpdateKeyAuthorized",
                 user_context,
                 success_callback));
}

void ExtendedAuthenticatorImpl::DoRemoveKey(const std::string& key_to_remove,
                                        const base::Closure& success_callback,
                                        const UserContext& user_context) {
  RecordStartMarker("RemoveKeyEx");

  cryptohome::Identification id(user_context.GetAccountId());
  const Key* const auth_key = user_context.GetKey();
  cryptohome::Authorization auth(auth_key->GetSecret(), auth_key->GetLabel());

  cryptohome::HomedirMethods::GetInstance()->RemoveKeyEx(
      id,
      auth,
      key_to_remove,
      base::Bind(&ExtendedAuthenticatorImpl::OnOperationComplete,
                 this,
                 "RemoveKeyEx",
                 user_context,
                 success_callback));
}

void ExtendedAuthenticatorImpl::OnMountComplete(
    const std::string& time_marker,
    const UserContext& user_context,
    const ResultCallback& success_callback,
    bool success,
    cryptohome::MountError return_code,
    const std::string& mount_hash) {
  RecordEndMarker(time_marker);
  UserContext copy = user_context;
  copy.SetUserIDHash(mount_hash);
  if (return_code == cryptohome::MOUNT_ERROR_NONE) {
    if (!success_callback.is_null())
      success_callback.Run(mount_hash);
    if (old_consumer_)
      old_consumer_->OnAuthSuccess(copy);
    return;
  }
  AuthState state = FAILED_MOUNT;
  if (return_code == cryptohome::MOUNT_ERROR_TPM_COMM_ERROR ||
      return_code == cryptohome::MOUNT_ERROR_TPM_DEFEND_LOCK ||
      return_code == cryptohome::MOUNT_ERROR_TPM_NEEDS_REBOOT) {
    state = FAILED_TPM;
  }
  if (return_code == cryptohome::MOUNT_ERROR_USER_DOES_NOT_EXIST) {
    state = NO_MOUNT;
  }
  if (consumer_)
    consumer_->OnAuthenticationFailure(state);
  if (old_consumer_) {
    AuthFailure failure(AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME);
    old_consumer_->OnAuthFailure(failure);
  }
}

void ExtendedAuthenticatorImpl::OnOperationComplete(
    const std::string& time_marker,
    const UserContext& user_context,
    const base::Closure& success_callback,
    bool success,
    cryptohome::MountError return_code) {
  RecordEndMarker(time_marker);
  if (return_code == cryptohome::MOUNT_ERROR_NONE) {
    if (!success_callback.is_null())
      success_callback.Run();
    if (old_consumer_)
      old_consumer_->OnAuthSuccess(user_context);
    return;
  }

  LOG(ERROR) << "Supervised user cryptohome error, code: " << return_code;

  AuthState state = FAILED_MOUNT;

  if (return_code == cryptohome::MOUNT_ERROR_TPM_COMM_ERROR ||
      return_code == cryptohome::MOUNT_ERROR_TPM_DEFEND_LOCK ||
      return_code == cryptohome::MOUNT_ERROR_TPM_NEEDS_REBOOT) {
    state = FAILED_TPM;
  }

  if (return_code == cryptohome::MOUNT_ERROR_USER_DOES_NOT_EXIST)
    state = NO_MOUNT;

  if (consumer_)
    consumer_->OnAuthenticationFailure(state);

  if (old_consumer_) {
    AuthFailure failure(AuthFailure::UNLOCK_FAILED);
    old_consumer_->OnAuthFailure(failure);
  }
}

}  // namespace chromeos
