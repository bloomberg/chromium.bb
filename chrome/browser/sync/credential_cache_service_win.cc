// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/credential_cache_service_win.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
#include "base/string_number_conversions.h"
#include "base/time.h"
#include "base/values.h"
#include "base/win/windows_version.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/sync/credential_cache_service_factory_win.h"
#include "chrome/browser/sync/glue/chrome_encryptor.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/gaia_constants.h"
#include "sync/internal_api/public/base/model_type.h"

namespace syncer {

// The time delay (in seconds) between two consecutive polls of the alternate
// credential cache. A two minute delay seems like a reasonable amount of time
// in which to propagate changes to signed in state between Metro and Desktop.
const int kCredentialCachePollIntervalSecs = 2 * 60;

// Keeps track of the last time a credential cache was written to. Used to make
// sure that we only apply changes from newer credential caches to older ones,
// and not vice versa.
const char kLastCacheUpdateTimeKey[] = "last_cache_update_time";

// Deprecated. We were previously using base::TimeTicks as a timestamp. This was
// bad because TimeTicks values roll over on machine restart. We now use
// base::Time as a timestamp. However, we can't simply reuse "last_updated_time"
// because it may result in a Time value being compared with a TimeTicks value.
const char kLastUpdatedTimeTicksDeprecated[] = "last_updated_time";

using base::TimeDelta;
using content::BrowserThread;

CredentialCacheService::CredentialCacheService(Profile* profile)
    : profile_(profile),
      // |profile_| is null in unit tests.
      sync_prefs_(profile_ ? profile_->GetPrefs() : NULL),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  if (profile_) {
    InitializeLocalCredentialCacheWriter();
    // If sync is not disabled, look for credentials in the alternate profile.
    // Note that we do want to look for credentials in the alternate profile
    // even if the local user is signed in, so that we can detect a sign out or
    // reconfigure originating from the alternate profile.
    if (!sync_prefs_.IsManaged())
      LookForCachedCredentialsInAlternateProfile();
  }
}

CredentialCacheService::~CredentialCacheService() {
  Shutdown();
}

void CredentialCacheService::Shutdown() {
  if (local_store_.get())
    local_store_->CommitPendingWrite();
  local_store_observer_.release();
  local_store_.release();
  alternate_store_observer_.release();
  alternate_store_.release();
}

