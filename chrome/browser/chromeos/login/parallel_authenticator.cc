// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/parallel_authenticator.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/chromeos/cros/cert_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/login/auth_response_handler.h"
#include "chrome/browser/chromeos/login/authentication_notification_details.h"
#include "chrome/browser/chromeos/login/login_status_consumer.h"
#include "chrome/browser/chromeos/login/ownership_service.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/gaia/gaia_auth_fetcher.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "crypto/encryptor.h"
#include "crypto/sha2.h"
#include "crypto/symmetric_key.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_status.h"
#include "third_party/libjingle/source/talk/base/urlencode.h"

using base::Time;
using base::TimeDelta;
using content::BrowserThread;
using file_util::GetFileSize;
using file_util::PathExists;
using file_util::ReadFile;
using file_util::ReadFileToString;

namespace {

const int kPassHashLen = 32;
const size_t kKeySize = 16;

// Decrypts (AES) hex encoded encrypted token given |key| and |salt|.
std::string DecryptTokenWithKey(
    crypto::SymmetricKey* key,
    const std::string& salt,
    const std::string& encrypted_token_hex) {
  std::vector<uint8> encrypted_token_bytes;
  if (!base::HexStringToBytes(encrypted_token_hex, &encrypted_token_bytes))
    return std::string();

  std::string encrypted_token(
      reinterpret_cast<char*>(encrypted_token_bytes.data()),
      encrypted_token_bytes.size());
  crypto::Encryptor encryptor;
  if (!encryptor.Init(key, crypto::Encryptor::CTR, std::string()))
    return std::string();

  std::string nonce = salt.substr(0, kKeySize);
  std::string token;
  CHECK(encryptor.SetCounter(nonce));
  if (!encryptor.Decrypt(encrypted_token, &token))
    return std::string();
  return token;
}

}   // namespace

namespace chromeos {

// static
const char ParallelAuthenticator::kLocalaccountFile[] = "localaccount";

// static
const int ParallelAuthenticator::kClientLoginTimeoutMs = 10000;
// static
const int ParallelAuthenticator::kLocalaccountRetryIntervalMs = 20;

ParallelAuthenticator::ParallelAuthenticator(LoginStatusConsumer* consumer)
    : Authenticator(consumer),
      already_reported_success_(false),
      checked_for_localaccount_(false),
      using_oauth_(
          CommandLine::ForCurrentProcess()->HasSwitch(
              switches::kWebUILogin) &&
          !CommandLine::ForCurrentProcess()->HasSwitch(
                  switches::kSkipOAuthLogin)) {
  // If not already owned, this is a no-op.  If it is, this loads the owner's
  // public key off of disk.
  OwnershipService::GetSharedInstance()->StartLoadOwnerKeyAttempt();
}

ParallelAuthenticator::~ParallelAuthenticator() {}

void ParallelAuthenticator::AuthenticateToLogin(
    Profile* profile,
    const std::string& username,
    const std::string& password,
    const std::string& login_token,
    const std::string& login_captcha) {
  std::string canonicalized = Authenticator::Canonicalize(username);
  authentication_profile_ = profile;
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
  // Sadly, this MUST be on the UI thread due to sending DBus traffic :-/
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,

      base::Bind(&CryptohomeOp::Initiate, mounter_.get()));
  // ClientLogin authentication check should happen immediately here.
  // We should not try OAuthLogin check until the profile loads.
  if (!using_oauth_) {
    // Initiate ClientLogin-based post authentication.
    current_online_ = new OnlineAttempt(using_oauth_,
                                        current_state_.get(),
                                        this);
    current_online_->Initiate(profile);
  }

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&ParallelAuthenticator::LoadLocalaccount, this,
                 std::string(kLocalaccountFile)));
}

