// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/supervised/supervised_user_authenticator.h"

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/cryptohome/cryptohome_parameters.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/login/auth/key.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using content::BrowserThread;

namespace chromeos {

namespace {

// Records status and calls resolver->Resolve().
void TriggerResolve(SupervisedUserAuthenticator::AuthAttempt* attempt,
                    scoped_refptr<SupervisedUserAuthenticator> resolver,
                    bool success,
                    cryptohome::MountError return_code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  attempt->RecordCryptohomeStatus(success, return_code);
  resolver->Resolve();
}

// Records status and calls resolver->Resolve().
void TriggerResolveResult(SupervisedUserAuthenticator::AuthAttempt* attempt,
                          scoped_refptr<SupervisedUserAuthenticator> resolver,
                          bool success,
                          const std::string& result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  attempt->RecordHash(result);
  resolver->Resolve();
}

// Calls TriggerResolve while adding login time marker.
void TriggerResolveWithLoginTimeMarker(
    const std::string& marker_name,
    SupervisedUserAuthenticator::AuthAttempt* attempt,
    scoped_refptr<SupervisedUserAuthenticator> resolver,
    bool success,
    cryptohome::MountError return_code) {
  chromeos::BootTimesLoader::Get()->AddLoginTimeMarker(marker_name, false);
  TriggerResolve(attempt, resolver, success, return_code);
}

// Calls cryptohome's mount method.
void Mount(SupervisedUserAuthenticator::AuthAttempt* attempt,
           scoped_refptr<SupervisedUserAuthenticator> resolver,
           int flags,
           const std::string& system_salt) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  chromeos::BootTimesLoader::Get()->AddLoginTimeMarker(
      "CryptohomeMount-LMU-Start", false);

  Key key(attempt->password);
  key.Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF, system_salt);
  cryptohome::AsyncMethodCaller::GetInstance()->AsyncMount(
      attempt->username,
      key.GetSecret(),
      flags,
      base::Bind(&TriggerResolveWithLoginTimeMarker,
                 "CryptohomeMount-LMU-End",
                 attempt,
                 resolver));

  cryptohome::AsyncMethodCaller::GetInstance()->AsyncGetSanitizedUsername(
      attempt->username,
      base::Bind(&TriggerResolveResult, attempt, resolver));
}

// Calls cryptohome's addKey method.
void AddKey(SupervisedUserAuthenticator::AuthAttempt* attempt,
            scoped_refptr<SupervisedUserAuthenticator> resolver,
            const std::string& plain_text_master_key,
            const std::string& system_salt) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  chromeos::BootTimesLoader::Get()->AddLoginTimeMarker(
      "CryptohomeAddKey-LMU-Start", false);

  Key user_key(attempt->password);
  user_key.Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF, system_salt);
  Key master_key(plain_text_master_key);
  master_key.Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF, system_salt);
  cryptohome::AsyncMethodCaller::GetInstance()->AsyncAddKey(
      attempt->username,
      user_key.GetSecret(),
      master_key.GetSecret(),
      base::Bind(&TriggerResolveWithLoginTimeMarker,
                 "CryptohomeAddKey-LMU-End",
                 attempt,
                 resolver));
}

}  // namespace

SupervisedUserAuthenticator::SupervisedUserAuthenticator(
    AuthStatusConsumer* consumer)
    : consumer_(consumer) {}

void SupervisedUserAuthenticator::AuthenticateToMount(
    const std::string& username,
    const std::string& password) {
  std::string canonicalized = gaia::CanonicalizeEmail(username);

  current_state_.reset(new SupervisedUserAuthenticator::AuthAttempt(
      canonicalized, password, false));

  SystemSaltGetter::Get()->GetSystemSalt(
      base::Bind(&Mount,
                 current_state_.get(),
                 scoped_refptr<SupervisedUserAuthenticator>(this),
                 cryptohome::MOUNT_FLAGS_NONE));
}

void SupervisedUserAuthenticator::AuthenticateToCreate(
    const std::string& username,
    const std::string& password) {
  std::string canonicalized = gaia::CanonicalizeEmail(username);

  current_state_.reset(new SupervisedUserAuthenticator::AuthAttempt(
      canonicalized, password, false));

  SystemSaltGetter::Get()->GetSystemSalt(
      base::Bind(&Mount,
                 current_state_.get(),
                 scoped_refptr<SupervisedUserAuthenticator>(this),
                 cryptohome::CREATE_IF_MISSING));
}

void SupervisedUserAuthenticator::AddMasterKey(
    const std::string& username,
    const std::string& password,
    const std::string& master_key) {
  std::string canonicalized = gaia::CanonicalizeEmail(username);

  current_state_.reset(new SupervisedUserAuthenticator::AuthAttempt(
      canonicalized, password, true));

  SystemSaltGetter::Get()->GetSystemSalt(
      base::Bind(&AddKey,
                 current_state_.get(),
                 scoped_refptr<SupervisedUserAuthenticator>(this),
                 master_key));
}

void SupervisedUserAuthenticator::OnAuthenticationSuccess(
    const std::string& mount_hash,
    bool add_key) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(1) << "Supervised user authentication success";
  if (consumer_) {
    if (add_key)
      consumer_->OnAddKeySuccess();
    else
      consumer_->OnMountSuccess(mount_hash);
  }
}

