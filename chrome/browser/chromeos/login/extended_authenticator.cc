// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/extended_authenticator.h"

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/login/login_status_consumer.h"
#include "chrome/browser/chromeos/login/parallel_authenticator.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/cryptohome/homedir_methods.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using content::BrowserThread;

namespace chromeos {

namespace {

void RecordStartMarker(const std::string& marker) {
  std::string full_marker = "Cryptohome-";
  full_marker.append(marker);
  full_marker.append("-Start");
  chromeos::BootTimesLoader::Get()->AddLoginTimeMarker(full_marker, false);
}

void RecordEndMarker(const std::string& marker) {
  std::string full_marker = "Cryptohome-";
  full_marker.append(marker);
  full_marker.append("-End");
  chromeos::BootTimesLoader::Get()->AddLoginTimeMarker(full_marker, false);
}

}  // namespace

ExtendedAuthenticator::ExtendedAuthenticator(AuthStatusConsumer* consumer)
    : salt_obtained_(false), consumer_(consumer), old_consumer_(NULL) {
  SystemSaltGetter::Get()->GetSystemSalt(
      base::Bind(&ExtendedAuthenticator::OnSaltObtained, this));
}

ExtendedAuthenticator::ExtendedAuthenticator(LoginStatusConsumer* consumer)
    : salt_obtained_(false), consumer_(NULL), old_consumer_(consumer) {
  SystemSaltGetter::Get()->GetSystemSalt(
      base::Bind(&ExtendedAuthenticator::OnSaltObtained, this));
}

ExtendedAuthenticator::~ExtendedAuthenticator() {}

void ExtendedAuthenticator::SetConsumer(LoginStatusConsumer* consumer) {
  old_consumer_ = consumer;
}

void ExtendedAuthenticator::OnSaltObtained(const std::string& system_salt) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  salt_obtained_ = true;
  system_salt_ = system_salt;
  for (size_t i = 0; i < hashing_queue_.size(); i++) {
    hashing_queue_[i].Run(system_salt);
  }
  hashing_queue_.clear();
}

void ExtendedAuthenticator::AuthenticateToMount(
    const UserContext& context,
    const HashSuccessCallback& success_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TransformContext(context,
                   base::Bind(&ExtendedAuthenticator::DoAuthenticateToMount,
                              this,
                              success_callback));
}

void ExtendedAuthenticator::AuthenticateToCheck(
    const UserContext& context,
    const base::Closure& success_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  TransformContext(context,
                   base::Bind(&ExtendedAuthenticator::DoAuthenticateToCheck,
                              this,
                              success_callback));
}