void CredentialCacheService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(local_store_.get());
  switch (type) {
    case chrome::NOTIFICATION_PREF_CHANGED: {
      // One of the two sync encryption tokens has changed. Update its value in
      // the local cache.
      const std::string pref_name =
          *(content::Details<const std::string>(details).ptr());
      if (pref_name == prefs::kSyncEncryptionBootstrapToken) {
        PackAndUpdateStringPref(pref_name,
                                sync_prefs_.GetEncryptionBootstrapToken());
      } else if (pref_name == prefs::kSyncKeystoreEncryptionBootstrapToken) {
        PackAndUpdateStringPref(
            pref_name,
            sync_prefs_.GetKeystoreEncryptionBootstrapToken());
      } else {
        NOTREACHED() "Invalid pref name " << pref_name << ".";
      }
      break;
    }

    case chrome::NOTIFICATION_GOOGLE_SIGNED_OUT: {
      // The user has signed out. Write blank values to the google username,
      // encryption tokens and token service credentials in the local cache.
      PackAndUpdateStringPref(prefs::kGoogleServicesUsername, std::string());
      if (HasPref(local_store_, prefs::kSyncEncryptionBootstrapToken)) {
        PackAndUpdateStringPref(prefs::kSyncEncryptionBootstrapToken,
                                std::string());
      }
      if (HasPref(local_store_, prefs::kSyncKeystoreEncryptionBootstrapToken)) {
        PackAndUpdateStringPref(prefs::kSyncKeystoreEncryptionBootstrapToken,
                                std::string());
      }
      PackAndUpdateStringPref(GaiaConstants::kGaiaLsid, std::string());
      PackAndUpdateStringPref(GaiaConstants::kGaiaSid, std::string());
      break;
    }

    case chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL: {
      // The user has signed in. Write the new value of the google username to
      // the local cache.
      SigninManager* signin = SigninManagerFactory::GetForProfile(profile_);
      PackAndUpdateStringPref(prefs::kGoogleServicesUsername,
                              signin->GetAuthenticatedUsername());
      break;
    }

    case chrome::NOTIFICATION_SYNC_CONFIGURE_DONE: {
      // Local sync configuration is done. The sync service is now ready to
      // be reconfigured. Immediately look for any unconsumed config changes in
      // the alternate profile. If all changes have been consumed, this is a
      // no-op.
      ScheduleNextReadFromAlternateCredentialCache(0);
      break;
    }

    case chrome::NOTIFICATION_SYNC_CONFIGURE_START: {
      // We have detected a sync sign in, auto-start or reconfigure. Write the
      // latest sync preferences to the local cache.
      WriteSyncPrefsToLocalCache();
      break;
    }

    case chrome::NOTIFICATION_TOKEN_LOADING_FINISHED: {
      // The token service has been fully initialized. Update the token service
      // credentials in the local cache. This is a no-op if the cache already
      // contains the latest values.
      TokenService* token_service =
          TokenServiceFactory::GetForProfile(profile_);
      if (token_service->AreCredentialsValid()) {
        GaiaAuthConsumer::ClientLoginResult credentials =
            token_service->credentials();
        PackAndUpdateStringPref(GaiaConstants::kGaiaLsid, credentials.lsid);
        PackAndUpdateStringPref(GaiaConstants::kGaiaSid, credentials.sid);
      }
      break;
    }

    case chrome::NOTIFICATION_TOKEN_SERVICE_CREDENTIALS_UPDATED: {
      // The token service has new credentials. Write them to the local cache.
      const TokenService::CredentialsUpdatedDetails& token_details =
          *(content::Details<const TokenService::CredentialsUpdatedDetails>(
              details).ptr());
      PackAndUpdateStringPref(GaiaConstants::kGaiaLsid, token_details.lsid());
      PackAndUpdateStringPref(GaiaConstants::kGaiaSid, token_details.sid());
      break;
    }

    case chrome::NOTIFICATION_TOKENS_CLEARED: {
      // Tokens have been cleared. Blank out lsid and sid in the local cache.
      PackAndUpdateStringPref(GaiaConstants::kGaiaLsid, std::string());
      PackAndUpdateStringPref(GaiaConstants::kGaiaSid, std::string());
      break;
    }

    default: {
      NOTREACHED();
      break;
    }
  }
}

