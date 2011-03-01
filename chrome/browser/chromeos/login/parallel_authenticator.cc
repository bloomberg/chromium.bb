// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/parallel_authenticator.h"

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/sha2.h"
#include "base/string_util.h"
#include "base/synchronization/lock.h"
#include "base/third_party/nss/blapi.h"
#include "base/third_party/nss/sha256.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/login/auth_response_handler.h"
#include "chrome/browser/chromeos/login/authentication_notification_details.h"
#include "chrome/browser/chromeos/login/login_status_consumer.h"
#include "chrome/browser/chromeos/login/ownership_service.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/net/gaia/gaia_auth_fetcher.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/notification_service.h"
#include "content/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_status.h"
#include "third_party/libjingle/source/talk/base/urlencode.h"

using base::Time;
using base::TimeDelta;
using file_util::GetFileSize;
using file_util::PathExists;
using file_util::ReadFile;
using file_util::ReadFileToString;

namespace chromeos {

// static
const char ParallelAuthenticator::kLocalaccountFile[] = "localaccount";

// static
const int ParallelAuthenticator::kClientLoginTimeoutMs = 10000;
// static
const int ParallelAuthenticator::kLocalaccountRetryIntervalMs = 20;

const int kPassHashLen = 32;

ParallelAuthenticator::ParallelAuthenticator(LoginStatusConsumer* consumer)
    : Authenticator(consumer),
      already_reported_success_(false),
      checked_for_localaccount_(false) {
  CHECK(chromeos::CrosLibrary::Get()->EnsureLoaded());
  // If not already owned, this is a no-op.  If it is, this loads the owner's
  // public key off of disk.
  OwnershipService::GetSharedInstance()->StartLoadOwnerKeyAttempt();
}

ParallelAuthenticator::~ParallelAuthenticator() {}

bool ParallelAuthenticator::AuthenticateToLogin(
    Profile* profile,
    const std::string& username,
    const std::string& password,
    const std::string& login_token,
    const std::string& login_captcha) {
  std::string canonicalized = Authenticator::Canonicalize(username);
  current_state_.reset(
      new AuthAttemptState(canonicalized,
                           password,
                           HashPassword(password),
                           login_token,
                           login_captcha,
                           !UserManager::Get()->IsKnownUser(canonicalized)));
  mounter_ = CryptohomeOp::CreateMountAttempt(current_state_.get(),
                                              this,
                                              false /* don't create */);
  current_online_ = new OnlineAttempt(current_state_.get(), this);
  // Sadly, this MUST be on the UI thread due to sending DBus traffic :-/
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(mounter_.get(), &CryptohomeOp::Initiate));
  current_online_->Initiate(profile);
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
                        &ParallelAuthenticator::LoadLocalaccount,
                        std::string(kLocalaccountFile)));
  return true;
}

bool ParallelAuthenticator::AuthenticateToUnlock(const std::string& username,
                                                 const std::string& password) {
  current_state_.reset(
      new AuthAttemptState(Authenticator::Canonicalize(username),
                           HashPassword(password)));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      NewRunnableMethod(this,
                        &ParallelAuthenticator::LoadLocalaccount,
                        std::string(kLocalaccountFile)));
  key_checker_ = CryptohomeOp::CreateCheckKeyAttempt(current_state_.get(),
                                                     this);
  // Sadly, this MUST be on the UI thread due to sending DBus traffic :-/
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(key_checker_.get(), &CryptohomeOp::Initiate));
  return true;
}

void ParallelAuthenticator::LoginOffTheRecord() {
  current_state_.reset(new AuthAttemptState("", "", "", "", "", false));
  guest_mounter_ =
      CryptohomeOp::CreateMountGuestAttempt(current_state_.get(), this);
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  guest_mounter_->Initiate();
}

void ParallelAuthenticator::OnLoginSuccess(
    const GaiaAuthConsumer::ClientLoginResult& credentials,
    bool request_pending) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(1) << "Login success";
  // Send notification of success
  AuthenticationNotificationDetails details(true);
  NotificationService::current()->Notify(
      NotificationType::LOGIN_AUTHENTICATION,
      NotificationService::AllSources(),
      Details<AuthenticationNotificationDetails>(&details));
  {
    base::AutoLock for_this_block(success_lock_);
    already_reported_success_ = true;
  }
  consumer_->OnLoginSuccess(current_state_->username,
                            current_state_->password,
                            credentials,
                            request_pending);
}

