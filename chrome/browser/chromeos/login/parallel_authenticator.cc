// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/parallel_authenticator.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/cros/cert_library.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/cryptohome_library.h"
#include "chrome/browser/chromeos/login/authentication_notification_details.h"
#include "chrome/browser/chromeos/login/login_status_consumer.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
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
void TriggerResolve(AuthAttemptState* attempt,
                    scoped_refptr<ParallelAuthenticator> resolver,
                    bool success,
                    cryptohome::MountError return_code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  attempt->RecordCryptohomeStatus(success, return_code);
  resolver->Resolve();
}

// Calls TriggerResolve while adding login time marker.
void TriggerResolveWithLoginTimeMarker(
    const std::string& marker_name,
    AuthAttemptState* attempt,
    scoped_refptr<ParallelAuthenticator> resolver,
    bool success,
    cryptohome::MountError return_code) {
  chromeos::BootTimesLoader::Get()->AddLoginTimeMarker(marker_name, false);
  TriggerResolve(attempt, resolver, success, return_code);
}

// Calls cryptohome's mount method.
void Mount(AuthAttemptState* attempt,
           scoped_refptr<ParallelAuthenticator> resolver,
           int flags) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  chromeos::BootTimesLoader::Get()->AddLoginTimeMarker(
      "CryptohomeMount-Start", false);
  cryptohome::AsyncMethodCaller::GetInstance()->AsyncMount(
      attempt->username,
      attempt->ascii_hash,
      flags,
      base::Bind(&TriggerResolveWithLoginTimeMarker,
                 "CryptohomeMount-End",
                 attempt,
                 resolver));
}

// Calls cryptohome's mount method for guest.
void MountGuest(AuthAttemptState* attempt,
                scoped_refptr<ParallelAuthenticator> resolver) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  cryptohome::AsyncMethodCaller::GetInstance()->AsyncMountGuest(
      base::Bind(&TriggerResolveWithLoginTimeMarker,
                 "CryptohomeMount-End",
                 attempt,
                 resolver));
}

// Calls cryptohome's key migration method.
void Migrate(AuthAttemptState* attempt,
             scoped_refptr<ParallelAuthenticator> resolver,
             bool passing_old_hash,
             const std::string& hash) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  chromeos::BootTimesLoader::Get()->AddLoginTimeMarker(
      "CryptohomeMigrate-Start", false);
  cryptohome::AsyncMethodCaller* caller =
      cryptohome::AsyncMethodCaller::GetInstance();
  if (passing_old_hash) {
    caller->AsyncMigrateKey(
        attempt->username,
        hash,
        attempt->ascii_hash,
        base::Bind(&TriggerResolveWithLoginTimeMarker,
                   "CryptohomeMount-End",
                   attempt,
                   resolver));
  } else {
    caller->AsyncMigrateKey(
        attempt->username,
        attempt->ascii_hash,
        hash,
        base::Bind(&TriggerResolveWithLoginTimeMarker,
                   "CryptohomeMount-End",
                   attempt,
                   resolver));
  }
}

// Calls cryptohome's remove method.
void Remove(AuthAttemptState* attempt,
            scoped_refptr<ParallelAuthenticator> resolver) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  chromeos::BootTimesLoader::Get()->AddLoginTimeMarker(
      "CryptohomeRemove-Start", false);
  cryptohome::AsyncMethodCaller::GetInstance()->AsyncRemove(
      attempt->username,
      base::Bind(&TriggerResolveWithLoginTimeMarker,
                 "CryptohomeRemove-End",
                 attempt,
                 resolver));
}

// Calls cryptohome's key check method.
void CheckKey(AuthAttemptState* attempt,
              scoped_refptr<ParallelAuthenticator> resolver) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  cryptohome::AsyncMethodCaller::GetInstance()->AsyncCheckKey(
      attempt->username,
      attempt->ascii_hash,
      base::Bind(&TriggerResolve, attempt, resolver));
}