void CredentialCacheService::ReadCachedCredentialsFromAlternateProfile() {
  // If the local user has signed in and signed out, we do not consume cached
  // credentials from the alternate profile. There is nothing more to do, now or
  // later on.
  if (HasUserSignedOut())
    return;

  // Sanity check the alternate credential cache. Note that it is sufficient to
  // have just one of the two sync encryption tokens. If any string credentials
  // are outright missing even though the file exists, something is awry with
  // the alternate profile store. There is no sense in flagging an error as the
  // problem lies in a different profile directory. There is nothing to do now.
  // We schedule a future read from the alternate credential cache and return.
  DCHECK(alternate_store_.get());
  if (!HasPref(alternate_store_, prefs::kGoogleServicesUsername) ||
      !HasPref(alternate_store_, GaiaConstants::kGaiaLsid) ||
      !HasPref(alternate_store_, GaiaConstants::kGaiaSid) ||
      !(HasPref(alternate_store_, prefs::kSyncEncryptionBootstrapToken) ||
        HasPref(alternate_store_,
                prefs::kSyncKeystoreEncryptionBootstrapToken)) ||
      !HasPref(alternate_store_, prefs::kSyncKeepEverythingSynced)) {
    VLOG(1) << "Could not find cached credentials in \""
            << GetCredentialPathInAlternateProfile().value() << "\".";
    ScheduleNextReadFromAlternateCredentialCache(
        kCredentialCachePollIntervalSecs);
    return;
  }

  // Extract the google username, lsid and sid from the alternate credential
  // cache.
  std::string alternate_google_services_username =
      GetAndUnpackStringPref(alternate_store_, prefs::kGoogleServicesUsername);
  std::string alternate_lsid =
      GetAndUnpackStringPref(alternate_store_, GaiaConstants::kGaiaLsid);
  std::string alternate_sid =
      GetAndUnpackStringPref(alternate_store_, GaiaConstants::kGaiaSid);

  // Extract the sync encryption tokens from the alternate credential cache.
  // Both tokens may not be found, since only one of them is used at any time.
  std::string alternate_encryption_bootstrap_token;
  if (HasPref(alternate_store_, prefs::kSyncEncryptionBootstrapToken)) {
    alternate_encryption_bootstrap_token =
        GetAndUnpackStringPref(alternate_store_,
                               prefs::kSyncEncryptionBootstrapToken);
  }
  std::string alternate_keystore_encryption_bootstrap_token;
  if (HasPref(alternate_store_, prefs::kSyncKeystoreEncryptionBootstrapToken)) {
    alternate_keystore_encryption_bootstrap_token =
        GetAndUnpackStringPref(alternate_store_,
                               prefs::kSyncKeystoreEncryptionBootstrapToken);
  }

  // Sign out of sync if the alternate profile has signed out the same user.
  // There is no need to schedule any more reads of the alternate profile
  // cache because we only apply cached credentials for first-time sign-ins.
  if (ShouldSignOutOfSync(alternate_google_services_username)) {
    VLOG(1) << "User has signed out on the other profile. Signing out.";
    InitiateSignOut();
    return;
  }

  // Extract cached sync prefs from the alternate credential cache.
  bool alternate_keep_everything_synced =
      GetBooleanPref(alternate_store_, prefs::kSyncKeepEverythingSynced);
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  ModelTypeSet registered_types = service->GetRegisteredDataTypes();
  ModelTypeSet alternate_preferred_types;
  for (ModelTypeSet::Iterator it = registered_types.First();
       it.Good();
       it.Inc()) {
    std::string datatype_pref_name =
        browser_sync::SyncPrefs::GetPrefNameForDataType(it.Get());
    if (!HasPref(alternate_store_, datatype_pref_name)) {
      // If there is no cached pref for a specific data type, it means that the
      // user originally signed in with an older version of Chrome, and then
      // upgraded to a version with a new datatype. In such cases, we leave the
      // default initial datatype setting as false while reading cached
      // credentials, just like we do in SyncPrefs::RegisterPreferences.
      VLOG(1) << "Could not find cached datatype pref for "
              << datatype_pref_name << " in "
              << GetCredentialPathInAlternateProfile().value() << ".";
      continue;
    }
    if (GetBooleanPref(alternate_store_, datatype_pref_name))
      alternate_preferred_types.Put(it.Get());
  }

  // Reconfigure if sync settings, encryption tokens or token service
  // credentials have changed in the alternate profile, but for the same user
  // that is signed in to the local profile.
  if (MayReconfigureSync(alternate_google_services_username)) {
    if (HaveSyncPrefsChanged(alternate_keep_everything_synced,
                             alternate_preferred_types)) {
      VLOG(1) << "Sync prefs have changed in other profile. Reconfiguring.";
      service->OnUserChoseDatatypes(alternate_keep_everything_synced,
                                    alternate_preferred_types);
    }
    if (HaveSyncEncryptionTokensChanged(
        alternate_encryption_bootstrap_token,
        alternate_keystore_encryption_bootstrap_token)) {
      VLOG(1) << "Sync encryption tokens have changed in other profile.";
      sync_prefs_.SetEncryptionBootstrapToken(
          alternate_encryption_bootstrap_token);
      sync_prefs_.SetKeystoreEncryptionBootstrapToken(
          alternate_keystore_encryption_bootstrap_token);
    }
    if (HaveTokenServiceCredentialsChanged(alternate_lsid, alternate_sid)) {
      VLOG(1) << "Token service credentials have changed in other profile.";
      UpdateTokenServiceCredentials(alternate_lsid, alternate_sid);
    }
  }

  // Sign in if we notice new cached credentials in the alternate profile.
  if (ShouldSignInToSync(alternate_google_services_username,
                         alternate_lsid,
                         alternate_sid,
                         alternate_encryption_bootstrap_token,
                         alternate_keystore_encryption_bootstrap_token)) {
    InitiateSignInWithCachedCredentials(
        alternate_google_services_username,
        alternate_encryption_bootstrap_token,
        alternate_keystore_encryption_bootstrap_token,
        alternate_keep_everything_synced,
        alternate_preferred_types);
    UpdateTokenServiceCredentials(alternate_lsid, alternate_sid);
  }

  // Schedule the next read from the alternate credential cache so that we can
  // detect future reconfigures or sign outs.
  ScheduleNextReadFromAlternateCredentialCache(
      kCredentialCachePollIntervalSecs);
}