void SupervisedUserAuthenticator::OnAuthenticationFailure(
    SupervisedUserAuthenticator::AuthState state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  LOG(WARNING) << "Supervised user authentication failure";
  if (consumer_)
    consumer_->OnAuthenticationFailure(state);
}

void SupervisedUserAuthenticator::Resolve() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SupervisedUserAuthenticator::AuthState state = ResolveState();
  VLOG(1) << "Resolved state to: " << state;
  switch (state) {
    case CONTINUE:
      // These are intermediate states; we need more info from a request that
      // is still pending.
      break;
    case FAILED_MOUNT:
      // In this case, whether login succeeded or not, we can't log
      // the user in because their data is horked.  So, override with
      // the appropriate failure.
      BrowserThread::PostTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&SupervisedUserAuthenticator::OnAuthenticationFailure,
                     this,
                     state));
      break;
    case NO_MOUNT:
      // In this case, whether login succeeded or not, we can't log
      // the user in because no data exist. So, override with
      // the appropriate failure.
      BrowserThread::PostTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&SupervisedUserAuthenticator::OnAuthenticationFailure,
                     this,
                     state));
      break;
    case FAILED_TPM:
      // In this case, we tried to create/mount cryptohome and failed
      // because of the critical TPM error.
      // Chrome will notify user and request reboot.
      BrowserThread::PostTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&SupervisedUserAuthenticator::OnAuthenticationFailure,
                     this,
                     state));
      break;
    case SUCCESS:
      VLOG(2) << "Supervised user login";
      BrowserThread::PostTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&SupervisedUserAuthenticator::OnAuthenticationSuccess,
                     this,
                     current_state_->hash(),
                     current_state_->add_key));
      break;
    default:
      NOTREACHED();
      break;
  }
}

SupervisedUserAuthenticator::~SupervisedUserAuthenticator() {}

SupervisedUserAuthenticator::AuthState
SupervisedUserAuthenticator::ResolveState() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // If we haven't mounted the user's home dir yet, we can't be done.
  // We never get past here if a cryptohome op is still pending.
  // This is an important invariant.
  if (!current_state_->cryptohome_complete())
    return CONTINUE;
  if (!current_state_->add_key && !current_state_->hash_obtained())
    return CONTINUE;

  AuthState state;

  if (current_state_->cryptohome_outcome())
    state = ResolveCryptohomeSuccessState();
  else
    state = ResolveCryptohomeFailureState();

  DCHECK(current_state_->cryptohome_complete());
  DCHECK(current_state_->hash_obtained() || current_state_->add_key);
  return state;
}

SupervisedUserAuthenticator::AuthState
    SupervisedUserAuthenticator::ResolveCryptohomeFailureState() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  LOG(ERROR) << "Failed to authenticate supervised user, code: "
             << current_state_->cryptohome_code();
  if (current_state_->cryptohome_code() ==
      cryptohome::MOUNT_ERROR_TPM_NEEDS_REBOOT) {
    // Critical TPM error detected, reboot needed.
    return FAILED_TPM;
  }

  if (current_state_->cryptohome_code() ==
      cryptohome::MOUNT_ERROR_USER_DOES_NOT_EXIST) {
    // If we tried a mount but the user did not exist, then we should wait
    // for online login to succeed and try again with the "create" flag set.
    return NO_MOUNT;
  }

  return FAILED_MOUNT;
}

SupervisedUserAuthenticator::AuthState
    SupervisedUserAuthenticator::ResolveCryptohomeSuccessState() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return SUCCESS;
}

SupervisedUserAuthenticator::AuthAttempt::AuthAttempt(
    const std::string& username,
    const std::string& password,
    bool add_key_attempt)
    : username(username),
      password(password),
      add_key(add_key_attempt),
      cryptohome_complete_(false),
      cryptohome_outcome_(false),
      hash_obtained_(false),
      cryptohome_code_(cryptohome::MOUNT_ERROR_NONE) {}

SupervisedUserAuthenticator::AuthAttempt::~AuthAttempt() {}

void SupervisedUserAuthenticator::AuthAttempt::RecordCryptohomeStatus(
    bool cryptohome_outcome,
    cryptohome::MountError cryptohome_code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  cryptohome_complete_ = true;
  cryptohome_outcome_ = cryptohome_outcome;
  cryptohome_code_ = cryptohome_code;
}

void SupervisedUserAuthenticator::AuthAttempt::RecordHash(
    const std::string& hash) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  hash_obtained_ = true;
  hash_ = hash;
}

bool SupervisedUserAuthenticator::AuthAttempt::cryptohome_complete() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return cryptohome_complete_;
}

bool SupervisedUserAuthenticator::AuthAttempt::cryptohome_outcome() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return cryptohome_outcome_;
}

cryptohome::MountError
    SupervisedUserAuthenticator::AuthAttempt::cryptohome_code() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return cryptohome_code_;
}

bool SupervisedUserAuthenticator::AuthAttempt::hash_obtained() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return hash_obtained_;
}

std::string SupervisedUserAuthenticator::AuthAttempt::hash() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return hash_;
}

}  // namespace chromeos