// Returns whether the login failure was connection issue.
bool WasConnectionIssue(const LoginFailure& online_outcome) {
  return ((online_outcome.reason() == LoginFailure::LOGIN_TIMED_OUT) ||
          (online_outcome.error().state() ==
           GoogleServiceAuthError::CONNECTION_FAILED) ||
          (online_outcome.error().state() ==
           GoogleServiceAuthError::REQUEST_CANCELED));
}

// Returns hash of |password|, salted with the system salt.
std::string HashPassword(const std::string& password) {
  // Get salt, ascii encode, update sha with that, then update with ascii
  // of password, then end.
  std::string ascii_salt =
      CrosLibrary::Get()->GetCryptohomeLibrary()->GetSystemSalt();
  char passhash_buf[kPasswordHashLength];

  // Hash salt and password
  crypto::SHA256HashString(ascii_salt + password,
                           &passhash_buf, sizeof(passhash_buf));

  // Only want the top half for 'weak' hashing so that the passphrase is not
  // immediately exposed even if the output is reversed.
  const int encoded_length = sizeof(passhash_buf) / 2;

  return StringToLowerASCII(base::HexEncode(
      reinterpret_cast<const void*>(passhash_buf), encoded_length));
}

}  // namespace

ParallelAuthenticator::ParallelAuthenticator(LoginStatusConsumer* consumer)
    : Authenticator(consumer),
      migrate_attempted_(false),
      remove_attempted_(false),
      ephemeral_mount_attempted_(false),
      check_key_attempted_(false),
      already_reported_success_(false),
      owner_is_verified_(false),
      user_can_login_(false),
      using_oauth_(true) {
}

void ParallelAuthenticator::AuthenticateToLogin(
    Profile* profile,
    const std::string& username,
    const std::string& password,
    const std::string& login_token,
    const std::string& login_captcha) {
  std::string canonicalized = gaia::CanonicalizeEmail(username);
  authentication_profile_ = profile;
  current_state_.reset(
      new AuthAttemptState(
          canonicalized,
          password,
          HashPassword(password),
          login_token,
          login_captcha,
          User::USER_TYPE_REGULAR,
          !UserManager::Get()->IsKnownUser(canonicalized)));
  // Reset the verified flag.
  owner_is_verified_ = false;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Mount,
                 current_state_.get(),
                 scoped_refptr<ParallelAuthenticator>(this),
                 cryptohome::MOUNT_FLAGS_NONE));
  // ClientLogin authentication check should happen immediately here.
  // We should not try OAuthLogin check until the profile loads.
  if (!using_oauth_) {
    // Initiate ClientLogin-based post authentication.
    current_online_.reset(new OnlineAttempt(using_oauth_,
                                            current_state_.get(),
                                            this));
    current_online_->Initiate(profile);
  }
}

void ParallelAuthenticator::CompleteLogin(Profile* profile,
                                          const std::string& username,
                                          const std::string& password) {
  std::string canonicalized = gaia::CanonicalizeEmail(username);
  authentication_profile_ = profile;
  current_state_.reset(
      new AuthAttemptState(
          canonicalized,
          password,
          HashPassword(password),
          !UserManager::Get()->IsKnownUser(canonicalized)));

  // Reset the verified flag.
  owner_is_verified_ = false;

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Mount,
                 current_state_.get(),
                 scoped_refptr<ParallelAuthenticator>(this),
                 cryptohome::MOUNT_FLAGS_NONE));

  if (!using_oauth_) {
    // Test automation needs to disable oauth, but that leads to other
    // services not being able to fetch a token, leading to browser crashes.
    // So initiate ClientLogin-based post authentication.
    // TODO(xiyuan): This should not be required.
    current_online_.reset(new OnlineAttempt(using_oauth_,
                                            current_state_.get(),
                                            this));
    current_online_->Initiate(profile);
  } else {
    // For login completion from extension, we just need to resolve the current
    // auth attempt state, the rest of OAuth related tasks will be done in
    // parallel.
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&ParallelAuthenticator::ResolveLoginCompletionStatus, this));
  }
}