void ParallelAuthenticator::OnOffTheRecordLoginSuccess() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Send notification of success
  AuthenticationNotificationDetails details(true);
  NotificationService::current()->Notify(
      NotificationType::LOGIN_AUTHENTICATION,
      NotificationService::AllSources(),
      Details<AuthenticationNotificationDetails>(&details));
  consumer_->OnOffTheRecordLoginSuccess();
}

void ParallelAuthenticator::OnPasswordChangeDetected(
    const GaiaAuthConsumer::ClientLoginResult& credentials) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  consumer_->OnPasswordChangeDetected(credentials);
}

void ParallelAuthenticator::CheckLocalaccount(const LoginFailure& error) {
  {
    base::AutoLock for_this_block(localaccount_lock_);
    VLOG(2) << "Checking localaccount";
    if (!checked_for_localaccount_) {
      BrowserThread::PostDelayedTask(
          BrowserThread::FILE, FROM_HERE,
          NewRunnableMethod(this,
                            &ParallelAuthenticator::CheckLocalaccount,
                            error),
          kLocalaccountRetryIntervalMs);
      return;
    }
  }

  if (!localaccount_.empty() && localaccount_ == current_state_->username) {
    // Success.  Go mount a tmpfs for the profile, if necessary.
    if (!current_state_->unlock) {
      guest_mounter_ =
          CryptohomeOp::CreateMountGuestAttempt(current_state_.get(), this);
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(guest_mounter_.get(), &CryptohomeOp::Initiate));
    } else {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(this, &ParallelAuthenticator::OnLoginSuccess,
                            GaiaAuthConsumer::ClientLoginResult(), false));
    }
  } else {
    // Not the localaccount.  Fail, passing along cached error info.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        NewRunnableMethod(this, &ParallelAuthenticator::OnLoginFailure, error));
  }
}

void ParallelAuthenticator::OnLoginFailure(const LoginFailure& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Send notification of failure
  AuthenticationNotificationDetails details(false);
  NotificationService::current()->Notify(
      NotificationType::LOGIN_AUTHENTICATION,
      NotificationService::AllSources(),
      Details<AuthenticationNotificationDetails>(&details));
  LOG(WARNING) << "Login failed: " << error.GetErrorString();
  consumer_->OnLoginFailure(error);
}

void ParallelAuthenticator::RecoverEncryptedData(
    const std::string& old_password,
    const GaiaAuthConsumer::ClientLoginResult& credentials) {
  std::string old_hash = HashPassword(old_password);
  key_migrator_ = CryptohomeOp::CreateMigrateAttempt(current_state_.get(),
                                                     this,
                                                     true,
                                                     old_hash);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this,
                        &ParallelAuthenticator::ResyncRecoverHelper,
                        key_migrator_));
}

void ParallelAuthenticator::ResyncEncryptedData(
    const GaiaAuthConsumer::ClientLoginResult& credentials) {
  data_remover_ =
      CryptohomeOp::CreateRemoveAttempt(current_state_.get(), this);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this,
                        &ParallelAuthenticator::ResyncRecoverHelper,
                        data_remover_));
}

void ParallelAuthenticator::ResyncRecoverHelper(CryptohomeOp* to_initiate) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  current_state_->ResetCryptohomeStatus();
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      NewRunnableMethod(to_initiate, &CryptohomeOp::Initiate));
}

void ParallelAuthenticator::RetryAuth(Profile* profile,
                                      const std::string& username,
                                      const std::string& password,
                                      const std::string& login_token,
                                      const std::string& login_captcha) {
  reauth_state_.reset(
      new AuthAttemptState(Authenticator::Canonicalize(username),
                           password,
                           HashPassword(password),
                           login_token,
                           login_captcha,
                           false /* not a new user */));
  current_online_ = new OnlineAttempt(reauth_state_.get(), this);
  current_online_->Initiate(profile);
}

