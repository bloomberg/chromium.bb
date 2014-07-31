// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/auth/parallel_authenticator.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/login/auth/authentication_notification_details.h"
#include "chrome/browser/chromeos/login/users/user_manager.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_factory.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/common/chrome_switches.h"
#include "chromeos/cryptohome/async_method_caller.h"
#include "chromeos/cryptohome/system_salt_getter.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/login/auth/auth_status_consumer.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/user_context.h"
#include "chromeos/login/login_state.h"
#include "chromeos/login/user_names.h"
#include "components/user_manager/user_type.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using content::BrowserThread;

namespace chromeos {

namespace {

// Hashes |key| with |system_salt| if it its type is KEY_TYPE_PASSWORD_PLAIN.
// Returns the keys unmodified otherwise.
scoped_ptr<Key> TransformKeyIfNeeded(const Key& key,
                                     const std::string& system_salt) {
  scoped_ptr<Key> result(new Key(key));
  if (result->GetKeyType() == Key::KEY_TYPE_PASSWORD_PLAIN)
    result->Transform(Key::KEY_TYPE_SALTED_SHA256_TOP_HALF, system_salt);

  return result.Pass();
}

// Records status and calls resolver->Resolve().
void TriggerResolve(AuthAttemptState* attempt,
                    scoped_refptr<ParallelAuthenticator> resolver,
                    bool success,
                    cryptohome::MountError return_code) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  attempt->RecordCryptohomeStatus(success, return_code);
  resolver->Resolve();
}

// Records get hash status and calls resolver->Resolve().
void TriggerResolveHash(AuthAttemptState* attempt,
                        scoped_refptr<ParallelAuthenticator> resolver,
                        bool success,
                        const std::string& username_hash) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (success)
    attempt->RecordUsernameHash(username_hash);
  else
    attempt->RecordUsernameHashFailed();
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
           int flags,
           const std::string& system_salt) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  chromeos::BootTimesLoader::Get()->AddLoginTimeMarker(
      "CryptohomeMount-Start", false);
  // Set state that username_hash is requested here so that test implementation
  // that returns directly would not generate 2 OnLoginSucces() calls.
  attempt->UsernameHashRequested();

  scoped_ptr<Key> key =
      TransformKeyIfNeeded(*attempt->user_context.GetKey(), system_salt);
  cryptohome::AsyncMethodCaller::GetInstance()->AsyncMount(
      attempt->user_context.GetUserID(),
      key->GetSecret(),
      flags,
      base::Bind(&TriggerResolveWithLoginTimeMarker,
                 "CryptohomeMount-End",
                 attempt,
                 resolver));
  cryptohome::AsyncMethodCaller::GetInstance()->AsyncGetSanitizedUsername(
      attempt->user_context.GetUserID(),
      base::Bind(&TriggerResolveHash,
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

// Calls cryptohome's mount method for guest and also get the user hash from
// cryptohome.
void MountGuestAndGetHash(AuthAttemptState* attempt,
                          scoped_refptr<ParallelAuthenticator> resolver) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  attempt->UsernameHashRequested();
  cryptohome::AsyncMethodCaller::GetInstance()->AsyncMountGuest(
      base::Bind(&TriggerResolveWithLoginTimeMarker,
                 "CryptohomeMount-End",
                 attempt,
                 resolver));
  cryptohome::AsyncMethodCaller::GetInstance()->AsyncGetSanitizedUsername(
      attempt->user_context.GetUserID(),
      base::Bind(&TriggerResolveHash,
                 attempt,
                 resolver));
}

// Calls cryptohome's MountPublic method
void MountPublic(AuthAttemptState* attempt,
                 scoped_refptr<ParallelAuthenticator> resolver,
                 int flags) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  cryptohome::AsyncMethodCaller::GetInstance()->AsyncMountPublic(
      attempt->user_context.GetUserID(),
      flags,
      base::Bind(&TriggerResolveWithLoginTimeMarker,
                 "CryptohomeMountPublic-End",
                 attempt,
                 resolver));
  cryptohome::AsyncMethodCaller::GetInstance()->AsyncGetSanitizedUsername(
      attempt->user_context.GetUserID(),
      base::Bind(&TriggerResolveHash,
                 attempt,
                 resolver));
}