void ExtendedAuthenticator::CreateMount(
    const std::string& user_id,
    const std::vector<cryptohome::KeyDefinition>& keys,
    const HashSuccessCallback& success_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RecordStartMarker("MountEx");

  std::string canonicalized = gaia::CanonicalizeEmail(user_id);
  cryptohome::Identification id(canonicalized);
  cryptohome::Authorization auth(keys.front());
  cryptohome::MountParameters mount(false);
  for (size_t i = 0; i < keys.size(); i++) {
    mount.create_keys.push_back(keys[i]);
  }
  UserContext context(user_id, keys.front().key, std::string());
  context.key_label = keys.front().label;

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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TransformContext(context,
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TransformContext(context,
                   base::Bind(&ExtendedAuthenticator::DoUpdateKeyAuthorized,
                              this,
                              key,
                              signature,
                              success_callback));
}

void ExtendedAuthenticator::RemoveKey(const UserContext& context,
                                      const std::string& key_to_remove,
                                      const base::Closure& success_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  TransformContext(context,
                   base::Bind(&ExtendedAuthenticator::DoRemoveKey,
                              this,
                              key_to_remove,
                              success_callback));
}

void ExtendedAuthenticator::DoAuthenticateToMount(
    const HashSuccessCallback& success_callback,
    const UserContext& user_context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RecordStartMarker("MountEx");

  std::string canonicalized = gaia::CanonicalizeEmail(user_context.username);
  cryptohome::Identification id(canonicalized);
  cryptohome::Authorization auth(user_context.password, user_context.key_label);
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RecordStartMarker("CheckKeyEx");

  std::string canonicalized = gaia::CanonicalizeEmail(user_context.username);
  cryptohome::Identification id(canonicalized);
  cryptohome::Authorization auth(user_context.password, user_context.key_label);

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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RecordStartMarker("AddKeyEx");

  std::string canonicalized = gaia::CanonicalizeEmail(user_context.username);
  cryptohome::Identification id(canonicalized);
  cryptohome::Authorization auth(user_context.password, user_context.key_label);

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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RecordStartMarker("UpdateKeyAuthorized");

  std::string canonicalized = gaia::CanonicalizeEmail(user_context.username);
  cryptohome::Identification id(canonicalized);
  cryptohome::Authorization auth(user_context.password, user_context.key_label);

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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RecordStartMarker("RemoveKeyEx");

  std::string canonicalized = gaia::CanonicalizeEmail(user_context.username);
  cryptohome::Identification id(canonicalized);
  cryptohome::Authorization auth(user_context.password, user_context.key_label);

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
    const HashSuccessCallback& success_callback,
    bool success,
    cryptohome::MountError return_code,
    const std::string& mount_hash) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RecordEndMarker(time_marker);
  UserContext copy;
  copy.CopyFrom(user_context);
  copy.username_hash = mount_hash;
  if (return_code == cryptohome::MOUNT_ERROR_NONE) {
    if (!success_callback.is_null())
      success_callback.Run(mount_hash);
    if (old_consumer_)
      old_consumer_->OnLoginSuccess(copy);
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
    LoginFailure failure(LoginFailure::COULD_NOT_MOUNT_CRYPTOHOME);
    old_consumer_->OnLoginFailure(failure);
  }
}

void ExtendedAuthenticator::OnOperationComplete(
    const std::string& time_marker,
    const UserContext& user_context,
    const base::Closure& success_callback,
    bool success,
    cryptohome::MountError return_code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  RecordEndMarker(time_marker);
  if (return_code == cryptohome::MOUNT_ERROR_NONE) {
    success_callback.Run();
    if (old_consumer_)
      old_consumer_->OnLoginSuccess(user_context);
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
    LoginFailure failure(LoginFailure::UNLOCK_FAILED);
    old_consumer_->OnLoginFailure(failure);
  }
}

void ExtendedAuthenticator::HashPasswordWithSalt(
    const std::string& password,
    const HashSuccessCallback& success_callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(consumer_) << "This is a part of new API";

  DoHashWithSalt(password, success_callback, system_salt_);
}

void ExtendedAuthenticator::TransformContext(const UserContext& user_context,
                                             const ContextCallback& callback) {
  if (!user_context.need_password_hashing) {
    callback.Run(user_context);
  } else {
    DoHashWithSalt(user_context.password,
                   base::Bind(&ExtendedAuthenticator::DidTransformContext,
                              this,
                              user_context,
                              callback),
                   system_salt_);
  }
}

void ExtendedAuthenticator::DidTransformContext(
    const UserContext& user_context,
    const ContextCallback& callback,
    const std::string& hashed_password) {
  DCHECK(user_context.need_password_hashing);
  UserContext context;
  context.CopyFrom(user_context);
  context.password = hashed_password;
  context.need_password_hashing = false;
  callback.Run(context);
}

void ExtendedAuthenticator::DoHashWithSalt(const std::string& password,
                                           const HashSuccessCallback& callback,
                                           const std::string& system_salt) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (salt_obtained_) {
    std::string hash =
        ParallelAuthenticator::HashPassword(password, system_salt);
    callback.Run(hash);
    return;
  }
  hashing_queue_.push_back(base::Bind(
      &ExtendedAuthenticator::DoHashWithSalt, this, password, callback));
}

}  // namespace chromeos