void ParallelAuthenticator::Resolve() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  bool request_pending = false;
  bool create = false;
  ParallelAuthenticator::AuthState state = ResolveState();
  VLOG(1) << "Resolved state to: " << state;
  switch (state) {
    case CONTINUE:
    case POSSIBLE_PW_CHANGE:
    case NO_MOUNT:
      // These are intermediate states; we need more info from a request that
      // is still pending.
      break;
    case FAILED_MOUNT:
      // In this case, whether login succeeded or not, we can't log
      // the user in because their data is horked.  So, override with
      // the appropriate failure.
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(
              this,
              &ParallelAuthenticator::OnLoginFailure,
              LoginFailure(LoginFailure::COULD_NOT_MOUNT_CRYPTOHOME)));
      break;
    case FAILED_REMOVE:
      // In this case, we tried to remove the user's old cryptohome at her
      // request, and the remove failed.
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(this, &ParallelAuthenticator::OnLoginFailure,
                            LoginFailure(LoginFailure::DATA_REMOVAL_FAILED)));
      break;
    case FAILED_TMPFS:
      // In this case, we tried to mount a tmpfs for BWSI or the localaccount
      // user and failed.
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(this, &ParallelAuthenticator::OnLoginFailure,
                            LoginFailure(LoginFailure::COULD_NOT_MOUNT_TMPFS)));
      break;
    case CREATE_NEW:
      create = true;
    case RECOVER_MOUNT:
      current_state_->ResetCryptohomeStatus();
      mounter_ = CryptohomeOp::CreateMountAttempt(current_state_.get(),
                                                  this,
                                                  create);
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(mounter_.get(), &CryptohomeOp::Initiate));
      break;
    case NEED_OLD_PW:
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(this,
                            &ParallelAuthenticator::OnPasswordChangeDetected,
                            current_state_->credentials()));
      break;
    case ONLINE_FAILED:
      // In this case, we know online login was rejected because the account
      // is disabled or something similarly fatal.  Sending the user through
      // the same path they get when their password is rejected is cleaner
      // for now.
      // TODO(cmasone): optimize this so that we don't send the user through
      // the 'changed password' path when we know doing so won't succeed.
    case NEED_NEW_PW:
      {
        base::AutoLock for_this_block(success_lock_);
        if (!already_reported_success_) {
          // This allows us to present the same behavior for "online:
          // fail, offline: ok", regardless of the order in which we
          // receive the results.  There will be cases in which we get
          // the online failure some time after the offline success,
          // so we just force all cases in this category to present like this:
          // OnLoginSuccess(..., ..., true) -> OnLoginFailure().
          BrowserThread::PostTask(
              BrowserThread::UI, FROM_HERE,
              NewRunnableMethod(this, &ParallelAuthenticator::OnLoginSuccess,
                                current_state_->credentials(), true));
        }
      }
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(this, &ParallelAuthenticator::OnLoginFailure,
                            (reauth_state_.get() ?
                             reauth_state_->online_outcome() :
                             current_state_->online_outcome())));
      break;
    case HAVE_NEW_PW:
      key_migrator_ =
          CryptohomeOp::CreateMigrateAttempt(reauth_state_.get(),
                                             this,
                                             true,
                                             current_state_->ascii_hash);
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(key_migrator_.get(), &CryptohomeOp::Initiate));
      break;
    case OFFLINE_LOGIN:
      VLOG(2) << "Offline login";
      request_pending = !current_state_->online_complete();
      // Fall through.
    case UNLOCK:
      // Fall through.
    case ONLINE_LOGIN:
      VLOG(2) << "Online login";
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(this, &ParallelAuthenticator::OnLoginSuccess,
                            current_state_->credentials(), request_pending));
      break;
    case LOCAL_LOGIN:
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          NewRunnableMethod(
              this,
              &ParallelAuthenticator::OnOffTheRecordLoginSuccess));
      break;
    case LOGIN_FAILED:
      current_state_->ResetCryptohomeStatus();
      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          NewRunnableMethod(this, &ParallelAuthenticator::CheckLocalaccount,
                            current_state_->online_outcome()));
      break;
    default:
      NOTREACHED();
      break;
  }
}

