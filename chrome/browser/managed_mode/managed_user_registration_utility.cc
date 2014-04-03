// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/managed_mode/managed_user_registration_utility.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/rand_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/managed_mode/managed_user_constants.h"
#include "chrome/browser/managed_mode/managed_user_refresh_token_fetcher.h"
#include "chrome/browser/managed_mode/managed_user_shared_settings_service.h"
#include "chrome/browser/managed_mode/managed_user_shared_settings_service_factory.h"
#include "chrome/browser/managed_mode/managed_user_shared_settings_update.h"
#include "chrome/browser/managed_mode/managed_user_sync_service.h"
#include "chrome/browser/managed_mode/managed_user_sync_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/sync/glue/device_info.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"

using base::DictionaryValue;

namespace {

ManagedUserRegistrationUtility* g_instance_for_tests = NULL;

// Actual implementation of ManagedUserRegistrationUtility.
class ManagedUserRegistrationUtilityImpl
    : public ManagedUserRegistrationUtility,
      public ManagedUserSyncServiceObserver {
 public:
  ManagedUserRegistrationUtilityImpl(
      PrefService* prefs,
      scoped_ptr<ManagedUserRefreshTokenFetcher> token_fetcher,
      ManagedUserSyncService* service,
      ManagedUserSharedSettingsService* shared_settings_service);

  virtual ~ManagedUserRegistrationUtilityImpl();

  // Registers a new managed user with the server. |managed_user_id| is a new
  // unique ID for the new managed user. If its value is the same as that of
  // of one of the existing managed users, then the same user will be created
  // on this machine (and if he has no avatar in sync, his avatar will
  // be updated). |info| contains necessary information like
  // the display name of the user and his avatar. |callback| is called
  // with the result of the registration. We use the info here and not the
  // profile, because on Chrome OS the profile of the managed user does not
  // yet exist.
  virtual void Register(const std::string& managed_user_id,
                        const ManagedUserRegistrationInfo& info,
                        const RegistrationCallback& callback) OVERRIDE;

  // ManagedUserSyncServiceObserver:
  virtual void OnManagedUserAcknowledged(const std::string& managed_user_id)
      OVERRIDE;
  virtual void OnManagedUsersSyncingStopped() OVERRIDE;
  virtual void OnManagedUsersChanged() OVERRIDE;

 private:
  // Fetches the managed user token when we have the device name.
  void FetchToken(const std::string& client_name);

  // Called when we have received a token for the managed user.
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

  // ManagedUserSharedSettingsUpdate acknowledgment callback for password data
  // in shared settings.
  void OnPasswordChangeAcknowledged(bool success);

  PrefService* prefs_;
  scoped_ptr<ManagedUserRefreshTokenFetcher> token_fetcher_;

  // A |KeyedService| owned by the custodian profile.
  ManagedUserSyncService* managed_user_sync_service_;

  // A |KeyedService| owned by the custodian profile.
  ManagedUserSharedSettingsService* managed_user_shared_settings_service_;

  std::string pending_managed_user_id_;
  std::string pending_managed_user_token_;
  bool pending_managed_user_acknowledged_;
  bool is_existing_managed_user_;
  bool avatar_updated_;
  RegistrationCallback callback_;
  scoped_ptr<ManagedUserSharedSettingsUpdate> password_update_;

  base::WeakPtrFactory<ManagedUserRegistrationUtilityImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ManagedUserRegistrationUtilityImpl);
};

} // namespace

ManagedUserRegistrationInfo::ManagedUserRegistrationInfo(
    const base::string16& name,
    int avatar_index)
    : avatar_index(avatar_index),
      name(name) {
}

ManagedUserRegistrationInfo::~ManagedUserRegistrationInfo() {}

ScopedTestingManagedUserRegistrationUtility::
    ScopedTestingManagedUserRegistrationUtility(
        ManagedUserRegistrationUtility* instance) {
  ManagedUserRegistrationUtility::SetUtilityForTests(instance);
}

ScopedTestingManagedUserRegistrationUtility::
    ~ScopedTestingManagedUserRegistrationUtility() {
  ManagedUserRegistrationUtility::SetUtilityForTests(NULL);
}

