// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/credential_cache_service_win.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/base64.h"
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
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/sync/glue/chrome_encryptor.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "sync/internal_api/public/base/model_type.h"

namespace syncer {

// The time delay (in seconds) between two consecutive polls of the alternate
// credential cache. A two minute delay seems like a reasonable amount of time
// in which to propagate changes to signed in state between Metro and Desktop.
const int kCredentialCachePollIntervalSecs = 2 * 60;

// Keeps track of the last time a credential cache was written to. Used to make
// sure that we only apply changes from newer credential caches to older ones,
// and not vice versa.
const char kLastUpdatedTime[] = "last_updated_time";

using base::TimeDelta;
using content::BrowserThread;

CredentialCacheService::CredentialCacheService(Profile* profile)
    : profile_(profile),
      // |profile_| is null in unit tests.
      sync_prefs_(profile_ ? profile_->GetPrefs() : NULL),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  if (profile_) {
    InitializeLocalCredentialCacheWriter();
    if (ShouldLookForCachedCredentialsInAlternateProfile())
      LookForCachedCredentialsInAlternateProfile();
  }
}

CredentialCacheService::~CredentialCacheService() {
  Shutdown();
}

void CredentialCacheService::Shutdown() {
  if (local_store_.get()) {
    local_store_.release();
  }

  if (alternate_store_.get()) {
    alternate_store_->RemoveObserver(this);
    alternate_store_.release();
  }
}

void CredentialCacheService::OnInitializationCompleted(bool succeeded) {
  DCHECK(succeeded);
  // When the local and alternate credential stores become available, begin
  // consuming the alternate cached credentials. We must also wait for the local
  // credential store because the credentials read from the alternate cache and
  // applied locally must eventually get stored in the local cache.
  if (alternate_store_.get() &&
      alternate_store_->IsInitializationComplete() &&
      local_store_.get() &&
      local_store_->IsInitializationComplete()) {
    ReadCachedCredentialsFromAlternateProfile();
  }
}

void CredentialCacheService::OnPrefValueChanged(const std::string& key) {
  // Nothing to do here, since credentials are cached silently.
}