// Calls cryptohome's key migration method.
void Migrate(AuthAttemptState* attempt,
             scoped_refptr<ParallelAuthenticator> resolver,
             bool passing_old_hash,
             const std::string& old_password,
             const std::string& system_salt) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  chromeos::BootTimesLoader::Get()->AddLoginTimeMarker(
      "CryptohomeMigrate-Start", false);
  cryptohome::AsyncMethodCaller* caller =
      cryptohome::AsyncMethodCaller::GetInstance();

  // TODO(bartfab): Retrieve the hashing algorithm and salt to use for |old_key|
  // from cryptohomed.
  scoped_ptr<Key> old_key =
      TransformKeyIfNeeded(Key(old_password), system_salt);
  scoped_ptr<Key> new_key =
      TransformKeyIfNeeded(*attempt->user_context.GetKey(), system_salt);
  if (passing_old_hash) {
    caller->AsyncMigrateKey(attempt->user_context.GetUserID(),
                            old_key->GetSecret(),
                            new_key->GetSecret(),
                            base::Bind(&TriggerResolveWithLoginTimeMarker,
                                       "CryptohomeMount-End",
                                       attempt,
                                       resolver));
  } else {
    caller->AsyncMigrateKey(attempt->user_context.GetUserID(),
                            new_key->GetSecret(),
                            old_key->GetSecret(),
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
      attempt->user_context.GetUserID(),
      base::Bind(&TriggerResolveWithLoginTimeMarker,
                 "CryptohomeRemove-End",
                 attempt,
                 resolver));
}

// Calls cryptohome's key check method.
void CheckKey(AuthAttemptState* attempt,
              scoped_refptr<ParallelAuthenticator> resolver,
              const std::string& system_salt) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  scoped_ptr<Key> key =
      TransformKeyIfNeeded(*attempt->user_context.GetKey(), system_salt);
  cryptohome::AsyncMethodCaller::GetInstance()->AsyncCheckKey(
      attempt->user_context.GetUserID(),
      key->GetSecret(),
      base::Bind(&TriggerResolve, attempt, resolver));
}

}  // namespace

ParallelAuthenticator::ParallelAuthenticator(AuthStatusConsumer* consumer)
    : Authenticator(consumer),
      migrate_attempted_(false),
      remove_attempted_(false),
      resync_attempted_(false),
      ephemeral_mount_attempted_(false),
      check_key_attempted_(false),
      already_reported_success_(false),
      owner_is_verified_(false),
      user_can_login_(false),
      remove_user_data_on_failure_(false),
      delayed_login_failure_(NULL) {
}

void ParallelAuthenticator::AuthenticateToLogin(
    Profile* profile,
    const UserContext& user_context) {
  authentication_profile_ = profile;
  current_state_.reset(new AuthAttemptState(
      user_context,
      user_manager::USER_TYPE_REGULAR,
      false,  // unlock
      false,  // online_complete
      !UserManager::Get()->IsKnownUser(user_context.GetUserID())));
  // Reset the verified flag.
  owner_is_verified_ = false;

  SystemSaltGetter::Get()->GetSystemSalt(
      base::Bind(&Mount,
                 current_state_.get(),
                 scoped_refptr<ParallelAuthenticator>(this),
                 cryptohome::MOUNT_FLAGS_NONE));
}

