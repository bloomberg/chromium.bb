// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/credential_cache_service_win.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/base64.h"
#include "base/compiler_specific.h"
#include "base/file_util.h"
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
  // When the alternate credential store becomes available, begin consuming its
  // cached credentials.
  if (alternate_store_.get() && alternate_store_->IsInitializationComplete()) {
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
  // 3) No user is currently signed in to sync.
  DCHECK(profile_);
  PrefService* prefs = profile_->GetPrefs();
  DCHECK(prefs);
  return !sync_prefs_.IsManaged() &&
         !sync_prefs_.IsStartSuppressed() &&
         prefs->GetString(prefs::kGoogleServicesUsername).empty();
}

void CredentialCacheService::InitializeLocalCredentialCacheWriter() {
  local_store_ = new JsonPrefStore(
      GetCredentialPathInCurrentProfile(),
      content::BrowserThread::GetMessageLoopProxyForThread(
          content::BrowserThread::FILE));
  local_store_->ReadPrefsAsync(NULL);

  // Register for notifications for updates to the sync credentials, which are
  // stored in the PrefStore.
  pref_registrar_.Init(profile_->GetPrefs());
  pref_registrar_.Add(prefs::kSyncEncryptionBootstrapToken, this);
  pref_registrar_.Add(prefs::kGoogleServicesUsername, this);
  pref_registrar_.Add(prefs::kSyncKeepEverythingSynced, this);
  for (int i = FIRST_REAL_MODEL_TYPE; i < MODEL_TYPE_COUNT; ++i) {
    if (i == NIGORI)  // The NIGORI preference is not persisted.
      continue;
    pref_registrar_.Add(
        browser_sync::SyncPrefs::GetPrefNameForDataType(ModelTypeFromInt(i)),
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
  // alternate profile directory, and there is nothing to do.
  // TODO(rsimha): Add a polling mechanism that periodically examines the
  // credential file in the alternate profile directory so we can respond to the
  // user signing in and signing out.
  DCHECK(should_initialize);
  if (!*should_initialize)
    return;
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

// Determines if credentials should be read from the alternate profile based
// on the existence of the local and alternate credential files. Returns
// true via |result| if there is a credential cache file in the alternate
// profile, but there isn't one in the local profile. Returns false otherwise.
void ShouldReadFromAlternateCache(
    const FilePath& credential_path_in_current_profile,
    const FilePath& credential_path_in_alternate_profile,
    bool* result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::FILE));
  DCHECK(result);
  *result = !file_util::PathExists(credential_path_in_current_profile) &&
            file_util::PathExists(credential_path_in_alternate_profile);
}

}  // namespace

void CredentialCacheService::LookForCachedCredentialsInAlternateProfile() {
  bool* should_initialize = new bool(false);
  content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&ShouldReadFromAlternateCache,
                 GetCredentialPathInCurrentProfile(),
                 GetCredentialPathInAlternateProfile(),
                 should_initialize),
      base::Bind(
          &CredentialCacheService::InitializeAlternateCredentialCacheReader,
          weak_factory_.GetWeakPtr(),
          base::Owned(should_initialize)));
}

void CredentialCacheService::ReadCachedCredentialsFromAlternateProfile() {
  DCHECK(alternate_store_.get());
  if (!HasPref(alternate_store_, prefs::kGoogleServicesUsername) ||
      !HasPref(alternate_store_, GaiaConstants::kGaiaLsid) ||
      !HasPref(alternate_store_, GaiaConstants::kGaiaSid) ||
      !HasPref(alternate_store_, prefs::kSyncEncryptionBootstrapToken) ||
      !HasPref(alternate_store_, prefs::kSyncKeepEverythingSynced)) {
    VLOG(1) << "Could not find cached credentials.";
    return;
  }

  std::string google_services_username =
      GetAndUnpackStringPref(alternate_store_, prefs::kGoogleServicesUsername);
  std::string lsid =
      GetAndUnpackStringPref(alternate_store_, GaiaConstants::kGaiaLsid);
  std::string sid =
      GetAndUnpackStringPref(alternate_store_, GaiaConstants::kGaiaSid);
  std::string encryption_bootstrap_token =
      GetAndUnpackStringPref(alternate_store_,
                             prefs::kSyncEncryptionBootstrapToken);
  bool keep_everything_synced =
      GetBooleanPref(alternate_store_, prefs::kSyncKeepEverythingSynced);

  if (google_services_username.empty() ||
      lsid.empty() ||
      sid.empty() ||
      encryption_bootstrap_token.empty()) {
    VLOG(1) << "Found empty cached credentials.";
    return;
  }

  bool datatype_prefs[MODEL_TYPE_COUNT] = { false };
  for (int i = FIRST_REAL_MODEL_TYPE; i < MODEL_TYPE_COUNT; ++i) {
    if (i == NIGORI)  // The NIGORI preference is not persisted.
      continue;
    std::string datatype_pref_name =
        browser_sync::SyncPrefs::GetPrefNameForDataType(ModelTypeFromInt(i));
    if (!HasPref(alternate_store_, datatype_pref_name)) {
      VLOG(1) << "Could not find cached datatype prefs.";
      return;
    }
    datatype_prefs[i] = GetBooleanPref(alternate_store_, datatype_pref_name);
  }

  ApplyCachedCredentials(google_services_username,
                         lsid,
                         sid,
                         encryption_bootstrap_token,
                         keep_everything_synced,
                         datatype_prefs);
}

void CredentialCacheService::ApplyCachedCredentials(
    const std::string& google_services_username,
    const std::string& lsid,
    const std::string& sid,
    const std::string& encryption_bootstrap_token,
    bool keep_everything_synced,
    const bool datatype_prefs[]) {
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
  syncer::ModelTypeSet registered_types;
  syncer::ModelTypeSet preferred_types;
  for (int i = FIRST_REAL_MODEL_TYPE; i < MODEL_TYPE_COUNT; ++i) {
    if (i == NIGORI)  // The NIGORI preference is not persisted.
      continue;
    registered_types.Put(ModelTypeFromInt(i));
    if (datatype_prefs[i])
      preferred_types.Put(ModelTypeFromInt(i));
  }
  sync_prefs_.SetPreferredDataTypes(registered_types, preferred_types);

  // Update the lsid and sid in the TokenService and mint new tokens for all
  // Chrome services.
  GaiaAuthConsumer::ClientLoginResult login_result;
  login_result.lsid = lsid;
  login_result.sid = sid;
  TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
  token_service->UpdateCredentials(login_result);
  DCHECK(token_service->AreCredentialsValid());
  token_service->StartFetchingTokens();
}

}  // namespace syncer