void CredentialCacheService::WriteSyncPrefsToLocalCache() {
  UpdateBooleanPref(prefs::kSyncKeepEverythingSynced,
                    sync_prefs_.HasKeepEverythingSynced());
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  ModelTypeSet registered_types = service->GetRegisteredDataTypes();
  for (ModelTypeSet::Iterator it = registered_types.First();
       it.Good();
       it.Inc()) {
    std::string datatype_pref_name =
        browser_sync::SyncPrefs::GetPrefNameForDataType(it.Get());
    UpdateBooleanPref(
        datatype_pref_name,
        profile_->GetPrefs()->GetBoolean(datatype_pref_name.c_str()));
  }
}

void CredentialCacheService::ScheduleNextReadFromAlternateCredentialCache(
    int delay_secs) {
  DCHECK_LE(0, delay_secs);
  // We must reinitialize |alternate_store_| here because the underlying
  // credential file in the alternate profile might have changed, and we must
  // re-read it afresh.
  alternate_store_observer_.release();
  alternate_store_.release();
  next_read_.Reset(base::Bind(
      &CredentialCacheService::LookForCachedCredentialsInAlternateProfile,
      weak_factory_.GetWeakPtr()));
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      next_read_.callback(),
      TimeDelta::FromSeconds(delay_secs));
}

bool CredentialCacheService::HasPref(scoped_refptr<JsonPrefStore> store,
                                     const std::string& pref_name) {
  return (store->GetValue(pref_name, NULL) == PrefStore::READ_OK);
}

// static
base::StringValue* CredentialCacheService::PackCredential(
    const std::string& credential) {
  // Do nothing for empty credentials.
  if (credential.empty())
    return base::Value::CreateStringValue("");

  browser_sync::ChromeEncryptor encryptor;
  std::string encrypted;
  if (!encryptor.EncryptString(credential, &encrypted)) {
    NOTREACHED();
    return base::Value::CreateStringValue(std::string());
  }

  std::string encoded;
  if (!base::Base64Encode(encrypted, &encoded)) {
    NOTREACHED();
    return base::Value::CreateStringValue(std::string());
  }

  return base::Value::CreateStringValue(encoded);
}

// static
std::string CredentialCacheService::UnpackCredential(
    const base::Value& packed) {
  std::string encoded;
  if (!packed.GetAsString(&encoded)) {
    NOTREACHED();
    return std::string();
  }

  // Do nothing for empty credentials.
  if (encoded.empty())
    return std::string();

  std::string encrypted;
  if (!base::Base64Decode(encoded, &encrypted)) {
    NOTREACHED();
    return std::string();
  }

  browser_sync::ChromeEncryptor encryptor;
  std::string unencrypted;
  if (!encryptor.DecryptString(encrypted, &unencrypted)) {
    NOTREACHED();
    return std::string();
  }

  return unencrypted;
}

void CredentialCacheService::WriteLastCacheUpdateTime() {
  DCHECK(local_store_.get());
  int64 last_cache_update_time_int64 = base::Time::Now().ToInternalValue();
  std::string last_cache_update_time_string =
      base::Int64ToString(last_cache_update_time_int64);
  local_store_->SetValueSilently(
      kLastCacheUpdateTimeKey,
      base::Value::CreateStringValue(last_cache_update_time_string));
}

void CredentialCacheService::PackAndUpdateStringPref(
      const std::string& pref_name,
      const std::string& new_value) {
  DCHECK(local_store_.get());
  if (HasPref(local_store_, pref_name) &&
      GetAndUnpackStringPref(local_store_, pref_name) == new_value) {
    return;
  }
  if (!HasUserSignedOut()) {
    local_store_->SetValueSilently(pref_name, PackCredential(new_value));
  } else {
    // Write a blank value since we cache credentials only for first-time
    // sign-ins.
    local_store_->SetValueSilently(pref_name, PackCredential(std::string()));
  }
  WriteLastCacheUpdateTime();
}

void CredentialCacheService::UpdateBooleanPref(const std::string& pref_name,
                                               bool new_value) {
  DCHECK(local_store_.get());
  if (HasPref(local_store_, pref_name) &&
      GetBooleanPref(local_store_, pref_name) == new_value) {
    return;
  }
  if (!HasUserSignedOut()) {
    local_store_->SetValueSilently(pref_name,
                                   base::Value::CreateBooleanValue(new_value));
  } else {
    // Write a default value of false since we cache credentials only for
    // first-time sign-ins.
    local_store_->SetValueSilently(pref_name,
                                   base::Value::CreateBooleanValue(false));
  }
  WriteLastCacheUpdateTime();
}