ParallelAuthenticator::AuthState ParallelAuthenticator::ResolveState() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  // If we haven't mounted the user's home dir yet, we can't be done.
  // We never get past here if a cryptohome op is still pending.
  // This is an important invariant.
  if (!current_state_->cryptohome_complete())
    return CONTINUE;

  AuthState state = (reauth_state_.get() ? ResolveReauthState() : CONTINUE);
  if (state != CONTINUE)
    return state;

  if (current_state_->cryptohome_outcome())
    state = ResolveCryptohomeSuccessState();
  else
    state = ResolveCryptohomeFailureState();

  DCHECK(current_state_->cryptohome_complete());  // Ensure invariant holds.
  key_migrator_ = NULL;
  data_remover_ = NULL;
  guest_mounter_ = NULL;
  key_checker_ = NULL;

  if (state != POSSIBLE_PW_CHANGE &&
      state != NO_MOUNT &&
      state != OFFLINE_LOGIN)
    return state;

  if (current_state_->online_complete()) {
    if (current_state_->online_outcome().reason() == LoginFailure::NONE) {
      // Online attempt succeeded as well, so combine the results.
      return ResolveOnlineSuccessState(state);
    }
    // Online login attempt was rejected or failed to occur.
    return ResolveOnlineFailureState(state);
  }
  // if online isn't complete yet, just return the offline result.
  return state;
}

ParallelAuthenticator::AuthState
ParallelAuthenticator::ResolveReauthState() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (reauth_state_->cryptohome_complete()) {
    if (!reauth_state_->cryptohome_outcome()) {
      // If we've tried to migrate and failed, log the error and just wait
      // til next time the user logs in to migrate their cryptohome key.
      LOG(ERROR) << "Failed to migrate cryptohome key: "
                 << reauth_state_->cryptohome_code();
    }
    reauth_state_.reset(NULL);
    return ONLINE_LOGIN;
  }
  // Haven't tried the migrate yet, must be processing the online auth attempt.
  if (!reauth_state_->online_complete()) {
    NOTREACHED();  // Shouldn't be here at all, if online reauth isn't done!
    return CONTINUE;
  }
  return (reauth_state_->online_outcome().reason() == LoginFailure::NONE) ?
      HAVE_NEW_PW : NEED_NEW_PW;
}

ParallelAuthenticator::AuthState
ParallelAuthenticator::ResolveCryptohomeFailureState() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (data_remover_.get())
    return FAILED_REMOVE;
  if (guest_mounter_.get())
    return FAILED_TMPFS;
  if (key_migrator_.get())
    return NEED_OLD_PW;
  if (key_checker_.get())
    return LOGIN_FAILED;
  if (current_state_->cryptohome_code() ==
      chromeos::kCryptohomeMountErrorKeyFailure) {
    // If we tried a mount but they used the wrong key, we may need to
    // ask the user for her old password.  We'll only know once we've
    // done the online check.
    return POSSIBLE_PW_CHANGE;
  }
  if (current_state_->cryptohome_code() ==
      chromeos::kCryptohomeMountErrorUserDoesNotExist) {
    // If we tried a mount but the user did not exist, then we should wait
    // for online login to succeed and try again with the "create" flag set.
    return NO_MOUNT;
  }
  return FAILED_MOUNT;
}

ParallelAuthenticator::AuthState
ParallelAuthenticator::ResolveCryptohomeSuccessState() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (data_remover_.get())
    return CREATE_NEW;
  if (guest_mounter_.get())
    return LOCAL_LOGIN;
  if (key_migrator_.get())
    return RECOVER_MOUNT;
  if (key_checker_.get())
    return UNLOCK;
  return OFFLINE_LOGIN;
}

ParallelAuthenticator::AuthState
ParallelAuthenticator::ResolveOnlineFailureState(
    ParallelAuthenticator::AuthState offline_state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (offline_state == OFFLINE_LOGIN) {
    if (current_state_->online_outcome().error().state() ==
        GoogleServiceAuthError::CONNECTION_FAILED) {
      // Couldn't do an online check, so just go with the offline result.
      return OFFLINE_LOGIN;
    }
    // Otherwise, online login was rejected!
    if (current_state_->online_outcome().error().state() ==
        GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS) {
      return NEED_NEW_PW;
    }
    return ONLINE_FAILED;
  }
  return LOGIN_FAILED;
}

