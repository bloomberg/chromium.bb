// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/login/auth/extended_authenticator.h"

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
#include "crypto/sha2.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

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

ExtendedAuthenticator::ExtendedAuthenticator(NewAuthStatusConsumer* consumer)
    : salt_obtained_(false), consumer_(consumer), old_consumer_(NULL) {
  SystemSaltGetter::Get()->GetSystemSalt(
      base::Bind(&ExtendedAuthenticator::OnSaltObtained, this));
}

ExtendedAuthenticator::ExtendedAuthenticator(AuthStatusConsumer* consumer)
    : salt_obtained_(false), consumer_(NULL), old_consumer_(consumer) {
  SystemSaltGetter::Get()->GetSystemSalt(
      base::Bind(&ExtendedAuthenticator::OnSaltObtained, this));
}

void ExtendedAuthenticator::SetConsumer(AuthStatusConsumer* consumer) {
  old_consumer_ = consumer;
}

void ExtendedAuthenticator::AuthenticateToMount(
    const UserContext& context,
    const ResultCallback& success_callback) {
  TransformKeyIfNeeded(context,
                       base::Bind(&ExtendedAuthenticator::DoAuthenticateToMount,
                                  this,
                                  success_callback));
}

void ExtendedAuthenticator::AuthenticateToCheck(
    const UserContext& context,
    const base::Closure& success_callback) {
  TransformKeyIfNeeded(context,
                       base::Bind(&ExtendedAuthenticator::DoAuthenticateToCheck,
                                  this,
                                  success_callback));
}

void ExtendedAuthenticator::CreateMount(
    const std::string& user_id,
    const std::vector<cryptohome::KeyDefinition>& keys,
    const ResultCallback& success_callback) {
  RecordStartMarker("MountEx");

  std::string canonicalized = gaia::CanonicalizeEmail(user_id);
  cryptohome::Identification id(canonicalized);
  cryptohome::Authorization auth(keys.front());
  cryptohome::MountParameters mount(false);
  for (size_t i = 0; i < keys.size(); i++) {
    mount.create_keys.push_back(keys[i]);
  }
  UserContext context(user_id);
  Key key(keys.front().key);
  key.SetLabel(keys.front().label);
  context.SetKey(key);

  cryptohome::HomedirMethods::GetInstance()->MountEx(
      id,
      auth,
      mount,
      base::Bind(&ExtendedAuthenticator::OnMountComplete,
                 this,
                 "MountEx",
                 context,
                 success_callback));
}

void ExtendedAuthenticator::AddKey(const UserContext& context,
                                   const cryptohome::KeyDefinition& key,
                                   bool replace_existing,
                                   const base::Closure& success_callback) {
  TransformKeyIfNeeded(context,
                       base::Bind(&ExtendedAuthenticator::DoAddKey,
                                  this,
                                  key,
                                  replace_existing,
                                  success_callback));
}

void ExtendedAuthenticator::UpdateKeyAuthorized(
    const UserContext& context,
    const cryptohome::KeyDefinition& key,
    const std::string& signature,
    const base::Closure& success_callback) {
  TransformKeyIfNeeded(context,
                       base::Bind(&ExtendedAuthenticator::DoUpdateKeyAuthorized,
                                  this,
                                  key,
                                  signature,
                                  success_callback));
}

void ExtendedAuthenticator::RemoveKey(const UserContext& context,
                                      const std::string& key_to_remove,
                                      const base::Closure& success_callback) {
  TransformKeyIfNeeded(context,
                       base::Bind(&ExtendedAuthenticator::DoRemoveKey,
                                  this,
                                  key_to_remove,
                                  success_callback));
}