base::Time CredentialCacheService::GetLastCacheUpdateTime(
    scoped_refptr<JsonPrefStore> store) {
  DCHECK(HasPref(store, kLastCacheUpdateTimeKey));
  const base::Value* last_cache_update_time_value = NULL;
  store->GetValue(kLastCacheUpdateTimeKey, &last_cache_update_time_value);
  DCHECK(last_cache_update_time_value);
  std::string last_cache_update_time_string;
  last_cache_update_time_value->GetAsString(&last_cache_update_time_string);
  int64 last_cache_update_time_int64;
  bool success = base::StringToInt64(last_cache_update_time_string,
                                     &last_cache_update_time_int64);
  DCHECK(success);
  return base::Time::FromInternalValue(last_cache_update_time_int64);
}

bool CredentialCacheService::AlternateCacheIsMoreRecent() {
  DCHECK(alternate_store_.get());
  // If the alternate credential cache doesn't have the "last_cache_update_time"
  // field, it was written by an older version of chrome, and we therefore
  // consider the local cache to be more recent.
  if (!HasPref(alternate_store_, kLastCacheUpdateTimeKey))
    return false;
  DCHECK(HasPref(local_store_, kLastCacheUpdateTimeKey));
  return GetLastCacheUpdateTime(alternate_store_) >
         GetLastCacheUpdateTime(local_store_);
}

std::string CredentialCacheService::GetAndUnpackStringPref(
    scoped_refptr<JsonPrefStore> store,
    const std::string& pref_name) {
  const base::Value* pref_value = NULL;
  store->GetValue(pref_name, &pref_value);
  return UnpackCredential(*pref_value);
}

bool CredentialCacheService::GetBooleanPref(
    scoped_refptr<JsonPrefStore> store,
    const std::string& pref_name) {
  const base::Value* pref_value = NULL;
  store->GetValue(pref_name, &pref_value);
  bool pref;
  pref_value->GetAsBoolean(&pref);
  return pref;
}

CredentialCacheService::LocalStoreObserver::LocalStoreObserver(
    CredentialCacheService* service,
    scoped_refptr<JsonPrefStore> local_store)
    : service_(service),
      local_store_(local_store) {
  local_store_->AddObserver(this);
}

CredentialCacheService::LocalStoreObserver::~LocalStoreObserver() {
  local_store_->RemoveObserver(this);
}

void CredentialCacheService::LocalStoreObserver::OnInitializationCompleted(
    bool succeeded) {
  // Note that |succeeded| will be true even if the local cache file wasn't
  // found, so long as its parent dir (the chrome profile directory) was found.
  // If |succeeded| is false, it means that the chrome profile directory is
  // missing. In this case, there's nothing we can do other than DCHECK.
  DCHECK(succeeded);

  // During startup, we do a precautionary write of the google username,
  // encryption tokens, sync prefs and last cache update time to the local cache
  // in order to recover from the following cases:
  // 1) There is no local credential cache, but the user is signed in. This
  //    could happen if a signed-in user restarts chrome after upgrading from
  //    an older version that didn't support credential caching.
  // 2) There is a local credential cache, but we missed writing sync credential
  //    updates to it in the past due to a crash, or due to the user exiting
  //    chrome in the midst of a sign in, sign out or reconfigure.
  // 3) There is a local credential cache that was written to by an older
  //    version of Chrome, and it does not contain the "last_cache_update_time"
  //    field.
  // Note: If the local credential cache was already up-to-date, the operations
  // below will be no-ops, and won't change the cache's last updated time. Also,
  // if the user is not signed in and there is no local credential cache, we
  // don't want to create a cache with empty values.
  SigninManager* signin =
      SigninManagerFactory::GetForProfile(service_->profile_);
  if ((local_store_->GetReadError() == JsonPrefStore::PREF_READ_ERROR_NO_FILE &&
       !signin->GetAuthenticatedUsername().empty()) ||
      (local_store_->GetReadError() == JsonPrefStore::PREF_READ_ERROR_NONE)) {
    service_->PackAndUpdateStringPref(prefs::kGoogleServicesUsername,
                                      signin->GetAuthenticatedUsername());
    if (!service_->sync_prefs_.GetEncryptionBootstrapToken().empty()) {
      service_->PackAndUpdateStringPref(
          prefs::kSyncEncryptionBootstrapToken,
          service_->sync_prefs_.GetEncryptionBootstrapToken());
    }
    if (!service_->sync_prefs_.GetKeystoreEncryptionBootstrapToken().empty()) {
      service_->PackAndUpdateStringPref(
          prefs::kSyncKeystoreEncryptionBootstrapToken,
          service_->sync_prefs_.GetKeystoreEncryptionBootstrapToken());
    }
    service_->WriteSyncPrefsToLocalCache();
    if (!service_->HasPref(local_store_, kLastCacheUpdateTimeKey))
      service_->WriteLastCacheUpdateTime();
    if (service_->HasPref(local_store_, kLastUpdatedTimeTicksDeprecated))
      local_store_->RemoveValue(kLastUpdatedTimeTicksDeprecated);
  }

  // Now that the local credential cache is ready, start listening for events
  // associated with various sync config changes.
  service_->StartListeningForSyncConfigChanges();
}