void ParallelAuthenticator::CompleteLogin(Profile* profile,
                                          const UserContext& user_context) {
  authentication_profile_ = profile;
  current_state_.reset(new AuthAttemptState(
      user_context,
      user_manager::USER_TYPE_REGULAR,
      true,   // unlock
      false,  // online_complete
      !UserManager::Get()->IsKnownUser(user_context.GetUserID())));

  // Reset the verified flag.
  owner_is_verified_ = false;

  SystemSaltGetter::Get()->GetSystemSalt(
      base::Bind(&Mount,
                 current_state_.get(),
                 scoped_refptr<ParallelAuthenticator>(this),
                 cryptohome::MOUNT_FLAGS_NONE));

  // For login completion from extension, we just need to resolve the current
  // auth attempt state, the rest of OAuth related tasks will be done in
  // parallel.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&ParallelAuthenticator::ResolveLoginCompletionStatus, this));
}

void ParallelAuthenticator::AuthenticateToUnlock(
    const UserContext& user_context) {
  current_state_.reset(new AuthAttemptState(user_context,
                                            user_manager::USER_TYPE_REGULAR,
                                            true,     // unlock
                                            true,     // online_complete
                                            false));  // user_is_new
  remove_user_data_on_failure_ = false;
  check_key_attempted_ = true;
  SystemSaltGetter::Get()->GetSystemSalt(
      base::Bind(&CheckKey,
                 current_state_.get(),
                 scoped_refptr<ParallelAuthenticator>(this)));
}

void ParallelAuthenticator::LoginAsSupervisedUser(
    const UserContext& user_context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // TODO(nkostylev): Pass proper value for |user_is_new| or remove (not used).
  current_state_.reset(
      new AuthAttemptState(user_context,
                           user_manager::USER_TYPE_SUPERVISED,
                           false,    // unlock
                           false,    // online_complete
                           false));  // user_is_new
  remove_user_data_on_failure_ = false;
  SystemSaltGetter::Get()->GetSystemSalt(
      base::Bind(&Mount,
                 current_state_.get(),
                 scoped_refptr<ParallelAuthenticator>(this),
                 cryptohome::MOUNT_FLAGS_NONE));
}

void ParallelAuthenticator::LoginRetailMode() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Note: |kRetailModeUserEMail| is used in other places to identify a retail
  // mode session.
  current_state_.reset(
      new AuthAttemptState(UserContext(chromeos::login::kRetailModeUserName),
                           user_manager::USER_TYPE_RETAIL_MODE,
                           false,    // unlock
                           false,    // online_complete
                           false));  // user_is_new
  remove_user_data_on_failure_ = false;
  ephemeral_mount_attempted_ = true;
  MountGuestAndGetHash(current_state_.get(),
                       scoped_refptr<ParallelAuthenticator>(this));
}

void ParallelAuthenticator::LoginOffTheRecord() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  current_state_.reset(
      new AuthAttemptState(UserContext(chromeos::login::kGuestUserName),
                           user_manager::USER_TYPE_GUEST,
                           false,    // unlock
                           false,    // online_complete
                           false));  // user_is_new
  remove_user_data_on_failure_ = false;
  ephemeral_mount_attempted_ = true;
  MountGuest(current_state_.get(),
             scoped_refptr<ParallelAuthenticator>(this));
}

void ParallelAuthenticator::LoginAsPublicSession(
    const UserContext& user_context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  current_state_.reset(
      new AuthAttemptState(user_context,
                           user_manager::USER_TYPE_PUBLIC_ACCOUNT,
                           false,    // unlock
                           false,    // online_complete
                           false));  // user_is_new
  remove_user_data_on_failure_ = false;
  ephemeral_mount_attempted_ = true;
  SystemSaltGetter::Get()->GetSystemSalt(
      base::Bind(&Mount,
                 current_state_.get(),
                 scoped_refptr<ParallelAuthenticator>(this),
                 cryptohome::CREATE_IF_MISSING | cryptohome::ENSURE_EPHEMERAL));
}