void ExtendedAuthenticator::TransformKeyIfNeeded(
    const UserContext& user_context,
    const ContextCallback& callback) {
  if (user_context.GetKey()->GetKeyType() != Key::KEY_TYPE_PASSWORD_PLAIN) {
    callback.Run(user_context);
    return;
  }

  if (!salt_obtained_) {
    system_salt_callbacks_.push_back(
        base::Bind(&ExtendedAuthenticator::TransformKeyIfNeeded,
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

ExtendedAuthenticator::~ExtendedAuthenticator() {
}

void ExtendedAuthenticator::OnSaltObtained(const std::string& system_salt) {
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

void ExtendedAuthenticator::DoAuthenticateToMount(
    const ResultCallback& success_callback,
    const UserContext& user_context) {
  RecordStartMarker("MountEx");

  std::string canonicalized = gaia::CanonicalizeEmail(user_context.GetUserID());
  cryptohome::Identification id(canonicalized);
  const Key* const key = user_context.GetKey();
  cryptohome::Authorization auth(key->GetSecret(), key->GetLabel());
  cryptohome::MountParameters mount(false);

  cryptohome::HomedirMethods::GetInstance()->MountEx(
      id,
      auth,
      mount,
      base::Bind(&ExtendedAuthenticator::OnMountComplete,
                 this,
                 "MountEx",
                 user_context,
                 success_callback));
}

void ExtendedAuthenticator::DoAuthenticateToCheck(
    const base::Closure& success_callback,
    const UserContext& user_context) {
  RecordStartMarker("CheckKeyEx");

  std::string canonicalized = gaia::CanonicalizeEmail(user_context.GetUserID());
  cryptohome::Identification id(canonicalized);
  const Key* const key = user_context.GetKey();
  cryptohome::Authorization auth(key->GetSecret(), key->GetLabel());

  cryptohome::HomedirMethods::GetInstance()->CheckKeyEx(
      id,
      auth,
      base::Bind(&ExtendedAuthenticator::OnOperationComplete,
                 this,
                 "CheckKeyEx",
                 user_context,
                 success_callback));
}

void ExtendedAuthenticator::DoAddKey(const cryptohome::KeyDefinition& key,
                                     bool replace_existing,
                                     const base::Closure& success_callback,
                                     const UserContext& user_context) {
  RecordStartMarker("AddKeyEx");

  std::string canonicalized = gaia::CanonicalizeEmail(user_context.GetUserID());
  cryptohome::Identification id(canonicalized);
  const Key* const auth_key = user_context.GetKey();
  cryptohome::Authorization auth(auth_key->GetSecret(), auth_key->GetLabel());

  cryptohome::HomedirMethods::GetInstance()->AddKeyEx(
      id,
      auth,
      key,
      replace_existing,
      base::Bind(&ExtendedAuthenticator::OnOperationComplete,
                 this,
                 "AddKeyEx",
                 user_context,
                 success_callback));
}

void ExtendedAuthenticator::DoUpdateKeyAuthorized(
    const cryptohome::KeyDefinition& key,
    const std::string& signature,
    const base::Closure& success_callback,
    const UserContext& user_context) {
  RecordStartMarker("UpdateKeyAuthorized");

  std::string canonicalized = gaia::CanonicalizeEmail(user_context.GetUserID());
  cryptohome::Identification id(canonicalized);
  const Key* const auth_key = user_context.GetKey();
  cryptohome::Authorization auth(auth_key->GetSecret(), auth_key->GetLabel());

  cryptohome::HomedirMethods::GetInstance()->UpdateKeyEx(
      id,
      auth,
      key,
      signature,
      base::Bind(&ExtendedAuthenticator::OnOperationComplete,
                 this,
                 "UpdateKeyAuthorized",
                 user_context,
                 success_callback));
}

void ExtendedAuthenticator::DoRemoveKey(const std::string& key_to_remove,
                                        const base::Closure& success_callback,
                                        const UserContext& user_context) {
  RecordStartMarker("RemoveKeyEx");

  std::string canonicalized = gaia::CanonicalizeEmail(user_context.GetUserID());
  cryptohome::Identification id(canonicalized);
  const Key* const auth_key = user_context.GetKey();
  cryptohome::Authorization auth(auth_key->GetSecret(), auth_key->GetLabel());

  cryptohome::HomedirMethods::GetInstance()->RemoveKeyEx(
      id,
      auth,
      key_to_remove,
      base::Bind(&ExtendedAuthenticator::OnOperationComplete,
                 this,
                 "RemoveKeyEx",
                 user_context,
                 success_callback));
}

void ExtendedAuthenticator::OnMountComplete(
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

void ExtendedAuthenticator::OnOperationComplete(
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