void CredentialCacheService::LocalStoreObserver::OnPrefValueChanged(
    const std::string& key) {
  // Nothing to do here, since credentials are cached silently.
}

CredentialCacheService::AlternateStoreObserver::AlternateStoreObserver(
    CredentialCacheService* service,
    scoped_refptr<JsonPrefStore> alternate_store)
    : service_(service),
      alternate_store_(alternate_store) {
  alternate_store_->AddObserver(this);
}

CredentialCacheService::AlternateStoreObserver::~AlternateStoreObserver() {
  alternate_store_->RemoveObserver(this);
}

void CredentialCacheService::AlternateStoreObserver::OnInitializationCompleted(
    bool succeeded) {
  // If an alternate credential cache was found, begin consuming its contents.
  // If not, schedule a future read.
  if (succeeded &&
      alternate_store_->GetReadError() == JsonPrefStore::PREF_READ_ERROR_NONE) {
    service_->ReadCachedCredentialsFromAlternateProfile();
  } else {
    service_->ScheduleNextReadFromAlternateCredentialCache(
        kCredentialCachePollIntervalSecs);
  }
}

void CredentialCacheService::AlternateStoreObserver::OnPrefValueChanged(
    const std::string& key) {
  // Nothing to do here, since credentials are cached silently.
}

FilePath CredentialCacheService::GetCredentialPathInCurrentProfile() const {
  // The sync credential path in the default Desktop profile is
  // "%Appdata%\Local\Google\Chrome\User Data\Default\Sync Credentials", while
  // the sync credential path in the default Metro profile is
  // "%Appdata%\Local\Google\Chrome\Metro\User Data\Default\Sync Credentials".
  DCHECK(profile_);
  return profile_->GetPath().Append(chrome::kSyncCredentialsFilename);
}

FilePath CredentialCacheService::GetCredentialPathInAlternateProfile() const {
  DCHECK(profile_);
  FilePath alternate_user_data_dir;
  chrome::GetAlternateUserDataDirectory(&alternate_user_data_dir);

  // TODO(rsimha): This code path is to allow for testing in the presence of
  // strange singleton mode. Delete this block before shipping.
  // See http://crbug.com/144280.
  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kEnableSyncCredentialCaching) &&
      !CredentialCacheServiceFactory::IsDefaultProfile(profile_)) {
    DCHECK(CredentialCacheServiceFactory::IsDefaultAlternateProfileForTest(
               profile_));
    chrome::GetDefaultUserDataDirectory(&alternate_user_data_dir);
  }

  FilePath alternate_default_profile_dir =
      ProfileManager::GetDefaultProfileDir(alternate_user_data_dir);
  return alternate_default_profile_dir.Append(chrome::kSyncCredentialsFilename);
}

void CredentialCacheService::InitializeLocalCredentialCacheWriter() {
  local_store_ = new JsonPrefStore(
      GetCredentialPathInCurrentProfile(),
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::FILE));
  local_store_observer_ = new LocalStoreObserver(this, local_store_);
  local_store_->ReadPrefsAsync(NULL);
}