void CredentialCacheService::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(local_store_.get());
  switch (type) {
    case chrome::NOTIFICATION_PREF_CHANGED: {
      const std::string pref_name =
          *(content::Details<const std::string>(details).ptr());
      if (pref_name == prefs::kGoogleServicesUsername ||
          pref_name == prefs::kSyncEncryptionBootstrapToken) {
        PackAndUpdateStringPref(
            pref_name,
            profile_->GetPrefs()->GetString(pref_name.c_str()));
      } else {
        UpdateBooleanPref(pref_name,
                          profile_->GetPrefs()->GetBoolean(pref_name.c_str()));
      }
      break;
    }

    case chrome::NOTIFICATION_TOKEN_SERVICE_CREDENTIALS_UPDATED: {
      const TokenService::CredentialsUpdatedDetails& token_details =
          *(content::Details<const TokenService::CredentialsUpdatedDetails>(
              details).ptr());
      PackAndUpdateStringPref(GaiaConstants::kGaiaLsid, token_details.lsid());
      PackAndUpdateStringPref(GaiaConstants::kGaiaSid, token_details.sid());
      break;
    }

    case chrome::NOTIFICATION_TOKENS_CLEARED: {
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

void CredentialCacheService::WriteLastUpdatedTime() {
  DCHECK(local_store_.get());
  int64 last_updated_time = base::TimeTicks::Now().ToInternalValue();
  std::string last_updated_time_string = base::Int64ToString(last_updated_time);
  local_store_->SetValueSilently(
      kLastUpdatedTime,
      base::Value::CreateStringValue(last_updated_time_string));
}

void CredentialCacheService::PackAndUpdateStringPref(
      const std::string& pref_name,
      const std::string& new_value) {
  DCHECK(local_store_.get());
  if (!HasUserSignedOut()) {
    local_store_->SetValueSilently(pref_name, PackCredential(new_value));
  } else {
    // Write a blank value since we cache credentials only for first-time
    // sign-ins.
    local_store_->SetValueSilently(pref_name, PackCredential(std::string()));
  }
  WriteLastUpdatedTime();
}

void CredentialCacheService::UpdateBooleanPref(const std::string& pref_name,
                                               bool new_value) {
  DCHECK(local_store_.get());
  if (!HasUserSignedOut()) {
    local_store_->SetValueSilently(pref_name,
                                   base::Value::CreateBooleanValue(new_value));
  } else {
    // Write a default value of false since we cache credentials only for
    // first-time sign-ins.
    local_store_->SetValueSilently(pref_name,
                                   base::Value::CreateBooleanValue(false));
  }
  WriteLastUpdatedTime();
}

int64 CredentialCacheService::GetLastUpdatedTime(
    scoped_refptr<JsonPrefStore> store) {
  const base::Value* last_updated_time_value = NULL;
  store->GetValue(kLastUpdatedTime, &last_updated_time_value);
  std::string last_updated_time_string;
  last_updated_time_value->GetAsString(&last_updated_time_string);
  int64 last_updated_time;
  bool success = base::StringToInt64(last_updated_time_string,
                                     &last_updated_time);
  DCHECK(success);
  return last_updated_time;
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
  FilePath alternate_default_profile_dir =
      ProfileManager::GetDefaultProfileDir(alternate_user_data_dir);
  return alternate_default_profile_dir.Append(chrome::kSyncCredentialsFilename);
}

bool CredentialCacheService::ShouldLookForCachedCredentialsInAlternateProfile()
    const {
  // We must look for credentials in the alternate profile iff the following are
  // true:
  // 1) Sync is not disabled by policy.
  // 2) Sync startup is not suppressed.
  // Note that we do want to look for credentials in the alternate profile even
  // if the local user is signed in, so we can detect a sign out originating
  // from the alternate profile.
  return !sync_prefs_.IsManaged() && !sync_prefs_.IsStartSuppressed();
}

void CredentialCacheService::InitializeLocalCredentialCacheWriter() {
  local_store_ = new JsonPrefStore(
      GetCredentialPathInCurrentProfile(),
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::FILE));
  local_store_->AddObserver(this);
  local_store_->ReadPrefsAsync(NULL);

  // Register for notifications for updates to the sync credentials, which are
  // stored in the PrefStore.
  pref_registrar_.Init(profile_->GetPrefs());
  pref_registrar_.Add(prefs::kSyncEncryptionBootstrapToken, this);
  pref_registrar_.Add(prefs::kGoogleServicesUsername, this);
  pref_registrar_.Add(prefs::kSyncKeepEverythingSynced, this);
  ModelTypeSet all_types = syncer::ModelTypeSet::All();
  for (ModelTypeSet::Iterator it = all_types.First(); it.Good(); it.Inc()) {
    if (it.Get() == NIGORI)  // The NIGORI preference is not persisted.
      continue;
    pref_registrar_.Add(
        browser_sync::SyncPrefs::GetPrefNameForDataType(it.Get()),
        this);
  }

  // Register for notifications for updates to lsid and sid, which are stored in
  // the TokenService.
  TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKEN_SERVICE_CREDENTIALS_UPDATED,
                 content::Source<TokenService>(token_service));
  registrar_.Add(this,
                 chrome::NOTIFICATION_TOKENS_CLEARED,
                 content::Source<TokenService>(token_service));
}

void CredentialCacheService::InitializeAlternateCredentialCacheReader(
    bool* should_initialize) {
  // If |should_initialize| is false, there was no credential cache in the
  // alternate profile directory, and there is nothing to do right now. Schedule
  // another read in the future and exit.
  DCHECK(should_initialize);
  if (!*should_initialize) {
    ScheduleNextReadFromAlternateCredentialCache();
    return;
  }

  // A credential cache file was found in the alternate profile. Prepare to
  // consume its contents.
  alternate_store_ = new JsonPrefStore(
      GetCredentialPathInAlternateProfile(),
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::FILE));
  alternate_store_->AddObserver(this);
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

namespace {

// Determines if there is a sync credential cache in the alternate profile.
// Returns true via |result| if there is a credential cache file in the
// alternate profile. Returns false otherwise.
void AlternateCredentialCacheExists(
    const FilePath& credential_path_in_alternate_profile,
    bool* result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  DCHECK(result);
  *result = file_util::PathExists(credential_path_in_alternate_profile);
}

}  // namespace