// static
scoped_ptr<ManagedUserRegistrationUtility>
ManagedUserRegistrationUtility::Create(Profile* profile) {
  if (g_instance_for_tests) {
    ManagedUserRegistrationUtility* result = g_instance_for_tests;
    g_instance_for_tests = NULL;
    return make_scoped_ptr(result);
  }

  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile);
  SigninManagerBase* signin_manager =
      SigninManagerFactory::GetForProfile(profile);
  scoped_ptr<ManagedUserRefreshTokenFetcher> token_fetcher =
      ManagedUserRefreshTokenFetcher::Create(
          token_service,
          signin_manager->GetAuthenticatedAccountId(),
          profile->GetRequestContext());
  ManagedUserSyncService* managed_user_sync_service =
      ManagedUserSyncServiceFactory::GetForProfile(profile);
  ManagedUserSharedSettingsService* managed_user_shared_settings_service =
      ManagedUserSharedSettingsServiceFactory::GetForBrowserContext(profile);
  return make_scoped_ptr(ManagedUserRegistrationUtility::CreateImpl(
      profile->GetPrefs(),
      token_fetcher.Pass(),
      managed_user_sync_service,
      managed_user_shared_settings_service));
}

// static
std::string ManagedUserRegistrationUtility::GenerateNewManagedUserId() {
  std::string new_managed_user_id;
  base::Base64Encode(base::RandBytesAsString(8), &new_managed_user_id);
  return new_managed_user_id;
}

// static
void ManagedUserRegistrationUtility::SetUtilityForTests(
    ManagedUserRegistrationUtility* utility) {
  if (g_instance_for_tests)
    delete g_instance_for_tests;
  g_instance_for_tests = utility;
}

// static
ManagedUserRegistrationUtility* ManagedUserRegistrationUtility::CreateImpl(
      PrefService* prefs,
      scoped_ptr<ManagedUserRefreshTokenFetcher> token_fetcher,
      ManagedUserSyncService* service,
      ManagedUserSharedSettingsService* shared_settings_service) {
  return new ManagedUserRegistrationUtilityImpl(prefs,
                                                token_fetcher.Pass(),
                                                service,
                                                shared_settings_service);
}