void CredentialCacheService::StartListeningForSyncConfigChanges() {
  // Register for notifications for google sign in and sign out.
  registrar_.Add(this,
                 chrome::NOTIFICATION_GOOGLE_SIGNED_OUT,
                 content::Source<Profile>(profile_));
  registrar_.Add(this,
                 chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
                 content::Source<Profile>(profile_));

  // Register for notifications for sync configuration changes that could occur
  // during sign in or reconfiguration.
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  registrar_.Add(this,
                 chrome::NOTIFICATION_SYNC_CONFIGURE_DONE,
                 content::Source<ProfileSyncService>(service));
  registrar_.Add(this,
                 chrome::NOTIFICATION_SYNC_CONFIGURE_START,
                 content::Source<ProfileSyncService>(service));

  // Register for notifications for updates to the sync encryption tokens, which
  // are stored in the PrefStore.
  pref_registrar_.Init(profile_->GetPrefs());
  pref_registrar_.Add(prefs::kSyncEncryptionBootstrapToken, this);
  pref_registrar_.Add(prefs::kSyncKeystoreEncryptionBootstrapToken, this);

  // Register for notifications for updates to lsid and sid, which are stored in
  // the TokenService.
  TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKEN_LOADING_FINISHED,
                 content::Source<TokenService>(token_service));
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKEN_SERVICE_CREDENTIALS_UPDATED,
                 content::Source<TokenService>(token_service));
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKENS_CLEARED,
                 content::Source<TokenService>(token_service));
}

void CredentialCacheService::LookForCachedCredentialsInAlternateProfile() {
  // Attempt to read cached credentials from the alternate profile. If no file
  // exists, ReadPrefsAsync() will cause PREF_READ_ERROR_NO_FILE to be returned
  // after initialization is complete.
  alternate_store_ = new JsonPrefStore(
      GetCredentialPathInAlternateProfile(),
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::FILE));
  alternate_store_observer_ = new AlternateStoreObserver(this,
                                                         alternate_store_);
  alternate_store_->ReadPrefsAsync(NULL);
}

bool CredentialCacheService::HasUserSignedOut() {
  DCHECK(local_store_.get());
  // If HasPref() is false, the user never signed in, since there are no
  // previously cached credentials. If the kGoogleServicesUsername pref is
  // empty, it means that the user signed in and subsequently signed out.
  return HasPref(local_store_, prefs::kGoogleServicesUsername) &&
         GetAndUnpackStringPref(local_store_,
                                prefs::kGoogleServicesUsername).empty();
}

void CredentialCacheService::InitiateSignInWithCachedCredentials(
    const std::string& google_services_username,
    const std::string& encryption_bootstrap_token,
    const std::string& keystore_encryption_bootstrap_token,
    bool keep_everything_synced,
    ModelTypeSet preferred_types) {
  // Update the google username in the SigninManager and PrefStore. Also update
  // its value in the local credential cache, since we will not send out
  // NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL in this case.
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  service->signin()->SetAuthenticatedUsername(google_services_username);
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername,
                                  google_services_username);
  PackAndUpdateStringPref(prefs::kGoogleServicesUsername,
                          service->signin()->GetAuthenticatedUsername());

  // Update sync encryption tokens after making sure at least one of them is
  // non-empty.
  DCHECK(!encryption_bootstrap_token.empty() ||
         !keystore_encryption_bootstrap_token.empty());
  if (!encryption_bootstrap_token.empty()) {
    sync_prefs_.SetEncryptionBootstrapToken(encryption_bootstrap_token);
  }
  if (!keystore_encryption_bootstrap_token.empty()) {
    sync_prefs_.SetKeystoreEncryptionBootstrapToken(
        keystore_encryption_bootstrap_token);
  }

  // Update the sync preferences.
  sync_prefs_.SetStartSuppressed(false);
  sync_prefs_.SetSyncSetupCompleted();
  sync_prefs_.SetKeepEverythingSynced(keep_everything_synced);
  sync_prefs_.SetPreferredDataTypes(service->GetRegisteredDataTypes(),
                                    preferred_types);
}

void CredentialCacheService::UpdateTokenServiceCredentials(
    const std::string& alternate_lsid,
    const std::string& alternate_sid) {
  GaiaAuthConsumer::ClientLoginResult login_result;
  login_result.lsid = alternate_lsid;
  login_result.sid = alternate_sid;
  TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
  token_service->UpdateCredentials(login_result);
  DCHECK(token_service->AreCredentialsValid());
  token_service->StartFetchingTokens();
}

void CredentialCacheService::InitiateSignOut() {
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  service->DisableForUser();
}

