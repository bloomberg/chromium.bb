// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/managed/managed_user_authenticator.h"

#include "base/bind.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/sha2.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using content::BrowserThread;

namespace chromeos {

namespace {

// Milliseconds until we timeout our attempt to hit ClientLogin.
const int kClientLoginTimeoutMs = 10000;

// Length of password hashed with SHA-256.
const int kPasswordHashLength = 32;

// Records status and calls resolver->Resolve().
void TriggerResolve(ManagedUserAuthenticator::AuthAttempt* attempt,
                    scoped_refptr<ManagedUserAuthenticator> resolver,
                    bool success,
                    cryptohome::MountError return_code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  attempt->RecordCryptohomeStatus(success, return_code);
  resolver->Resolve();
}

// Records status and calls resolver->Resolve().
void TriggerResolveResult(ManagedUserAuthenticator::AuthAttempt* attempt,
                          scoped_refptr<ManagedUserAuthenticator> resolver,
                          bool success,
                          const std::string& result) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  attempt->RecordHash(result);
  resolver->Resolve();
}

// Calls TriggerResolve while adding login time marker.
void TriggerResolveWithLoginTimeMarker(
    const std::string& marker_name,
    ManagedUserAuthenticator::AuthAttempt* attempt,
    scoped_refptr<ManagedUserAuthenticator> resolver,
    bool success,
    cryptohome::MountError return_code) {
  chromeos::BootTimesLoader::Get()->AddLoginTimeMarker(marker_name, false);
  TriggerResolve(attempt, resolver, success, return_code);
}

// Calls cryptohome's mount method.
void Mount(ManagedUserAuthenticator::AuthAttempt* attempt,
           scoped_refptr<ManagedUserAuthenticator> resolver,
           int flags) {
  // TODO(antrim) : use additional mount function here.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  chromeos::BootTimesLoader::Get()->AddLoginTimeMarker(
      "CryptohomeMount-LMU-Start", false);
  cryptohome::AsyncMethodCaller::GetInstance()->AsyncMount(
      attempt->username,
      attempt->hashed_password,
      flags,
      base::Bind(&TriggerResolveWithLoginTimeMarker,
                 "CryptohomeMount-LMU-End",
                 attempt,
                 resolver));

  cryptohome::AsyncMethodCaller::GetInstance()->AsyncGetSanitizedUsername(
      attempt->username,
      base::Bind(&TriggerResolveResult, attempt, resolver));
}

// Returns hash of |password|, salted with the system salt.
std::string HashPassword(const std::string& password) {
  // Get salt, ascii encode, update sha with that, then update with ascii
  // of password, then end.
  std::string ascii_salt =
      CrosLibrary::Get()->GetCryptohomeLibrary()->GetSystemSalt();
  char passhash_buf[kPasswordHashLength];

  // Hash salt and password
  crypto::SHA256HashString(
      ascii_salt + password, &passhash_buf, sizeof(passhash_buf));

  // Only want the top half for 'weak' hashing so that the passphrase is not
  // immediately exposed even if the output is reversed.
  const int encoded_length = sizeof(passhash_buf) / 2;

  return StringToLowerASCII(base::HexEncode(
      reinterpret_cast<const void*>(passhash_buf), encoded_length));
}

}  // namespace

ManagedUserAuthenticator::ManagedUserAuthenticator(StatusConsumer* consumer)
    : consumer_(consumer) {}

void ManagedUserAuthenticator::AuthenticateToMount(const std::string& username,
                                               const std::string& password) {
  std::string canonicalized = gaia::CanonicalizeEmail(username);

  current_state_.reset(new ManagedUserAuthenticator::AuthAttempt(
      canonicalized, password, HashPassword(password)));

  BrowserThread::PostTask(BrowserThread::UI,
      FROM_HERE,
      base::Bind(&Mount,
          current_state_.get(),
          scoped_refptr<ManagedUserAuthenticator>(this),
          cryptohome::MOUNT_FLAGS_NONE));
}

void ManagedUserAuthenticator::AuthenticateToCreate(const std::string& username,
                                                const std::string& password) {

  std::string canonicalized = gaia::CanonicalizeEmail(username);

  current_state_.reset(new ManagedUserAuthenticator::AuthAttempt(
      canonicalized, password, HashPassword(password)));

  BrowserThread::PostTask(BrowserThread::UI,
      FROM_HERE,
      base::Bind(&Mount,
           current_state_.get(),
           scoped_refptr<ManagedUserAuthenticator>(this),
           cryptohome::CREATE_IF_MISSING));
}