namespace {

ManagedUserRegistrationUtilityImpl::ManagedUserRegistrationUtilityImpl(
    PrefService* prefs,
    scoped_ptr<ManagedUserRefreshTokenFetcher> token_fetcher,
    ManagedUserSyncService* service,
    ManagedUserSharedSettingsService* shared_settings_service)
    : prefs_(prefs),
      token_fetcher_(token_fetcher.Pass()),
      managed_user_sync_service_(service),
      managed_user_shared_settings_service_(shared_settings_service),
      pending_managed_user_acknowledged_(false),
      is_existing_managed_user_(false),
      avatar_updated_(false),
      weak_ptr_factory_(this) {
  managed_user_sync_service_->AddObserver(this);
}

ManagedUserRegistrationUtilityImpl::~ManagedUserRegistrationUtilityImpl() {
  managed_user_sync_service_->RemoveObserver(this);
  CancelPendingRegistration();
}

void ManagedUserRegistrationUtilityImpl::Register(
    const std::string& managed_user_id,
    const ManagedUserRegistrationInfo& info,
    const RegistrationCallback& callback) {
  DCHECK(pending_managed_user_id_.empty());
  callback_ = callback;
  pending_managed_user_id_ = managed_user_id;

  const base::DictionaryValue* dict =
      prefs_->GetDictionary(prefs::kManagedUsers);
  is_existing_managed_user_ = dict->HasKey(managed_user_id);
  if (!is_existing_managed_user_) {
    managed_user_sync_service_->AddManagedUser(pending_managed_user_id_,
                                               base::UTF16ToUTF8(info.name),
                                               info.master_key,
                                               info.password_signature_key,
                                               info.password_encryption_key,
                                               info.avatar_index);
  } else {
    const base::DictionaryValue* value = NULL;
    bool success =
        dict->GetDictionaryWithoutPathExpansion(managed_user_id, &value);
    DCHECK(success);
    std::string key;
    bool need_keys = !info.password_signature_key.empty() ||
                     !info.password_encryption_key.empty();
    bool have_keys =
        value->GetString(ManagedUserSyncService::kPasswordSignatureKey, &key) &&
        !key.empty() &&
        value->GetString(ManagedUserSyncService::kPasswordEncryptionKey,
                         &key) &&
        !key.empty();

    bool keys_need_update = need_keys && !have_keys;

    if (keys_need_update) {
      managed_user_sync_service_->UpdateManagedUser(
          pending_managed_user_id_,
          base::UTF16ToUTF8(info.name),
          info.master_key,
          info.password_signature_key,
          info.password_encryption_key,
          info.avatar_index);
    } else {
      // The user already exists and does not need to be updated.
      OnManagedUserAcknowledged(managed_user_id);
    }
    avatar_updated_ =
        managed_user_sync_service_->UpdateManagedUserAvatarIfNeeded(
            managed_user_id,
            info.avatar_index);
  }
#if defined(OS_CHROMEOS)
  const char* kAvatarKey = managed_users::kChromeOSAvatarIndex;
#else
  const char* kAvatarKey = managed_users::kChromeAvatarIndex;
#endif
  managed_user_shared_settings_service_->SetValue(
      pending_managed_user_id_, kAvatarKey,
      base::FundamentalValue(info.avatar_index));
  if (!info.password_data.empty()) {
    password_update_.reset(new ManagedUserSharedSettingsUpdate(
        managed_user_shared_settings_service_,
        pending_managed_user_id_,
        managed_users::kChromeOSPasswordData,
        scoped_ptr<base::Value>(info.password_data.DeepCopy()),
        base::Bind(
            &ManagedUserRegistrationUtilityImpl::OnPasswordChangeAcknowledged,
            weak_ptr_factory_.GetWeakPtr())));
  }

  browser_sync::DeviceInfo::GetClientName(
      base::Bind(&ManagedUserRegistrationUtilityImpl::FetchToken,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ManagedUserRegistrationUtilityImpl::CancelPendingRegistration() {
  AbortPendingRegistration(
      false,  // Don't run the callback. The error will be ignored.
      GoogleServiceAuthError(GoogleServiceAuthError::NONE));
}

void ManagedUserRegistrationUtilityImpl::OnManagedUserAcknowledged(
    const std::string& managed_user_id) {
  DCHECK_EQ(pending_managed_user_id_, managed_user_id);
  DCHECK(!pending_managed_user_acknowledged_);
  pending_managed_user_acknowledged_ = true;
  CompleteRegistrationIfReady();
}

void ManagedUserRegistrationUtilityImpl::OnPasswordChangeAcknowledged(
    bool success) {
  DCHECK(password_update_);
  DCHECK(success);
  password_update_.reset();
  CompleteRegistrationIfReady();
}

void ManagedUserRegistrationUtilityImpl::OnManagedUsersSyncingStopped() {
  AbortPendingRegistration(
      true,  // Run the callback.
      GoogleServiceAuthError(GoogleServiceAuthError::REQUEST_CANCELED));
}

void ManagedUserRegistrationUtilityImpl::OnManagedUsersChanged() {}

void ManagedUserRegistrationUtilityImpl::FetchToken(
    const std::string& client_name) {
  token_fetcher_->Start(
      pending_managed_user_id_, client_name,
      base::Bind(&ManagedUserRegistrationUtilityImpl::OnReceivedToken,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ManagedUserRegistrationUtilityImpl::OnReceivedToken(
    const GoogleServiceAuthError& error,
    const std::string& token) {
  if (error.state() != GoogleServiceAuthError::NONE) {
    CompleteRegistration(true, error);
    return;
  }

  DCHECK(!token.empty());
  pending_managed_user_token_ = token;
  CompleteRegistrationIfReady();
}

void ManagedUserRegistrationUtilityImpl::CompleteRegistrationIfReady() {
  bool skip_check = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kNoManagedUserAcknowledgmentCheck);

  if (!pending_managed_user_acknowledged_ && !skip_check)
    return;
  if (password_update_ && !skip_check)
    return;
  if (pending_managed_user_token_.empty())
    return;

  GoogleServiceAuthError error(GoogleServiceAuthError::NONE);
  CompleteRegistration(true, error);
}

void ManagedUserRegistrationUtilityImpl::AbortPendingRegistration(
    bool run_callback,
    const GoogleServiceAuthError& error) {
  pending_managed_user_token_.clear();
  CompleteRegistration(run_callback, error);
}

void ManagedUserRegistrationUtilityImpl::CompleteRegistration(
    bool run_callback,
    const GoogleServiceAuthError& error) {
  if (callback_.is_null())
    return;

  if (pending_managed_user_token_.empty()) {
    DCHECK(!pending_managed_user_id_.empty());

    if (!is_existing_managed_user_) {
      // Remove the pending managed user if we weren't successful.
      // However, check that we are not importing a managed user
      // before deleting it from sync to avoid accidental deletion of
      // existing managed users by just canceling the registration for example.
      managed_user_sync_service_->DeleteManagedUser(pending_managed_user_id_);
    } else if (avatar_updated_) {
      // Canceling (or failing) a managed user import that did set the avatar
      // should undo this change.
      managed_user_sync_service_->ClearManagedUserAvatar(
          pending_managed_user_id_);
    }
  }

  if (run_callback)
    callback_.Run(error, pending_managed_user_token_);
  callback_.Reset();
}

} // namespace