bool CredentialCacheService::HaveSyncPrefsChanged(
    bool alternate_keep_everything_synced,
    ModelTypeSet alternate_preferred_types) const {
  if (alternate_keep_everything_synced &&
      sync_prefs_.HasKeepEverythingSynced()) {
    return false;
  }
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  ModelTypeSet local_preferred_types =
      sync_prefs_.GetPreferredDataTypes(service->GetRegisteredDataTypes());
  return
      (alternate_keep_everything_synced !=
       sync_prefs_.HasKeepEverythingSynced()) ||
      !alternate_preferred_types.Equals(local_preferred_types);
}

bool CredentialCacheService::HaveSyncEncryptionTokensChanged(
    const std::string& alternate_encryption_bootstrap_token,
    const std::string& alternate_keystore_encryption_bootstrap_token) {
  std::string local_encryption_bootstrap_token;
  if (HasPref(local_store_, prefs::kSyncEncryptionBootstrapToken)) {
    local_encryption_bootstrap_token =
        GetAndUnpackStringPref(local_store_,
                               prefs::kSyncEncryptionBootstrapToken);
  }
  std::string local_keystore_encryption_bootstrap_token;
  if (HasPref(local_store_, prefs::kSyncKeystoreEncryptionBootstrapToken)) {
    local_keystore_encryption_bootstrap_token =
        GetAndUnpackStringPref(local_store_,
                               prefs::kSyncKeystoreEncryptionBootstrapToken);
  }
  return (local_encryption_bootstrap_token !=
          alternate_encryption_bootstrap_token) ||
         (local_keystore_encryption_bootstrap_token !=
          alternate_keystore_encryption_bootstrap_token);
}

bool CredentialCacheService::HaveTokenServiceCredentialsChanged(
    const std::string& alternate_lsid,
    const std::string& alternate_sid) {
  std::string local_lsid =
      GetAndUnpackStringPref(local_store_, GaiaConstants::kGaiaLsid);
  std::string local_sid =
      GetAndUnpackStringPref(local_store_, GaiaConstants::kGaiaSid);
  return local_lsid != alternate_lsid || local_sid != alternate_sid;
}

bool CredentialCacheService::ShouldSignOutOfSync(
    const std::string& alternate_google_services_username) {
  // We must sign out of sync iff:
  // 1) The user is signed in to the local profile.
  // 2) The user has never signed out of the local profile in the past.
  // 3) We noticed that the user has signed out of the alternate profile.
  // 4) The user is not already in the process of configuring sync.
  // 5) The alternate cache was updated more recently than the local cache.
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  return !service->signin()->GetAuthenticatedUsername().empty() &&
         !HasUserSignedOut() &&
         alternate_google_services_username.empty() &&
         !service->setup_in_progress() &&
         AlternateCacheIsMoreRecent();
}

bool CredentialCacheService::MayReconfigureSync(
    const std::string& alternate_google_services_username) {
  // We may attempt to reconfigure sync iff:
  // 1) The user is signed in to the local profile.
  // 2) The user has never signed out of the local profile in the past.
  // 3) The user is signed in to the alternate profile with the same account.
  // 4) The user is not already in the process of configuring sync.
  // 5) The alternate cache was updated more recently than the local cache.
  // 6) The sync backend is initialized and ready to consume config changes.
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  return !service->signin()->GetAuthenticatedUsername().empty() &&
         !HasUserSignedOut() &&
         (alternate_google_services_username ==
          service->signin()->GetAuthenticatedUsername()) &&
         !service->setup_in_progress() &&
         AlternateCacheIsMoreRecent() &&
         service->ShouldPushChanges();
}

bool CredentialCacheService::ShouldSignInToSync(
    const std::string& alternate_google_services_username,
    const std::string& alternate_lsid,
    const std::string& alternate_sid,
    const std::string& alternate_encryption_bootstrap_token,
    const std::string& alternate_keystore_encryption_bootstrap_token) {
  // We should sign in with cached credentials from the alternate profile iff:
  // 1) The user is not currently signed in to the local profile.
  // 2) The user has never signed out of the local profile in the past.
  // 3) Valid cached credentials are available in the alternate profile.
  // 4) The user is not already in the process of configuring sync.
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  return service->signin()->GetAuthenticatedUsername().empty() &&
         !HasUserSignedOut() &&
         !alternate_google_services_username.empty() &&
         !alternate_lsid.empty() &&
         !alternate_sid.empty() &&
         !(alternate_encryption_bootstrap_token.empty() &&
           alternate_keystore_encryption_bootstrap_token.empty()) &&
         !service->setup_in_progress();
}

}  // namespace syncer
