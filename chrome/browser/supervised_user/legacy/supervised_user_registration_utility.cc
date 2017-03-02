// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/legacy/supervised_user_registration_utility.h"

#include <memory>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_worker_pool.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_refresh_token_fetcher.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_shared_settings_service.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_shared_settings_service_factory.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_shared_settings_update.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_sync_service.h"
#include "chrome/browser/supervised_user/legacy/supervised_user_sync_service_factory.h"
#include "chrome/browser/supervised_user/supervised_user_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_client.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/sync/base/get_session_name.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"

using base::DictionaryValue;

namespace {

SupervisedUserRegistrationUtility* g_instance_for_tests = NULL;

// Actual implementation of SupervisedUserRegistrationUtility.
class SupervisedUserRegistrationUtilityImpl
    : public SupervisedUserRegistrationUtility,
      public SupervisedUserSyncServiceObserver {
 public:
  SupervisedUserRegistrationUtilityImpl(
      PrefService* prefs,
      std::unique_ptr<SupervisedUserRefreshTokenFetcher> token_fetcher,
      SupervisedUserSyncService* service,
      SupervisedUserSharedSettingsService* shared_settings_service);

  ~SupervisedUserRegistrationUtilityImpl() override;

  // Registers a new supervised user with the server. |supervised_user_id| is a
  // new unique ID for the new supervised user. If its value is the same as that
  // of one of the existing supervised users, then the same user will be created
  // on this machine (and if they have no avatar in sync, their avatar will be
  // updated). |info| contains necessary information like the display name of
  // the user and their avatar. |callback| is called with the result of the
  // registration. We use the info here and not the profile, because on Chrome
  // OS the profile of the supervised user does not yet exist.
  void Register(const std::string& supervised_user_id,
                const SupervisedUserRegistrationInfo& info,
                const RegistrationCallback& callback) override;

  // SupervisedUserSyncServiceObserver:
  void OnSupervisedUserAcknowledged(
      const std::string& supervised_user_id) override;
  void OnSupervisedUsersSyncingStopped() override;
  void OnSupervisedUsersChanged() override;

 private:
  // Fetches the supervised user token when we have the device name.
  void FetchToken(const std::string& client_name);

  // Called when we have received a token for the supervised user.
  void OnReceivedToken(const GoogleServiceAuthError& error,
                       const std::string& token);

  // Dispatches the callback and cleans up if all the conditions have been met.
  void CompleteRegistrationIfReady();

  // Aborts any registration currently in progress. If |run_callback| is true,
  // calls the callback specified in Register() with the given |error|.
  void AbortPendingRegistration(bool run_callback,
                                const GoogleServiceAuthError& error);

  // If |run_callback| is true, dispatches the callback with the saved token
  // (which may be empty) and the given |error|. In any case, resets internal
  // variables to be ready for the next registration.
  void CompleteRegistration(bool run_callback,
                            const GoogleServiceAuthError& error);

  // Cancels any registration currently in progress, without calling the
  // callback or reporting an error.
  void CancelPendingRegistration();

  // SupervisedUserSharedSettingsUpdate acknowledgment callback for password
  // data in shared settings.
  void OnPasswordChangeAcknowledged(bool success);

  PrefService* prefs_;
  std::unique_ptr<SupervisedUserRefreshTokenFetcher> token_fetcher_;

  // A |KeyedService| owned by the custodian profile.
  SupervisedUserSyncService* supervised_user_sync_service_;

  // A |KeyedService| owned by the custodian profile.
  SupervisedUserSharedSettingsService* supervised_user_shared_settings_service_;

  std::string pending_supervised_user_id_;
  std::string pending_supervised_user_token_;
  bool pending_supervised_user_acknowledged_;
  bool is_existing_supervised_user_;
  bool avatar_updated_;
  RegistrationCallback callback_;
  std::unique_ptr<SupervisedUserSharedSettingsUpdate> password_update_;

  base::WeakPtrFactory<SupervisedUserRegistrationUtilityImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SupervisedUserRegistrationUtilityImpl);
};

} // namespace

SupervisedUserRegistrationInfo::SupervisedUserRegistrationInfo(
    const base::string16& name,
    int avatar_index)
    : avatar_index(avatar_index),
      name(name) {
}

SupervisedUserRegistrationInfo::~SupervisedUserRegistrationInfo() {}

ScopedTestingSupervisedUserRegistrationUtility::
    ScopedTestingSupervisedUserRegistrationUtility(
        SupervisedUserRegistrationUtility* instance) {
  SupervisedUserRegistrationUtility::SetUtilityForTests(instance);
}

ScopedTestingSupervisedUserRegistrationUtility::
    ~ScopedTestingSupervisedUserRegistrationUtility() {
  SupervisedUserRegistrationUtility::SetUtilityForTests(NULL);
}