void CredentialCacheService::LookForCachedCredentialsInAlternateProfile() {
  bool* should_initialize = new bool(false);
  content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&AlternateCredentialCacheExists,
                 GetCredentialPathInAlternateProfile(),
                 should_initialize),
      base::Bind(
          &CredentialCacheService::InitializeAlternateCredentialCacheReader,
          weak_factory_.GetWeakPtr(),
          base::Owned(should_initialize)));
}

void CredentialCacheService::ReadCachedCredentialsFromAlternateProfile() {
  // If the local user has signed in and signed out, we do not consume cached
  // credentials from the alternate profile. There is nothing more to do, now or
  // later on.
  if (HasUserSignedOut())
    return;

  // Sanity check the alternate credential cache. If any string credentials
  // are outright missing even though the file exists, something is awry with
  // the alternate profile store. There is no sense in flagging an error as the
  // problem lies in a different profile directory. There is nothing to do now.
  // We schedule a future read from the alternate credential cache and return.
  DCHECK(alternate_store_.get());
  if (!HasPref(alternate_store_, prefs::kGoogleServicesUsername) ||
      !HasPref(alternate_store_, GaiaConstants::kGaiaLsid) ||
      !HasPref(alternate_store_, GaiaConstants::kGaiaSid) ||
      !HasPref(alternate_store_, prefs::kSyncEncryptionBootstrapToken) ||
      !HasPref(alternate_store_, prefs::kSyncKeepEverythingSynced)) {
    VLOG(1) << "Could not find cached credentials in \""
            << GetCredentialPathInAlternateProfile().value() << "\".";
    ScheduleNextReadFromAlternateCredentialCache();
    return;
  }

  // Extract cached credentials from the alternate credential cache.
  std::string google_services_username =
      GetAndUnpackStringPref(alternate_store_, prefs::kGoogleServicesUsername);
  std::string lsid =
      GetAndUnpackStringPref(alternate_store_, GaiaConstants::kGaiaLsid);
  std::string sid =
      GetAndUnpackStringPref(alternate_store_, GaiaConstants::kGaiaSid);
  std::string encryption_bootstrap_token =
      GetAndUnpackStringPref(alternate_store_,
                             prefs::kSyncEncryptionBootstrapToken);

  // Sign out of sync if the alternate profile has signed out the same user.
  // There is no need to schedule any more reads of the alternate profile
  // cache because we only apply cached credentials for first-time sign-ins.
  if (ShouldSignOutOfSync(google_services_username)) {
    VLOG(1) << "User has signed out on the other profile. Signing out.";
    InitiateSignOut();
    return;
  }

  // Extract cached sync prefs from the alternate credential cache.
  bool keep_everything_synced =
      GetBooleanPref(alternate_store_, prefs::kSyncKeepEverythingSynced);
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  ModelTypeSet registered_types = service->GetRegisteredDataTypes();
  ModelTypeSet preferred_types;
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
      preferred_types.Put(it.Get());
  }

  // Reconfigure if sync settings or credentials have changed in the alternate
  // profile, but for the same user that is signed in to the local profile.
  if (MayReconfigureSync(google_services_username)) {
    if (HaveSyncPrefsChanged(keep_everything_synced, preferred_types)) {
      VLOG(1) << "Sync prefs have changed in other profile. Reconfiguring.";
      service->OnUserChoseDatatypes(keep_everything_synced, preferred_types);
    }
    if (HaveTokenServiceCredentialsChanged(lsid, sid)) {
      VLOG(1) << "Token service credentials have changed in other profile.";
      UpdateTokenServiceCredentials(lsid, sid);
    }
  }

  // Sign in if we notice new cached credentials in the alternate profile.
  if (ShouldSignInToSync(google_services_username,
                         lsid,
                         sid,
                         encryption_bootstrap_token)) {
    InitiateSignInWithCachedCredentials(google_services_username,
                                        encryption_bootstrap_token,
                                        keep_everything_synced,
                                        preferred_types);
    UpdateTokenServiceCredentials(lsid, sid);
  }

  // Schedule the next read from the alternate credential cache so that we can
  // detect future reconfigures or sign outs.
  ScheduleNextReadFromAlternateCredentialCache();
}