void ParallelAuthenticator::AuthenticateToUnlock(const std::string& username,
                                                 const std::string& password) {
  current_state_.reset(
      new AuthAttemptState(
          gaia::CanonicalizeEmail(username),
          HashPassword(password)));
  check_key_attempted_ = true;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&CheckKey,
                 current_state_.get(),
                 scoped_refptr<ParallelAuthenticator>(this)));
}

void ParallelAuthenticator::LoginRetailMode() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Note: |kRetailModeUserEMail| is used in other places to identify a retail
  // mode session.
  current_state_.reset(new AuthAttemptState(kRetailModeUserEMail,
                                            "", "", "", "",
                                            User::USER_TYPE_RETAIL_MODE,
                                            false));
  ephemeral_mount_attempted_ = true;
  MountGuest(current_state_.get(),
             scoped_refptr<ParallelAuthenticator>(this));
}

void ParallelAuthenticator::LoginOffTheRecord() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  current_state_.reset(new AuthAttemptState("", "", "", "", "",
                                            User::USER_TYPE_GUEST,
                                            false));
  ephemeral_mount_attempted_ = true;
  MountGuest(current_state_.get(),
             scoped_refptr<ParallelAuthenticator>(this));
}

void ParallelAuthenticator::LoginAsPublicAccount(const std::string& username) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  current_state_.reset(new AuthAttemptState(username, "", "", "", "",
                                            User::USER_TYPE_PUBLIC_ACCOUNT,
                                            false));
  ephemeral_mount_attempted_ = true;
  Mount(current_state_.get(),
        scoped_refptr<ParallelAuthenticator>(this),
        cryptohome::CREATE_IF_MISSING | cryptohome::ENSURE_EPHEMERAL);
}

void ParallelAuthenticator::OnRetailModeLoginSuccess() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(1) << "Retail mode login success";
  // Send notification of success
  AuthenticationNotificationDetails details(true);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_AUTHENTICATION,
      content::NotificationService::AllSources(),
      content::Details<AuthenticationNotificationDetails>(&details));
  if (consumer_)
    consumer_->OnRetailModeLoginSuccess();
}

void ParallelAuthenticator::OnLoginSuccess(bool request_pending) {
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
  if (consumer_)
    consumer_->OnLoginSuccess(current_state_->username,
                              current_state_->password,
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
  if (consumer_)
    consumer_->OnOffTheRecordLoginSuccess();
}

void ParallelAuthenticator::OnPasswordChangeDetected() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (consumer_)
    consumer_->OnPasswordChangeDetected();
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
  if (consumer_)
    consumer_->OnLoginFailure(error);
}

void ParallelAuthenticator::RecordOAuthCheckFailure(
    const std::string& user_name) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(using_oauth_);
  // Mark this account's OAuth token state as invalid in the local state.
  UserManager::Get()->SaveUserOAuthStatus(
      user_name,
      CommandLine::ForCurrentProcess()->HasSwitch(switches::kForceOAuth1) ?
          User::OAUTH1_TOKEN_STATUS_INVALID :
          User::OAUTH2_TOKEN_STATUS_INVALID);
}

void ParallelAuthenticator::RecoverEncryptedData(
    const std::string& old_password) {
  std::string old_hash = HashPassword(old_password);
  migrate_attempted_ = true;
  current_state_->ResetCryptohomeStatus();
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Migrate,
                 current_state_.get(),
                 scoped_refptr<ParallelAuthenticator>(this),
                 true,
                 old_hash));
}

void ParallelAuthenticator::ResyncEncryptedData() {
  remove_attempted_ = true;
  current_state_->ResetCryptohomeStatus();
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Remove,
                 current_state_.get(),
                 scoped_refptr<ParallelAuthenticator>(this)));
}