void ParallelAuthenticator::LoginAsKioskAccount(
    const std::string& app_user_id,
    bool use_guest_mount) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const std::string user_id =
      use_guest_mount ? chromeos::login::kGuestUserName : app_user_id;
  current_state_.reset(new AuthAttemptState(UserContext(user_id),
                                            user_manager::USER_TYPE_KIOSK_APP,
                                            false,    // unlock
                                            false,    // online_complete
                                            false));  // user_is_new

  remove_user_data_on_failure_ = true;
  if (!use_guest_mount) {
    MountPublic(current_state_.get(),
          scoped_refptr<ParallelAuthenticator>(this),
          cryptohome::CREATE_IF_MISSING);
  } else {
    ephemeral_mount_attempted_ = true;
    MountGuestAndGetHash(current_state_.get(),
                         scoped_refptr<ParallelAuthenticator>(this));
  }
}

void ParallelAuthenticator::OnRetailModeAuthSuccess() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  VLOG(1) << "Retail mode login success";
  // Send notification of success
  AuthenticationNotificationDetails details(true);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_AUTHENTICATION,
      content::NotificationService::AllSources(),
      content::Details<AuthenticationNotificationDetails>(&details));
  if (consumer_)
    consumer_->OnRetailModeAuthSuccess(current_state_->user_context);
}

void ParallelAuthenticator::OnAuthSuccess() {
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
    consumer_->OnAuthSuccess(current_state_->user_context);
}

void ParallelAuthenticator::OnOffTheRecordAuthSuccess() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Send notification of success
  AuthenticationNotificationDetails details(true);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_AUTHENTICATION,
      content::NotificationService::AllSources(),
      content::Details<AuthenticationNotificationDetails>(&details));
  if (consumer_)
    consumer_->OnOffTheRecordAuthSuccess();
}

void ParallelAuthenticator::OnPasswordChangeDetected() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (consumer_)
    consumer_->OnPasswordChangeDetected();
}

void ParallelAuthenticator::OnAuthFailure(const AuthFailure& error) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // OnAuthFailure will be called again with the same |error|
  // after the cryptohome has been removed.
  if (remove_user_data_on_failure_) {
    delayed_login_failure_ = &error;
    RemoveEncryptedData();
    return;
  }

  // Send notification of failure
  AuthenticationNotificationDetails details(false);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_LOGIN_AUTHENTICATION,
      content::NotificationService::AllSources(),
      content::Details<AuthenticationNotificationDetails>(&details));
  LOG(WARNING) << "Login failed: " << error.GetErrorString();
  if (consumer_)
    consumer_->OnAuthFailure(error);
}

void ParallelAuthenticator::RecoverEncryptedData(
    const std::string& old_password) {
  migrate_attempted_ = true;
  current_state_->ResetCryptohomeStatus();
  SystemSaltGetter::Get()->GetSystemSalt(
      base::Bind(&Migrate,
                 current_state_.get(),
                 scoped_refptr<ParallelAuthenticator>(this),
                 true,
                 old_password));
}

void ParallelAuthenticator::RemoveEncryptedData() {
  remove_attempted_ = true;
  current_state_->ResetCryptohomeStatus();
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&Remove,
                 current_state_.get(),
                 scoped_refptr<ParallelAuthenticator>(this)));
}

void ParallelAuthenticator::ResyncEncryptedData() {
  resync_attempted_ = true;
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

  const std::string& user_id = current_state_->user_context.GetUserID();
  OwnerSettingsServiceFactory::GetInstance()->SetUsername(user_id);

  // |IsOwnerForSafeModeAsync| expects logged in state to be
  // LOGGED_IN_SAFE_MODE.
  if (LoginState::IsInitialized()) {
    LoginState::Get()->SetLoggedInState(LoginState::LOGGED_IN_SAFE_MODE,
                                        LoginState::LOGGED_IN_USER_NONE);
  }

  OwnerSettingsService::IsOwnerForSafeModeAsync(
      user_id,
      current_state_->user_context.GetUserIDHash(),
      base::Bind(&ParallelAuthenticator::OnOwnershipChecked, this));
  return false;
}