void ParallelAuthenticator::CompleteLogin(Profile* profile,
                                          const std::string& username,
                                          const std::string& password) {
  std::string canonicalized = Authenticator::Canonicalize(username);
  authentication_profile_ = profile;
  current_state_.reset(
      new AuthAttemptState(canonicalized,
                           password,
                           HashPassword(password),
                           !UserManager::Get()->IsKnownUser(canonicalized)));
  mounter_ = CryptohomeOp::CreateMountAttempt(current_state_.get(),
                                              this,
                                              false /* don't create */);
  // Sadly, this MUST be on the UI thread due to sending DBus traffic :-/
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&CryptohomeOp::Initiate, mounter_.get()));

  if (!using_oauth_) {
    // Test automation needs to disable oauth, but that leads to other
    // services not being able to fetch a token, leading to browser crashes.
    // So initiate ClientLogin-based post authentication.
    // TODO(xiyuan): This should not be required.
    current_online_ = new OnlineAttempt(using_oauth_,
                                        current_state_.get(),
                                        this);
    current_online_->Initiate(profile);
  } else {
    // For login completion from extension, we just need to resolve the current
    // auth attempt state, the rest of OAuth related tasks will be done in
    // parallel.
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&ParallelAuthenticator::ResolveLoginCompletionStatus, this));
  }

  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&ParallelAuthenticator::LoadLocalaccount, this,
                 std::string(kLocalaccountFile)));
}

void ParallelAuthenticator::AuthenticateToUnlock(const std::string& username,
                                                 const std::string& password) {
  current_state_.reset(
      new AuthAttemptState(Authenticator::Canonicalize(username),
                           HashPassword(password)));
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&ParallelAuthenticator::LoadLocalaccount, this,
                 std::string(kLocalaccountFile)));
  key_checker_ = CryptohomeOp::CreateCheckKeyAttempt(current_state_.get(),
                                                     this);
  // Sadly, this MUST be on the UI thread due to sending DBus traffic :-/
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&CryptohomeOp::Initiate, key_checker_.get()));
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
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_AUTHENTICATION,
      content::NotificationService::AllSources(),
      content::Details<AuthenticationNotificationDetails>(&details));
  {
    base::AutoLock for_this_block(success_lock_);
    already_reported_success_ = true;
  }
  consumer_->OnLoginSuccess(current_state_->username,
                            current_state_->password,
                            credentials,
                            request_pending,
                            using_oauth_);
}

void ParallelAuthenticator::OnOffTheRecordLoginSuccess() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Send notification of success
  AuthenticationNotificationDetails details(true);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_AUTHENTICATION,
      content::NotificationService::AllSources(),
      content::Details<AuthenticationNotificationDetails>(&details));
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
          base::Bind(&ParallelAuthenticator::CheckLocalaccount, this, error),
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
          base::Bind(&CryptohomeOp::Initiate, guest_mounter_.get()));
    } else {
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&ParallelAuthenticator::OnLoginSuccess, this,
                     GaiaAuthConsumer::ClientLoginResult(), false));
    }
  } else {
    // Not the localaccount.  Fail, passing along cached error info.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&ParallelAuthenticator::OnLoginFailure, this, error));
  }
}

void ParallelAuthenticator::OnLoginFailure(const LoginFailure& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Send notification of failure
  AuthenticationNotificationDetails details(false);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_AUTHENTICATION,
      content::NotificationService::AllSources(),
      content::Details<AuthenticationNotificationDetails>(&details));
  LOG(WARNING) << "Login failed: " << error.GetErrorString();
  consumer_->OnLoginFailure(error);
}

void ParallelAuthenticator::RecordOAuthCheckFailure(
    const std::string& user_name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(using_oauth_);
  // Mark this account's OAuth token state as invalid in the local state.
  UserManager::Get()->SaveUserOAuthStatus(user_name,
                                          User::OAUTH_TOKEN_STATUS_INVALID);
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
      base::Bind(&ParallelAuthenticator::ResyncRecoverHelper, this,
                 key_migrator_));
}

void ParallelAuthenticator::ResyncEncryptedData(
    const GaiaAuthConsumer::ClientLoginResult& credentials) {
  data_remover_ =
      CryptohomeOp::CreateRemoveAttempt(current_state_.get(), this);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ParallelAuthenticator::ResyncRecoverHelper, this,
                 data_remover_));
}