ParallelAuthenticator::AuthState
ParallelAuthenticator::ResolveOnlineSuccessState(
    ParallelAuthenticator::AuthState offline_state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  switch (offline_state) {
    case POSSIBLE_PW_CHANGE:
      return NEED_OLD_PW;
    case NO_MOUNT:
      return CREATE_NEW;
    case OFFLINE_LOGIN:
      return ONLINE_LOGIN;
    default:
      NOTREACHED();
      return offline_state;
  }
}

void ParallelAuthenticator::LoadSystemSalt() {
  if (!system_salt_.empty())
    return;
  system_salt_ = CrosLibrary::Get()->GetCryptohomeLibrary()->GetSystemSalt();
  CHECK(!system_salt_.empty());
  CHECK_EQ(system_salt_.size() % 2, 0U);
}

void ParallelAuthenticator::LoadLocalaccount(const std::string& filename) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  {
    base::AutoLock for_this_block(localaccount_lock_);
    if (checked_for_localaccount_)
      return;
  }
  FilePath localaccount_file;
  std::string localaccount;
  if (PathService::Get(base::DIR_EXE, &localaccount_file)) {
    localaccount_file = localaccount_file.Append(filename);
    VLOG(2) << "Looking for localaccount in " << localaccount_file.value();

    ReadFileToString(localaccount_file, &localaccount);
    TrimWhitespaceASCII(localaccount, TRIM_TRAILING, &localaccount);
    VLOG(1) << "Loading localaccount: " << localaccount;
  } else {
    VLOG(1) << "Assuming no localaccount";
  }
  SetLocalaccount(localaccount);
}

void ParallelAuthenticator::SetLocalaccount(const std::string& new_name) {
  localaccount_ = new_name;
  {  // extra braces for clarity about AutoLock scope.
    base::AutoLock for_this_block(localaccount_lock_);
    checked_for_localaccount_ = true;
  }
}


std::string ParallelAuthenticator::HashPassword(const std::string& password) {
  // Get salt, ascii encode, update sha with that, then update with ascii
  // of password, then end.
  std::string ascii_salt = SaltAsAscii();
  unsigned char passhash_buf[kPassHashLen];
  char ascii_buf[kPassHashLen + 1];

  // Hash salt and password
  SHA256Context ctx;
  SHA256_Begin(&ctx);
  SHA256_Update(&ctx,
                reinterpret_cast<const unsigned char*>(ascii_salt.data()),
                static_cast<unsigned int>(ascii_salt.length()));
  SHA256_Update(&ctx,
                reinterpret_cast<const unsigned char*>(password.data()),
                static_cast<unsigned int>(password.length()));
  SHA256_End(&ctx,
             passhash_buf,
             NULL,
             static_cast<unsigned int>(sizeof(passhash_buf)));

  std::vector<unsigned char> passhash(passhash_buf,
                                      passhash_buf + sizeof(passhash_buf));
  BinaryToHex(passhash,
              passhash.size() / 2,  // only want top half, at least for now.
              ascii_buf,
              sizeof(ascii_buf));
  return std::string(ascii_buf, sizeof(ascii_buf) - 1);
}

std::string ParallelAuthenticator::SaltAsAscii() {
  LoadSystemSalt();  // no-op if it's already loaded.
  unsigned int salt_len = system_salt_.size();
  char ascii_salt[2 * salt_len + 1];
  if (ParallelAuthenticator::BinaryToHex(system_salt_,
                                       salt_len,
                                       ascii_salt,
                                       sizeof(ascii_salt))) {
    return std::string(ascii_salt, sizeof(ascii_salt) - 1);
  }
  return std::string();
}

// static
bool ParallelAuthenticator::BinaryToHex(
    const std::vector<unsigned char>& binary,
    const unsigned int binary_len,
    char* hex_string,
    const unsigned int len) {
  if (len < 2*binary_len)
    return false;
  memset(hex_string, 0, len);
  for (uint i = 0, j = 0; i < binary_len; i++, j+=2)
    snprintf(hex_string + j, len - j, "%02x", binary[i]);
  return true;
}

}  // namespace chromeos