void ManagedUserAuthenticator::OnAuthenticationSuccess(
    const std::string& mount_hash) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(1) << "Locally managed user authentication success";

  if (consumer_)
    consumer_->OnMountSuccess(mount_hash);
}

void ManagedUserAuthenticator::OnAuthenticationFailure(
    ManagedUserAuthenticator::AuthState state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  LOG(WARNING) << "Locally managed user authentication failure";
  if (consumer_)
    consumer_->OnAuthenticationFailure(state);
}

void ManagedUserAuthenticator::Resolve() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ManagedUserAuthenticator::AuthState state = ResolveState();
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
          base::Bind(
              &ManagedUserAuthenticator::OnAuthenticationFailure, this, state));
      break;
    case NO_MOUNT:
      // In this case, whether login succeeded or not, we can't log
      // the user in because no data exist. So, override with
      // the appropriate failure.
      BrowserThread::PostTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(
              &ManagedUserAuthenticator::OnAuthenticationFailure, this, state));
      break;
    case FAILED_TPM:
      // In this case, we tried to create/mount cryptohome and failed
      // because of the critical TPM error.
      // Chrome will notify user and request reboot.
      BrowserThread::PostTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(
              &ManagedUserAuthenticator::OnAuthenticationFailure, this, state));
      break;
    case SUCCESS:
      VLOG(2) << "Locally managed user login";
      BrowserThread::PostTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&ManagedUserAuthenticator::OnAuthenticationSuccess,
                     this,
                     current_state_->hash()));
      break;
    default:
      NOTREACHED();
      break;
  }
}

ManagedUserAuthenticator::~ManagedUserAuthenticator() {}

ManagedUserAuthenticator::AuthState ManagedUserAuthenticator::ResolveState() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // If we haven't mounted the user's home dir yet, we can't be done.
  // We never get past here if a cryptohome op is still pending.
  // This is an important invariant.
  if (!current_state_->cryptohome_complete())
    return CONTINUE;
  if (!current_state_->hash_obtained())
    return CONTINUE;

  AuthState state;

  if (current_state_->cryptohome_outcome())
    state = ResolveCryptohomeSuccessState();
  else
    state = ResolveCryptohomeFailureState();

  DCHECK(current_state_->cryptohome_complete());
  DCHECK(current_state_->hash_obtained());
  return state;
}

ManagedUserAuthenticator::AuthState
    ManagedUserAuthenticator::ResolveCryptohomeFailureState() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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

ManagedUserAuthenticator::AuthState
    ManagedUserAuthenticator::ResolveCryptohomeSuccessState() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return SUCCESS;
}

ManagedUserAuthenticator::AuthAttempt::AuthAttempt(const std::string& username,
                                               const std::string& password,
                                               const std::string& hashed)
    : username(username),
      password(password),
      hashed_password(hashed),
      cryptohome_complete_(false),
      cryptohome_outcome_(false),
      hash_obtained_(false),
      cryptohome_code_(cryptohome::MOUNT_ERROR_NONE) {}

ManagedUserAuthenticator::AuthAttempt::~AuthAttempt() {}

void ManagedUserAuthenticator::AuthAttempt::RecordCryptohomeStatus(
    bool cryptohome_outcome,
    cryptohome::MountError cryptohome_code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  cryptohome_complete_ = true;
  cryptohome_outcome_ = cryptohome_outcome;
  cryptohome_code_ = cryptohome_code;
}

void ManagedUserAuthenticator::AuthAttempt::RecordHash(
    const std::string& hash) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  hash_obtained_ = true;
  hash_ = hash;
}

bool ManagedUserAuthenticator::AuthAttempt::cryptohome_complete() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return cryptohome_complete_;
}

bool ManagedUserAuthenticator::AuthAttempt::cryptohome_outcome() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return cryptohome_outcome_;
}

cryptohome::MountError
    ManagedUserAuthenticator::AuthAttempt::cryptohome_code() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return cryptohome_code_;
}

bool ManagedUserAuthenticator::AuthAttempt::hash_obtained() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return hash_obtained_;
}

std::string ManagedUserAuthenticator::AuthAttempt::hash() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return hash_;
}

}  // namespace chromeos