void CredentialCacheService::InitiateSignInWithCachedCredentials(
    const std::string& google_services_username,
    const std::string& encryption_bootstrap_token,
    bool keep_everything_synced,
    ModelTypeSet preferred_types) {
  // Update the google username in the SigninManager and PrefStore.
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  service->signin()->SetAuthenticatedUsername(google_services_username);
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername,
                                  google_services_username);

  // Update the sync preferences.
  sync_prefs_.SetStartSuppressed(false);
  sync_prefs_.SetSyncSetupCompleted();
  sync_prefs_.SetEncryptionBootstrapToken(encryption_bootstrap_token);
  sync_prefs_.SetKeepEverythingSynced(keep_everything_synced);
  sync_prefs_.SetPreferredDataTypes(service->GetRegisteredDataTypes(),
                                    preferred_types);
}

void CredentialCacheService::UpdateTokenServiceCredentials(
    const std::string& lsid,
    const std::string& sid) {
  GaiaAuthConsumer::ClientLoginResult login_result;
  login_result.lsid = lsid;
  login_result.sid = sid;
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
    bool keep_everything_synced,
    ModelTypeSet preferred_types) const {
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  ModelTypeSet local_preferred_types =
      sync_prefs_.GetPreferredDataTypes(service->GetRegisteredDataTypes());
  return
      (keep_everything_synced != sync_prefs_.HasKeepEverythingSynced()) ||
      !Difference(preferred_types, local_preferred_types).Empty();
}

bool CredentialCacheService::HaveTokenServiceCredentialsChanged(
    const std::string& lsid,
    const std::string& sid) {
  std::string local_lsid =
      GetAndUnpackStringPref(local_store_, GaiaConstants::kGaiaLsid);
  std::string local_sid =
      GetAndUnpackStringPref(local_store_, GaiaConstants::kGaiaSid);
  return local_lsid != lsid || local_sid != sid;
}

bool CredentialCacheService::ShouldSignOutOfSync(
    const std::string& google_services_username) {
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
         google_services_username.empty() &&
         !service->setup_in_progress() &&
         (GetLastUpdatedTime(alternate_store_) >
          GetLastUpdatedTime(local_store_));
}

bool CredentialCacheService::MayReconfigureSync(
    const std::string& google_services_username) {
  // We may attempt to reconfigure sync iff:
  // 1) The user is signed in to the local profile.
  // 2) The user has never signed out of the local profile in the past.
  // 3) The user is signed in to the alternate profile with the same account.
  // 4) The user is not already in the process of configuring sync.
  // 5) The alternate cache was updated more recently than the local cache.
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  return !service->signin()->GetAuthenticatedUsername().empty() &&
         !HasUserSignedOut() &&
         (google_services_username ==
          service->signin()->GetAuthenticatedUsername()) &&
         !service->setup_in_progress() &&
         (GetLastUpdatedTime(alternate_store_) >
          GetLastUpdatedTime(local_store_));
}

bool CredentialCacheService::ShouldSignInToSync(
    const std::string& google_services_username,
    const std::string& lsid,
    const std::string& sid,
    const std::string& encryption_bootstrap_token) {
  // We should sign in with cached credentials from the alternate profile iff:
  // 1) The user is not currently signed in to the local profile.
  // 2) The user has never signed out of the local profile in the past.
  // 3) Valid cached credentials are available in the alternate profile.
  // 4) The user is not already in the process of configuring sync.
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetForProfile(profile_);
  return service->signin()->GetAuthenticatedUsername().empty() &&
         !HasUserSignedOut() &&
         !google_services_username.empty() &&
         !lsid.empty() &&
         !sid.empty() &&
         !encryption_bootstrap_token.empty() &&
         !service->setup_in_progress();
}

void CredentialCacheService::ScheduleNextReadFromAlternateCredentialCache() {
  // We must reinitialize |alternate_store_| here because the underlying
  // credential file in the alternate profile might have changed, and we must
  // re-read it afresh.
  if (alternate_store_.get()) {
    alternate_store_->RemoveObserver(this);
    alternate_store_.release();
  }
  next_read_.Reset(base::Bind(
      &CredentialCacheService::LookForCachedCredentialsInAlternateProfile,
      weak_factory_.GetWeakPtr()));
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      next_read_.callback(),
      TimeDelta::FromSeconds(kCredentialCachePollIntervalSecs));
}

}  // namespace syncer