bool ParallelAuthenticator::VerifyOwner() {
  if (owner_is_verified_)
    return true;
  // Check if policy data is fine and continue in safe mode if needed.
  bool is_safe_mode = false;
  CrosSettings::Get()->GetBoolean(kPolicyMissingMitigationMode, &is_safe_mode);
  if (!is_safe_mode) {
    // Now we can continue with the login and report mount success.
    user_can_login_ = true;
    owner_is_verified_ = true;
    return true;
  }
  // First we have to make sure the current user's cert store is available.
  CrosLibrary::Get()->GetCertLibrary()->LoadKeyStore();
  // Now we can continue reading the private key.
  DeviceSettingsService::Get()->SetUsername(current_state_->username);
  DeviceSettingsService::Get()->GetOwnershipStatusAsync(
      base::Bind(&ParallelAuthenticator::OnOwnershipChecked, this));
  return false;
}

void ParallelAuthenticator::OnOwnershipChecked(
    DeviceSettingsService::OwnershipStatus status,
    bool is_owner) {
  // Now we can check if this user is the owner.
  user_can_login_ = is_owner;
  owner_is_verified_ = true;
  Resolve();
}

void ParallelAuthenticator::RetryAuth(Profile* profile,
                                      const std::string& username,
                                      const std::string& password,
                                      const std::string& login_token,
                                      const std::string& login_captcha) {
  reauth_state_.reset(
      new AuthAttemptState(
          gaia::CanonicalizeEmail(username),
          password,
          HashPassword(password),
          login_token,
          login_captcha,
          User::USER_TYPE_REGULAR,
          false /* not a new user */));
  // Always use ClientLogin regardless of using_oauth flag. This is because
  // we are unable to renew oauth token on lock screen currently and will
  // stuck with lock screen if we use OAuthLogin here.
  // TODO(xiyuan): Revisit this after we support Gaia in lock screen.
  current_online_.reset(new OnlineAttempt(false /* using_oauth */,
                                          reauth_state_.get(),
                                          this));
  current_online_->Initiate(profile);
}


void ParallelAuthenticator::Resolve() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  bool request_pending = false;
  int mount_flags = cryptohome::MOUNT_FLAGS_NONE;
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
      // In this case, we tried to mount a tmpfs for guest and failed.
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&ParallelAuthenticator::OnLoginFailure, this,
                     LoginFailure(LoginFailure::COULD_NOT_MOUNT_TMPFS)));
      break;
    case FAILED_TPM:
      // In this case, we tried to create/mount cryptohome and failed
      // because of the critical TPM error.
      // Chrome will notify user and request reboot.
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&ParallelAuthenticator::OnLoginFailure, this,
                     LoginFailure(LoginFailure::TPM_ERROR)));
      break;
    case CREATE_NEW:
      mount_flags |= cryptohome::CREATE_IF_MISSING;
    case RECOVER_MOUNT:
      current_state_->ResetCryptohomeStatus();
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&Mount,
                     current_state_.get(),
                     scoped_refptr<ParallelAuthenticator>(this),
                     mount_flags));
      break;
    case NEED_OLD_PW:
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&ParallelAuthenticator::OnPasswordChangeDetected, this));
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
                base::Bind(&ParallelAuthenticator::OnLoginSuccess, this, true));
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
      migrate_attempted_ = true;
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&Migrate,
                     reauth_state_.get(),
                     scoped_refptr<ParallelAuthenticator>(this),
                     true,
                     current_state_->ascii_hash));
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
                     request_pending));
      break;
    case DEMO_LOGIN:
      VLOG(2) << "Retail mode login";
      using_oauth_ = false;
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&ParallelAuthenticator::OnRetailModeLoginSuccess, this));
      break;
    case GUEST_LOGIN:
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&ParallelAuthenticator::OnOffTheRecordLoginSuccess, this));
      break;
    case PUBLIC_ACCOUNT_LOGIN:
      using_oauth_ = false;
      BrowserThread::PostTask(
          BrowserThread::UI, FROM_HERE,
          base::Bind(&ParallelAuthenticator::OnLoginSuccess, this, false));
      break;
    case LOGIN_FAILED:
      current_state_->ResetCryptohomeStatus();
      BrowserThread::PostTask(BrowserThread::UI,
                              FROM_HERE,
                              base::Bind(
                                  &ParallelAuthenticator::OnLoginFailure,
                                  this,
                                  current_state_->online_outcome()));
      break;
    case OWNER_REQUIRED: {
      current_state_->ResetCryptohomeStatus();
      bool success = false;
      DBusThreadManager::Get()->GetCryptohomeClient()->Unmount(&success);
      if (!success) {
        // Maybe we should reboot immediately here?
        LOG(ERROR) << "Couldn't unmount users home!";
      }
      BrowserThread::PostTask(BrowserThread::UI,
                              FROM_HERE,
                              base::Bind(
                                  &ParallelAuthenticator::OnLoginFailure,
                                  this,
                                  LoginFailure(LoginFailure::OWNER_REQUIRED)));
      break;
    }
    default:
      NOTREACHED();
      break;
  }
}