// static
std::unique_ptr<SupervisedUserRegistrationUtility>
SupervisedUserRegistrationUtility::Create(Profile* profile) {
  if (g_instance_for_tests) {
    SupervisedUserRegistrationUtility* result = g_instance_for_tests;
    g_instance_for_tests = NULL;
    return base::WrapUnique(result);
  }

  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile);
  SigninClient* signin_client =
      ChromeSigninClientFactory::GetForProfile(profile);
  std::string signin_scoped_device_id =
      signin_client->GetSigninScopedDeviceId();
  std::unique_ptr<SupervisedUserRefreshTokenFetcher> token_fetcher =
      SupervisedUserRefreshTokenFetcher::Create(
          token_service, signin_manager->GetAuthenticatedAccountId(),
          signin_scoped_device_id, profile->GetRequestContext());
  SupervisedUserSyncService* supervised_user_sync_service =
      SupervisedUserSyncServiceFactory::GetForProfile(profile);
  SupervisedUserSharedSettingsService* supervised_user_shared_settings_service =
      SupervisedUserSharedSettingsServiceFactory::GetForBrowserContext(profile);
  return base::WrapUnique(SupervisedUserRegistrationUtility::CreateImpl(
      profile->GetPrefs(), std::move(token_fetcher),
      supervised_user_sync_service, supervised_user_shared_settings_service));
}

// static
std::string SupervisedUserRegistrationUtility::GenerateNewSupervisedUserId() {
  std::string new_supervised_user_id;
  base::Base64Encode(base::RandBytesAsString(8), &new_supervised_user_id);
  return new_supervised_user_id;
}

// static
void SupervisedUserRegistrationUtility::SetUtilityForTests(
    SupervisedUserRegistrationUtility* utility) {
  if (g_instance_for_tests)
    delete g_instance_for_tests;
  g_instance_for_tests = utility;
}

// static
SupervisedUserRegistrationUtility*
SupervisedUserRegistrationUtility::CreateImpl(
    PrefService* prefs,
    std::unique_ptr<SupervisedUserRefreshTokenFetcher> token_fetcher,
    SupervisedUserSyncService* service,
    SupervisedUserSharedSettingsService* shared_settings_service) {
  return new SupervisedUserRegistrationUtilityImpl(
      prefs, std::move(token_fetcher), service, shared_settings_service);
}

namespace {

SupervisedUserRegistrationUtilityImpl::SupervisedUserRegistrationUtilityImpl(
    PrefService* prefs,
    std::unique_ptr<SupervisedUserRefreshTokenFetcher> token_fetcher,
    SupervisedUserSyncService* service,
    SupervisedUserSharedSettingsService* shared_settings_service)
    : prefs_(prefs),
      token_fetcher_(std::move(token_fetcher)),
      supervised_user_sync_service_(service),
      supervised_user_shared_settings_service_(shared_settings_service),
      pending_supervised_user_acknowledged_(false),
      is_existing_supervised_user_(false),
      avatar_updated_(false),
      weak_ptr_factory_(this) {
  supervised_user_sync_service_->AddObserver(this);
}

SupervisedUserRegistrationUtilityImpl::
~SupervisedUserRegistrationUtilityImpl() {
  supervised_user_sync_service_->RemoveObserver(this);
  CancelPendingRegistration();
}

void SupervisedUserRegistrationUtilityImpl::Register(
    const std::string& supervised_user_id,
    const SupervisedUserRegistrationInfo& info,
    const RegistrationCallback& callback) {
  DCHECK(pending_supervised_user_id_.empty());
  callback_ = callback;
  pending_supervised_user_id_ = supervised_user_id;

  bool need_password_update = !info.password_data.empty();
  const base::DictionaryValue* dict =
      prefs_->GetDictionary(prefs::kSupervisedUsers);
  is_existing_supervised_user_ = dict->HasKey(supervised_user_id);
  if (!is_existing_supervised_user_) {
    supervised_user_sync_service_->AddSupervisedUser(
        pending_supervised_user_id_,
        base::UTF16ToUTF8(info.name),
        info.master_key,
        info.password_signature_key,
        info.password_encryption_key,
        info.avatar_index);
  } else {
    const base::DictionaryValue* value = NULL;
    bool success =
        dict->GetDictionaryWithoutPathExpansion(supervised_user_id, &value);
    DCHECK(success);
    std::string key;
    bool need_keys = !info.password_signature_key.empty() ||
                     !info.password_encryption_key.empty();
    bool have_keys =
        value->GetString(SupervisedUserSyncService::kPasswordSignatureKey,
                         &key) &&
        !key.empty() &&
        value->GetString(SupervisedUserSyncService::kPasswordEncryptionKey,
                         &key) &&
        !key.empty();

    bool keys_need_update = need_keys && !have_keys;

    if (keys_need_update) {
      supervised_user_sync_service_->UpdateSupervisedUser(
          pending_supervised_user_id_,
          base::UTF16ToUTF8(info.name),
          info.master_key,
          info.password_signature_key,
          info.password_encryption_key,
          info.avatar_index);
    } else {
      // The user already exists and does not need to be updated.
      need_password_update = false;
      OnSupervisedUserAcknowledged(supervised_user_id);
    }
    avatar_updated_ =
        supervised_user_sync_service_->UpdateSupervisedUserAvatarIfNeeded(
            supervised_user_id,
            info.avatar_index);
  }
#if defined(OS_CHROMEOS)
  const char* kAvatarKey = supervised_users::kChromeOSAvatarIndex;
#else
  const char* kAvatarKey = supervised_users::kChromeAvatarIndex;
#endif
  supervised_user_shared_settings_service_->SetValue(
      pending_supervised_user_id_, kAvatarKey, base::Value(info.avatar_index));
  if (need_password_update) {
    password_update_.reset(new SupervisedUserSharedSettingsUpdate(
        supervised_user_shared_settings_service_, pending_supervised_user_id_,
        supervised_users::kChromeOSPasswordData,
        std::unique_ptr<base::Value>(info.password_data.DeepCopy()),
        base::Bind(&SupervisedUserRegistrationUtilityImpl::
                       OnPasswordChangeAcknowledged,
                   weak_ptr_factory_.GetWeakPtr())));
  }

  syncer::GetSessionName(
      content::BrowserThread::GetBlockingPool()
          ->GetTaskRunnerWithShutdownBehavior(
              base::SequencedWorkerPool::CONTINUE_ON_SHUTDOWN).get(),
      base::Bind(&SupervisedUserRegistrationUtilityImpl::FetchToken,
                 weak_ptr_factory_.GetWeakPtr()));
}

void SupervisedUserRegistrationUtilityImpl::CancelPendingRegistration() {
  AbortPendingRegistration(
      false,  // Don't run the callback. The error will be ignored.
      GoogleServiceAuthError(GoogleServiceAuthError::NONE));
}

void SupervisedUserRegistrationUtilityImpl::OnSupervisedUserAcknowledged(
    const std::string& supervised_user_id) {
  DCHECK_EQ(pending_supervised_user_id_, supervised_user_id);
  DCHECK(!pending_supervised_user_acknowledged_);
  pending_supervised_user_acknowledged_ = true;
  CompleteRegistrationIfReady();
}

void SupervisedUserRegistrationUtilityImpl::OnPasswordChangeAcknowledged(
    bool success) {
  DCHECK(password_update_);
  DCHECK(success);
  password_update_.reset();
  CompleteRegistrationIfReady();
}

void SupervisedUserRegistrationUtilityImpl::OnSupervisedUsersSyncingStopped() {
  AbortPendingRegistration(
      true,  // Run the callback.
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));
}