void ParallelAuthenticator::OnOwnershipChecked(bool is_owner) {
  // Now we can check if this user is the owner.
  user_can_login_ = is_owner;
  owner_is_verified_ = true;
  Resolve();
}

void ParallelAuthenticator::Resolve() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
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
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&ParallelAuthenticator::OnAuthFailure,
                     this,
                     AuthFailure(AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME)));
      break;
    case FAILED_REMOVE:
      // In this case, we tried to remove the user's old cryptohome at her
      // request, and the remove failed.
      remove_user_data_on_failure_ = false;
      BrowserThread::PostTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&ParallelAuthenticator::OnAuthFailure,
                     this,
                     AuthFailure(AuthFailure::DATA_REMOVAL_FAILED)));
      break;
    case FAILED_TMPFS:
      // In this case, we tried to mount a tmpfs for guest and failed.
      BrowserThread::PostTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&ParallelAuthenticator::OnAuthFailure,
                     this,
                     AuthFailure(AuthFailure::COULD_NOT_MOUNT_TMPFS)));
      break;
    case FAILED_TPM:
      // In this case, we tried to create/mount cryptohome and failed
      // because of the critical TPM error.
      // Chrome will notify user and request reboot.
      BrowserThread::PostTask(BrowserThread::UI,
                              FROM_HERE,
                              base::Bind(&ParallelAuthenticator::OnAuthFailure,
                                         this,
                                         AuthFailure(AuthFailure::TPM_ERROR)));
      break;
    case FAILED_USERNAME_HASH:
      // In this case, we failed the GetSanitizedUsername request to
      // cryptohomed. This can happen for any login attempt.
      BrowserThread::PostTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&ParallelAuthenticator::OnAuthFailure,
                     this,
                     AuthFailure(AuthFailure::USERNAME_HASH_FAILED)));
      break;
    case REMOVED_DATA_AFTER_FAILURE:
      remove_user_data_on_failure_ = false;
      BrowserThread::PostTask(BrowserThread::UI,
                              FROM_HERE,
                              base::Bind(&ParallelAuthenticator::OnAuthFailure,
                                         this,
                                         *delayed_login_failure_));
      break;
    case CREATE_NEW:
      mount_flags |= cryptohome::CREATE_IF_MISSING;
    case RECOVER_MOUNT:
      current_state_->ResetCryptohomeStatus();
      SystemSaltGetter::Get()->GetSystemSalt(
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
    case NEED_NEW_PW:
    case HAVE_NEW_PW:
      NOTREACHED() << "Using obsolete ClientLogin code path.";
      break;
    case OFFLINE_LOGIN:
      VLOG(2) << "Offline login";
      // Fall through.
    case UNLOCK:
      VLOG(2) << "Unlock";
      // Fall through.
    case ONLINE_LOGIN:
      VLOG(2) << "Online login";
      BrowserThread::PostTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&ParallelAuthenticator::OnAuthSuccess, this));
      break;
    case DEMO_LOGIN:
      VLOG(2) << "Retail mode login";
      current_state_->user_context.SetIsUsingOAuth(false);
      BrowserThread::PostTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&ParallelAuthenticator::OnRetailModeAuthSuccess, this));
      break;
    case GUEST_LOGIN:
      BrowserThread::PostTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&ParallelAuthenticator::OnOffTheRecordAuthSuccess, this));
      break;
    case KIOSK_ACCOUNT_LOGIN:
    case PUBLIC_ACCOUNT_LOGIN:
      current_state_->user_context.SetIsUsingOAuth(false);
      BrowserThread::PostTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&ParallelAuthenticator::OnAuthSuccess, this));
      break;
    case SUPERVISED_USER_LOGIN:
      current_state_->user_context.SetIsUsingOAuth(false);
      BrowserThread::PostTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&ParallelAuthenticator::OnAuthSuccess, this));
      break;
    case LOGIN_FAILED:
      current_state_->ResetCryptohomeStatus();
      BrowserThread::PostTask(BrowserThread::UI,
                              FROM_HERE,
                              base::Bind(&ParallelAuthenticator::OnAuthFailure,
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
      BrowserThread::PostTask(
          BrowserThread::UI,
          FROM_HERE,
          base::Bind(&ParallelAuthenticator::OnAuthFailure,
                     this,
                     AuthFailure(AuthFailure::OWNER_REQUIRED)));
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
  // If we haven't mounted the user's home dir yet or
  // haven't got sanitized username value, we can't be done.
  // We never get past here if any of these two cryptohome ops is still pending.
  // This is an important invariant.
  if (!current_state_->cryptohome_complete() ||
      !current_state_->username_hash_obtained()) {
    return CONTINUE;
  }

  AuthState state = CONTINUE;

  if (current_state_->cryptohome_outcome() &&
      current_state_->username_hash_valid()) {
    state = ResolveCryptohomeSuccessState();
  } else {
    state = ResolveCryptohomeFailureState();
  }

  DCHECK(current_state_->cryptohome_complete());  // Ensure invariant holds.
  migrate_attempted_ = false;
  remove_attempted_ = false;
  resync_attempted_ = false;
  ephemeral_mount_attempted_ = false;
  check_key_attempted_ = false;

  if (state != POSSIBLE_PW_CHANGE &&
      state != NO_MOUNT &&
      state != OFFLINE_LOGIN)
    return state;

  if (current_state_->online_complete()) {
    if (current_state_->online_outcome().reason() == AuthFailure::NONE) {
      // Online attempt succeeded as well, so combine the results.
      return ResolveOnlineSuccessState(state);
    }
    NOTREACHED() << "Using obsolete ClientLogin code path.";
  }
  // if online isn't complete yet, just return the offline result.
  return state;
}

ParallelAuthenticator::AuthState
ParallelAuthenticator::ResolveCryptohomeFailureState() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (remove_attempted_ || resync_attempted_)
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

  // Return intermediate states in the following case:
  // when there is an online result to use;
  // This is the case after user finishes Gaia login;
  if (current_state_->online_complete()) {
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

  if (!current_state_->username_hash_valid())
    return FAILED_USERNAME_HASH;

  return FAILED_MOUNT;
}

ParallelAuthenticator::AuthState
ParallelAuthenticator::ResolveCryptohomeSuccessState() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (resync_attempted_)
    return CREATE_NEW;
  if (remove_attempted_)
    return REMOVED_DATA_AFTER_FAILURE;
  if (migrate_attempted_)
    return RECOVER_MOUNT;
  if (check_key_attempted_)
    return UNLOCK;

  if (current_state_->user_type == user_manager::USER_TYPE_GUEST)
    return GUEST_LOGIN;
  if (current_state_->user_type == user_manager::USER_TYPE_RETAIL_MODE)
    return DEMO_LOGIN;
  if (current_state_->user_type == user_manager::USER_TYPE_PUBLIC_ACCOUNT)
    return PUBLIC_ACCOUNT_LOGIN;
  if (current_state_->user_type == user_manager::USER_TYPE_KIOSK_APP)
    return KIOSK_ACCOUNT_LOGIN;
  if (current_state_->user_type == user_manager::USER_TYPE_SUPERVISED)
    return SUPERVISED_USER_LOGIN;

  if (!VerifyOwner())
    return CONTINUE;
  return user_can_login_ ? OFFLINE_LOGIN : OWNER_REQUIRED;
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
  current_state_->RecordOnlineLoginStatus(AuthFailure::AuthFailureNone());
  Resolve();
}

void ParallelAuthenticator::SetOwnerState(bool owner_check_finished,
                                          bool check_result) {
  owner_is_verified_ = owner_check_finished;
  user_can_login_ = check_result;
}

}  // namespace chromeos