void ParallelAuthenticator::ResyncRecoverHelper(CryptohomeOp* to_initiate) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  current_state_->ResetCryptohomeStatus();
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&CryptohomeOp::Initiate, to_initiate));
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
  // Always use ClientLogin regardless of using_oauth flag. This is because
  // we are unable to renew oauth token on lock screen currently and will
  // stuck with lock screen if we use OAuthLogin here.
  // TODO(xiyuan): Revisit this after we support Gaia in lock screen.
  current_online_ = new OnlineAttempt(false /* using_oauth */,
                                      reauth_state_.get(),
                                      this);
  current_online_->Initiate(profile);
}


void ParallelAuthenticator::VerifyOAuth1AccessToken(
    const std::string& oauth1_access_token, const std::string& oauth1_secret) {
  DCHECK(using_oauth_);
  current_state_->SetOAuth1Token(oauth1_access_token, oauth1_secret);
  // Initiate ClientLogin-based post authentication.
  current_online_ = new OnlineAttempt(using_oauth_,
                                      current_state_.get(),
                                      this);
  current_online_->Initiate(authentication_profile());
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
          base::Bind(&ParallelAuthenticator::OnLoginFailure, this,
                     LoginFailure(LoginFailure::COULD_NOT_MOUNT_CRYPTOHOME)));
      break;
    case FAILED_REMOVE:
      // In this case, we tried to remove the user's old cryptohome at her
      // request, and the remove failed.
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&ParallelAuthenticator::OnLoginFailure, this,
                     LoginFailure(LoginFailure::DATA_REMOVAL_FAILED)));
      break;
    case FAILED_TMPFS:
      // In this case, we tried to mount a tmpfs for BWSI or the localaccount
      // user and failed.
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&ParallelAuthenticator::OnLoginFailure, this,
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
          base::Bind(&CryptohomeOp::Initiate, mounter_.get()));
      break;
    case NEED_OLD_PW:
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&ParallelAuthenticator::OnPasswordChangeDetected, this,
                     current_state_->credentials()));
      break;
    case ONLINE_FAILED:
      // In this case, we know online login was rejected because the account
      // is disabled or something similarly fatal.  Sending the user through
      // the same path they get when their password is rejected is cleaner
      // for now.
      // TODO(cmasone): optimize this so that we don't send the user through
      // the 'changed password' path when we know doing so won't succeed.
    case NEED_NEW_PW: {
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
                base::Bind(&ParallelAuthenticator::OnLoginSuccess, this,
                           current_state_->credentials(), true));
          }
        }
        const LoginFailure& login_failure =
            reauth_state_.get() ? reauth_state_->online_outcome() :
                                  current_state_->online_outcome();
        BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            base::Bind(&ParallelAuthenticator::OnLoginFailure, this,
                       login_failure));
        // Check if we couldn't verify OAuth token here.
        if (using_oauth_ &&
            login_failure.reason() == LoginFailure::NETWORK_AUTH_FAILED) {
          BrowserThread::PostTask(
              BrowserThread::UI, FROM_HERE,
              base::Bind(&ParallelAuthenticator::RecordOAuthCheckFailure, this,
                         (reauth_state_.get() ? reauth_state_->username :
                             current_state_->username)));
        }
        break;
    }
    case HAVE_NEW_PW:
      key_migrator_ =
          CryptohomeOp::CreateMigrateAttempt(reauth_state_.get(),
                                             this,
                                             true,
                                             current_state_->ascii_hash);
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&CryptohomeOp::Initiate, key_migrator_.get()));
      break;
    case OFFLINE_LOGIN:
      VLOG(2) << "Offline login";
      // Marking request_pending to false when using OAuth because OAuth related
      // tasks are performed after user profile is mounted and are not performed
      // by ParallelAuthenticator.
      // TODO(xiyuan): Revert this when we support Gaia in lock screen and
      // start to use ParallelAuthenticator's VerifyOAuth1AccessToken again.
      request_pending = using_oauth_ ?
          false :
          !current_state_->online_complete();
      // Fall through.
    case UNLOCK:
      VLOG(2) << "Unlock";
      // Fall through.
    case ONLINE_LOGIN:
      VLOG(2) << "Online login";
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&ParallelAuthenticator::OnLoginSuccess, this,
                     current_state_->credentials(), request_pending));
      break;
    case LOCAL_LOGIN:
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&ParallelAuthenticator::OnOffTheRecordLoginSuccess, this));
      break;
    case LOGIN_FAILED:
      current_state_->ResetCryptohomeStatus();
      BrowserThread::PostTask(
          BrowserThread::FILE, FROM_HERE,
          base::Bind(&ParallelAuthenticator::CheckLocalaccount, this,
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

  // Return intermediate states in the following cases:
  // 1. When there is a parallel online attempt to resolve them later;
  //    This is the case with legacy ClientLogin flow;
  // 2. When there is an online result to use;
  //    This is the case after user finishes Gaia login;
  if (current_online_.get() || current_state_->online_complete()) {
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

namespace {
bool WasConnectionIssue(const LoginFailure& online_outcome) {
  return ((online_outcome.reason() == LoginFailure::LOGIN_TIMED_OUT) ||
          (online_outcome.error().state() ==
           GoogleServiceAuthError::CONNECTION_FAILED) ||
          (online_outcome.error().state() ==
           GoogleServiceAuthError::REQUEST_CANCELED));
}
}  // anonymous namespace

ParallelAuthenticator::AuthState
ParallelAuthenticator::ResolveOnlineFailureState(
    ParallelAuthenticator::AuthState offline_state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (offline_state == OFFLINE_LOGIN) {
    if (WasConnectionIssue(current_state_->online_outcome())) {
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

bool ParallelAuthenticator::LoadSupplementalUserKey() {
  if (!supplemental_user_key_.get()) {
    supplemental_user_key_.reset(
        CrosLibrary::Get()->GetCertLibrary()->GetSupplementalUserKey());
  }
  return supplemental_user_key_.get() != NULL;
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

std::string ParallelAuthenticator::EncryptToken(const std::string& token) {
  if (!LoadSupplementalUserKey())
    return std::string();
  crypto::Encryptor encryptor;
  if (!encryptor.Init(supplemental_user_key_.get(), crypto::Encryptor::CTR,
                      std::string()))
    return std::string();

  std::string nonce = SaltAsAscii().substr(0, kKeySize);
  std::string encoded_token;
  CHECK(encryptor.SetCounter(nonce));
  if (!encryptor.Encrypt(token, &encoded_token))
    return std::string();

  return StringToLowerASCII(base::HexEncode(
      reinterpret_cast<const void*>(encoded_token.data()),
      encoded_token.size()));
}
std::string ParallelAuthenticator::DecryptToken(
    const std::string& encrypted_token_hex) {
  if (!LoadSupplementalUserKey())
    return std::string();
  return DecryptTokenWithKey(supplemental_user_key_.get(),
                             SaltAsAscii(),
                             encrypted_token_hex);
}

std::string ParallelAuthenticator::HashPassword(const std::string& password) {
  // Get salt, ascii encode, update sha with that, then update with ascii
  // of password, then end.
  std::string ascii_salt = SaltAsAscii();
  char passhash_buf[kPassHashLen];

  // Hash salt and password
  crypto::SHA256HashString(ascii_salt + password,
                           &passhash_buf, sizeof(passhash_buf));

  return StringToLowerASCII(base::HexEncode(
      reinterpret_cast<const void*>(passhash_buf),
      sizeof(passhash_buf) / 2));
}

std::string ParallelAuthenticator::SaltAsAscii() {
  LoadSystemSalt();  // no-op if it's already loaded.
  return StringToLowerASCII(base::HexEncode(
      reinterpret_cast<const void*>(system_salt_.data()),
      system_salt_.size()));
}

void ParallelAuthenticator::ResolveLoginCompletionStatus() {
  // Shortcut online state resolution process.
  current_state_->RecordOnlineLoginStatus(GaiaAuthConsumer::ClientLoginResult(),
                                          LoginFailure::None());
  Resolve();
}

}  // namespace chromeos