void SupervisedUserRegistrationUtilityImpl::OnSupervisedUsersChanged() {}

void SupervisedUserRegistrationUtilityImpl::FetchToken(
    const std::string& client_name) {
  token_fetcher_->Start(
      pending_supervised_user_id_, client_name,
      base::Bind(&SupervisedUserRegistrationUtilityImpl::OnReceivedToken,
                 weak_ptr_factory_.GetWeakPtr()));
}

void SupervisedUserRegistrationUtilityImpl::OnReceivedToken(
    const GoogleServiceAuthError& error,
    const std::string& token) {
  if (error.state() != GoogleServiceAuthError::NONE) {
    CompleteRegistration(true, error);
    return;
  }

  DCHECK(!token.empty());
  pending_supervised_user_token_ = token;
  CompleteRegistrationIfReady();
}

void SupervisedUserRegistrationUtilityImpl::CompleteRegistrationIfReady() {
  bool skip_check = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kNoSupervisedUserAcknowledgmentCheck);

  if (!pending_supervised_user_acknowledged_ && !skip_check)
    return;
  if (password_update_ && !skip_check)
    return;
  if (pending_supervised_user_token_.empty())
    return;

  GoogleServiceAuthError error(GoogleServiceAuthError::NONE);
  CompleteRegistration(true, error);
}

void SupervisedUserRegistrationUtilityImpl::AbortPendingRegistration(
    bool run_callback,
    const GoogleServiceAuthError& error) {
  pending_supervised_user_token_.clear();
  CompleteRegistration(run_callback, error);
}

void SupervisedUserRegistrationUtilityImpl::CompleteRegistration(
    bool run_callback,
    const GoogleServiceAuthError& error) {
  if (callback_.is_null())
    return;

  if (pending_supervised_user_token_.empty()) {
    DCHECK(!pending_supervised_user_id_.empty());

    if (!is_existing_supervised_user_) {
      // Remove the pending supervised user if we weren't successful.
      // However, check that we are not importing a supervised user
      // before deleting it from sync to avoid accidental deletion of
      // existing supervised users by just canceling the registration for
      // example.
      supervised_user_sync_service_->DeleteSupervisedUser(
          pending_supervised_user_id_);
    } else if (avatar_updated_) {
      // Canceling (or failing) a supervised user import that did set the avatar
      // should undo this change.
      supervised_user_sync_service_->ClearSupervisedUserAvatar(
          pending_supervised_user_id_);
    }
  }

  if (run_callback)
    callback_.Run(error, pending_supervised_user_token_);
  callback_.Reset();
}

} // namespace