ParallelAuthenticator::~ParallelAuthenticator() {}

ParallelAuthenticator::AuthState ParallelAuthenticator::ResolveState() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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
  migrate_attempted_ = false;
  remove_attempted_ = false;
  ephemeral_mount_attempted_ = false;
  check_key_attempted_ = false;

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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (remove_attempted_)
    return FAILED_REMOVE;
  if (ephemeral_mount_attempted_)
    return FAILED_TMPFS;
  if (migrate_attempted_)
    return NEED_OLD_PW;
  if (check_key_attempted_)
    return LOGIN_FAILED;

  if (current_state_->cryptohome_code() ==
      cryptohome::MOUNT_ERROR_TPM_NEEDS_REBOOT) {
    // Critical TPM error detected, reboot needed.
    return FAILED_TPM;
  }

  // Return intermediate states in the following cases:
  // 1. When there is a parallel online attempt to resolve them later;
  //    This is the case with legacy ClientLogin flow;
  // 2. When there is an online result to use;
  //    This is the case after user finishes Gaia login;
  if (current_online_.get() || current_state_->online_complete()) {
    if (current_state_->cryptohome_code() ==
        cryptohome::MOUNT_ERROR_KEY_FAILURE) {
      // If we tried a mount but they used the wrong key, we may need to
      // ask the user for her old password.  We'll only know once we've
      // done the online check.
      return POSSIBLE_PW_CHANGE;
    }
    if (current_state_->cryptohome_code() ==
        cryptohome::MOUNT_ERROR_USER_DOES_NOT_EXIST) {
      // If we tried a mount but the user did not exist, then we should wait
      // for online login to succeed and try again with the "create" flag set.
      return NO_MOUNT;
    }
  }

  return FAILED_MOUNT;
}

ParallelAuthenticator::AuthState
ParallelAuthenticator::ResolveCryptohomeSuccessState() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (remove_attempted_)
    return CREATE_NEW;
  if (migrate_attempted_)
    return RECOVER_MOUNT;
  if (check_key_attempted_)
    return UNLOCK;

  if (current_state_->user_type == User::USER_TYPE_GUEST)
    return GUEST_LOGIN;
  if (current_state_->user_type == User::USER_TYPE_RETAIL_MODE)
    return DEMO_LOGIN;
  if (current_state_->user_type == User::USER_TYPE_PUBLIC_ACCOUNT)
    return PUBLIC_ACCOUNT_LOGIN;

  if (!VerifyOwner())
    return CONTINUE;
  return user_can_login_ ? OFFLINE_LOGIN : OWNER_REQUIRED;
}

ParallelAuthenticator::AuthState
ParallelAuthenticator::ResolveOnlineFailureState(
    ParallelAuthenticator::AuthState offline_state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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

void ParallelAuthenticator::ResolveLoginCompletionStatus() {
  // Shortcut online state resolution process.
  current_state_->RecordOnlineLoginStatus(LoginFailure::None());
  Resolve();
}

void ParallelAuthenticator::SetOwnerState(bool owner_check_finished,
                                          bool check_result) {
  owner_is_verified_ = owner_check_finished;
  user_can_login_ = check_result;
}

}  // namespace chromeos
