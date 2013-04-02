// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_service_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/oauth2_token_service.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/sync/about_sync_util.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_prefs.h"
#include "chrome/browser/sync/sync_ui_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "google/cacheinvalidation/types.pb.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "grit/generated_resources.h"
#include "jni/ProfileSyncService_jni.h"
#include "sync/internal_api/public/base/model_type_invalidation_map.h"
#include "sync/internal_api/public/read_transaction.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;

namespace {
const char kSyncDisabledStatus[] = "OFFLINE_DISABLED";

enum {
#define DEFINE_MODEL_TYPE_SELECTION(name,value)  name = value,
#include "chrome/browser/sync/profile_sync_service_model_type_selection_android.h"
#undef DEFINE_MODEL_TYPE_SELECTION
};

}  // namespace

ProfileSyncServiceAndroid::ProfileSyncServiceAndroid(JNIEnv* env, jobject obj)
    : profile_(NULL),
      sync_service_(NULL),
      weak_java_profile_sync_service_(env, obj) {
  if (g_browser_process == NULL ||
      g_browser_process->profile_manager() == NULL) {
    NOTREACHED() << "Browser process or profile manager not initialized";
    return;
  }

  profile_ = g_browser_process->profile_manager()->GetDefaultProfile();
  if (profile_ == NULL) {
    NOTREACHED() << "Sync Init: Profile not found.";
    return;
  }

  sync_service_ =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile_);
  DCHECK(sync_service_);
}

void ProfileSyncServiceAndroid::Init() {
  sync_service_->AddObserver(this);

  std::string signed_in_username =
      SigninManagerFactory::GetForProfile(profile_)->GetAuthenticatedUsername();
  if (!signed_in_username.empty()) {
    // If the user is logged in, see if he has a valid token - if not, fetch
    // a new one.
    TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
    if (!token_service->HasTokenForService(GaiaConstants::kSyncService) ||
        (sync_service_->GetAuthError().state() ==
         GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS)) {
      DVLOG(2) << "Trying to update token for user " << signed_in_username;
      InvalidateAuthToken();
    }
  }
}

void ProfileSyncServiceAndroid::RemoveObserver() {
  if (sync_service_->HasObserver(this)) {
    sync_service_->RemoveObserver(this);
  }
}

ProfileSyncServiceAndroid::~ProfileSyncServiceAndroid() {
  RemoveObserver();
}

void ProfileSyncServiceAndroid::SendNudgeNotification(
    const std::string& str_object_id,
    int64 version,
    const std::string& state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // TODO(nileshagrawal): Merge this with ChromeInvalidationClient::Invalidate.
  // Construct the ModelTypeStateMap and send it over with the notification.
  syncer::ModelType model_type;
  if (!syncer::NotificationTypeToRealModelType(str_object_id, &model_type)) {
    DVLOG(1) << "Could not get invalidation model type; "
            << "Sending notification with empty state map.";
    syncer::ModelTypeInvalidationMap model_types_with_states;
    content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_SYNC_REFRESH_REMOTE,
          content::Source<Profile>(profile_),
          content::Details<const syncer::ModelTypeInvalidationMap>(
              &model_types_with_states));
    return;
  }

  if (version != ipc::invalidation::Constants::UNKNOWN) {
    std::map<syncer::ModelType, int64>::const_iterator it =
        max_invalidation_versions_.find(model_type);
    if ((it != max_invalidation_versions_.end()) &&
        (version <= it->second)) {
      DVLOG(1) << "Dropping redundant invalidation with version " << version;
      return;
    }
    max_invalidation_versions_[model_type] = version;
  }

  syncer::ModelTypeSet types;
  types.Put(model_type);
  syncer::ModelTypeInvalidationMap model_types_with_states =
      syncer::ModelTypeSetToInvalidationMap(types, state);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_SYNC_REFRESH_REMOTE,
      content::Source<Profile>(profile_),
      content::Details<const syncer::ModelTypeInvalidationMap>(
          &model_types_with_states));
}


void ProfileSyncServiceAndroid::OnStateChanged() {
  // Check for auth errors.
  const GoogleServiceAuthError& auth_error = sync_service_->GetAuthError();
  if (auth_error.state() == GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS) {
      DVLOG(2) << "Updating auth token.";
      InvalidateAuthToken();
  }

  // Notify the java world that our sync state has changed.
  JNIEnv* env = AttachCurrentThread();
  Java_ProfileSyncService_syncStateChanged(
      env, weak_java_profile_sync_service_.get(env).obj());
}

void ProfileSyncServiceAndroid::TokenAvailable(
    JNIEnv* env, jobject, jstring username, jstring auth_token) {
  std::string token = ConvertJavaStringToUTF8(env, auth_token);
  TokenServiceFactory::GetForProfile(profile_)->OnIssueAuthTokenSuccess(
      GaiaConstants::kSyncService, token);
}

void ProfileSyncServiceAndroid::InvalidateOAuth2Token(
    const std::string& scope, const std::string& invalid_token) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_scope =
      ConvertUTF8ToJavaString(env, scope);
  ScopedJavaLocalRef<jstring> j_invalid_token =
      ConvertUTF8ToJavaString(env, invalid_token);
  Java_ProfileSyncService_invalidateOAuth2AuthToken(
      env, weak_java_profile_sync_service_.get(env).obj(),
      j_scope.obj(),
      j_invalid_token.obj());
}

void ProfileSyncServiceAndroid::FetchOAuth2Token(
    const std::string& scope, const FetchOAuth2TokenCallback& callback) {
  const std::string& sync_username =
      SigninManagerFactory::GetForProfile(profile_)->GetAuthenticatedUsername();

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_sync_username =
      ConvertUTF8ToJavaString(env, sync_username);
  ScopedJavaLocalRef<jstring> j_scope =
      ConvertUTF8ToJavaString(env, scope);

  // Allocate a copy of the callback on the heap, because the callback
  // needs to be passed through JNI as an int.
  // It will be passed back to OAuth2TokenFetched(), where it will be freed.
  scoped_ptr<FetchOAuth2TokenCallback> heap_callback(
      new FetchOAuth2TokenCallback(callback));

  // Call into Java to get a new token.
  Java_ProfileSyncService_getOAuth2AuthToken(
      env, weak_java_profile_sync_service_.get(env).obj(),
      j_sync_username.obj(),
      j_scope.obj(),
      reinterpret_cast<int>(heap_callback.release()));
}

void ProfileSyncServiceAndroid::OAuth2TokenFetched(
    JNIEnv* env, jobject, int callback, jstring auth_token, jboolean result) {
  std::string token = ConvertJavaStringToUTF8(env, auth_token);
  scoped_ptr<FetchOAuth2TokenCallback> heap_callback(
      reinterpret_cast<FetchOAuth2TokenCallback*>(callback));
  GoogleServiceAuthError err(result ?
                             GoogleServiceAuthError::NONE :
                             GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  heap_callback->Run(err, token, base::Time());
}

void ProfileSyncServiceAndroid::EnableSync(JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Don't need to do anything if we're already enabled.
  browser_sync::SyncPrefs prefs(profile_->GetPrefs());
  if (prefs.IsStartSuppressed())
    sync_service_->UnsuppressAndStart();
  else
    DVLOG(2) << "Ignoring call to EnableSync() because sync is already enabled";
}

void ProfileSyncServiceAndroid::DisableSync(JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  sync_service_->StopAndSuppress();
}

void ProfileSyncServiceAndroid::SignInSync(
    JNIEnv* env, jobject, jstring username, jstring auth_token) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Just return if sync already has everything it needs to start up (sync
  // should start up automatically as long as it has credentials). This can
  // happen normally if (for example) the user closes and reopens the sync
  // settings window quickly during initial startup.
  if (sync_service_->IsSyncEnabledAndLoggedIn() &&
      sync_service_->IsSyncTokenAvailable() &&
      sync_service_->HasSyncSetupCompleted()) {
    return;
  }

  if (!sync_service_->IsSyncEnabledAndLoggedIn() ||
      !sync_service_->IsSyncTokenAvailable()) {
    // Set the currently-signed-in username, fetch an auth token if necessary,
    // and enable sync.
    std::string name = ConvertJavaStringToUTF8(env, username);
    // TODO(tim) It should be enough to only call
    // SigninManager::SetAuthenticatedUsername here. See
    // http://crbug.com/107160.
    profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername, name);
    SigninManagerFactory::GetForProfile(profile_)->
        SetAuthenticatedUsername(name);
    std::string token = ConvertJavaStringToUTF8(env, auth_token);
    if (token.empty()) {
      // No credentials passed in - request an auth token.
      // If fetching the auth token is successful, this will cause
      // ProfileSyncService to start sync when it receives
      // NOTIFICATION_TOKEN_AVAILABLE.
      DVLOG(2) << "Fetching auth token for " << name;
      InvalidateAuthToken();
    } else {
      // OnIssueAuthTokenSuccess will send out a notification to the sync
      // service that will cause the sync backend to initialize.
      TokenService* token_service =
          TokenServiceFactory::GetForProfile(profile_);
      token_service->OnIssueAuthTokenSuccess(GaiaConstants::kSyncService,
                                             token);
    }
  }

  // Enable sync (if we don't have credentials yet, this will enable sync but
  // will not start it up - sync will start once credentials arrive).
  sync_service_->UnsuppressAndStart();
}

void ProfileSyncServiceAndroid::SignOutSync(JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile_);
  sync_service_->DisableForUser();

  // Clear the tokens.
  SigninManagerFactory::GetForProfile(profile_)->SignOut();

  // Need to clear suppress start flag manually
  browser_sync::SyncPrefs prefs(profile_->GetPrefs());
  prefs.SetStartSuppressed(false);
}

ScopedJavaLocalRef<jstring> ProfileSyncServiceAndroid::QuerySyncStatusSummary(
    JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile_);
  std::string status(sync_service_->QuerySyncStatusSummary());
  return ConvertUTF8ToJavaString(env, status);
}

jboolean ProfileSyncServiceAndroid::SetSyncSessionsId(
    JNIEnv* env, jobject obj, jstring tag) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(profile_);
  std::string machine_tag = ConvertJavaStringToUTF8(env, tag);
  browser_sync::SyncPrefs prefs(profile_->GetPrefs());
  prefs.SetSyncSessionsGUID(machine_tag);
  return true;
}

jint ProfileSyncServiceAndroid::GetAuthError(JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return sync_service_->GetAuthError().state();
}

jboolean ProfileSyncServiceAndroid::IsEncryptEverythingEnabled(
    JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return sync_service_->EncryptEverythingEnabled();
}

jboolean ProfileSyncServiceAndroid::IsSyncInitialized(JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return sync_service_->sync_initialized();
}

jboolean ProfileSyncServiceAndroid::IsFirstSetupInProgress(
    JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return sync_service_->FirstSetupInProgress();
}

jboolean ProfileSyncServiceAndroid::IsPassphraseRequired(JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return sync_service_->IsPassphraseRequired();
}

jboolean ProfileSyncServiceAndroid::IsPassphraseRequiredForDecryption(
    JNIEnv* env, jobject obj) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // In case of CUSTOM_PASSPHRASE we always sync passwords. Prompt the user for
  // a passphrase if cryptographer has any pending keys.
  if (sync_service_->GetPassphraseType() == syncer::CUSTOM_PASSPHRASE) {
    return !IsCryptographerReady(env, obj);
  }
  if (sync_service_->IsPassphraseRequiredForDecryption()) {
    // Passwords datatype should never prompt for a passphrase, except when
    // user is using a custom passphrase. Do not prompt for a passphrase if
    // passwords are the only encrypted datatype. This prevents a temporary
    // notification for passphrase  when PSS has not completed configuring
    // DataTypeManager, after configuration password datatype shall be disabled.
    const syncer::ModelTypeSet encrypted_types =
        sync_service_->GetEncryptedDataTypes();
    const bool are_passwords_the_only_encrypted_type =
        encrypted_types.Has(syncer::PASSWORDS) && encrypted_types.Size() == 1 &&
        !sync_service_->ShouldEnablePasswordSyncForAndroid();
    return !are_passwords_the_only_encrypted_type;
  }
  return false;
}

jboolean ProfileSyncServiceAndroid::IsPassphraseRequiredForExternalType(
    JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return
      sync_service_->passphrase_required_reason() == syncer::REASON_DECRYPTION;
}

jboolean ProfileSyncServiceAndroid::IsUsingSecondaryPassphrase(
    JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return sync_service_->IsUsingSecondaryPassphrase();
}

jboolean ProfileSyncServiceAndroid::SetDecryptionPassphrase(
    JNIEnv* env, jobject obj, jstring passphrase) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::string key = ConvertJavaStringToUTF8(env, passphrase);
  return sync_service_->SetDecryptionPassphrase(key);
}

void ProfileSyncServiceAndroid::SetEncryptionPassphrase(
    JNIEnv* env, jobject obj, jstring passphrase, jboolean is_gaia) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  std::string key = ConvertJavaStringToUTF8(env, passphrase);
  sync_service_->SetEncryptionPassphrase(
      key,
      is_gaia ? ProfileSyncService::IMPLICIT : ProfileSyncService::EXPLICIT);
}

jboolean ProfileSyncServiceAndroid::IsCryptographerReady(JNIEnv* env, jobject) {
  syncer::ReadTransaction trans(FROM_HERE, sync_service_->GetUserShare());
  return sync_service_->IsCryptographerReady(&trans);
}

jint ProfileSyncServiceAndroid::GetPassphraseType(JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return sync_service_->GetPassphraseType();
}

jboolean ProfileSyncServiceAndroid::HasExplicitPassphraseTime(
    JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::Time passphrase_time = sync_service_->GetExplicitPassphraseTime();
  return !passphrase_time.is_null();
}

ScopedJavaLocalRef<jstring>
    ProfileSyncServiceAndroid::GetSyncEnterGooglePassphraseBodyWithDateText(
        JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::Time passphrase_time = sync_service_->GetExplicitPassphraseTime();
  string16 passphrase_time_str = base::TimeFormatShortDate(passphrase_time);
  return base::android::ConvertUTF16ToJavaString(env,
      l10n_util::GetStringFUTF16(
        IDS_SYNC_ENTER_GOOGLE_PASSPHRASE_BODY_WITH_DATE,
        passphrase_time_str));
}

ScopedJavaLocalRef<jstring>
    ProfileSyncServiceAndroid::GetSyncEnterCustomPassphraseBodyWithDateText(
        JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::Time passphrase_time = sync_service_->GetExplicitPassphraseTime();
  string16 passphrase_time_str = base::TimeFormatShortDate(passphrase_time);
  return base::android::ConvertUTF16ToJavaString(env,
      l10n_util::GetStringFUTF16(IDS_SYNC_ENTER_PASSPHRASE_BODY_WITH_DATE,
        passphrase_time_str));
}

ScopedJavaLocalRef<jstring>
    ProfileSyncServiceAndroid::GetCurrentSignedInAccountText(
        JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  const std::string& sync_username =
      SigninManagerFactory::GetForProfile(profile_)->GetAuthenticatedUsername();
  return base::android::ConvertUTF16ToJavaString(env,
      l10n_util::GetStringFUTF16(
          IDS_SYNC_ACCOUNT_SYNCING_TO_USER,
          ASCIIToUTF16(sync_username)));
}

ScopedJavaLocalRef<jstring>
    ProfileSyncServiceAndroid::GetSyncEnterCustomPassphraseBodyText(
        JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return ConvertUTF8ToJavaString(
      env, l10n_util::GetStringUTF8(IDS_SYNC_ENTER_PASSPHRASE_BODY));
}

jboolean ProfileSyncServiceAndroid::IsSyncKeystoreMigrationDone(
      JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  syncer::SyncStatus status;
  bool is_status_valid = sync_service_->QueryDetailedSyncStatus(&status);
  return is_status_valid && !status.keystore_migration_time.is_null();
}

jlong ProfileSyncServiceAndroid::GetEnabledDataTypes(JNIEnv* env,
                                                     jobject obj) {
  jlong model_type_selection = 0;
  syncer::ModelTypeSet types = sync_service_->GetPreferredDataTypes();
  types.PutAll(syncer::ControlTypes());
  if (types.Has(syncer::BOOKMARKS)) {
    model_type_selection |= BOOKMARK;
  }
  if (types.Has(syncer::AUTOFILL)) {
    model_type_selection |= AUTOFILL;
  }
  if (types.Has(syncer::AUTOFILL_PROFILE)) {
    model_type_selection |= AUTOFILL_PROFILE;
  }
  if (types.Has(syncer::PASSWORDS)) {
    model_type_selection |= PASSWORD;
  }
  if (types.Has(syncer::TYPED_URLS)) {
    model_type_selection |= TYPED_URL;
  }
  if (types.Has(syncer::SESSIONS)) {
    model_type_selection |= SESSION;
  }
  if (types.Has(syncer::HISTORY_DELETE_DIRECTIVES)) {
    model_type_selection |= HISTORY_DELETE_DIRECTIVE;
  }
  if (types.Has(syncer::PROXY_TABS)) {
    model_type_selection |= PROXY_TABS;
  }
  if (types.Has(syncer::FAVICON_IMAGES)) {
    model_type_selection |= FAVICON_IMAGE;
  }
  if (types.Has(syncer::FAVICON_TRACKING)) {
    model_type_selection |= FAVICON_TRACKING;
  }
  if (types.Has(syncer::DEVICE_INFO)) {
    model_type_selection |= DEVICE_INFO;
  }
  if (types.Has(syncer::NIGORI)) {
    model_type_selection |= NIGORI;
  }
  if (types.Has(syncer::EXPERIMENTS)) {
    model_type_selection |= EXPERIMENTS;
  }
  return model_type_selection;
}

void ProfileSyncServiceAndroid::SetPreferredDataTypes(
    JNIEnv* env, jobject obj,
    jboolean sync_everything,
    jlong model_type_selection) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  syncer::ModelTypeSet types;
  // Note: only user selectable types should be included here.
  if (model_type_selection & AUTOFILL)
    types.Put(syncer::AUTOFILL);
  if (model_type_selection & BOOKMARK)
    types.Put(syncer::BOOKMARKS);
  if (model_type_selection & PASSWORD)
    types.Put(syncer::PASSWORDS);
  if (model_type_selection & PROXY_TABS)
    types.Put(syncer::PROXY_TABS);
  if (model_type_selection & TYPED_URL)
    types.Put(syncer::TYPED_URLS);
  DCHECK(syncer::UserSelectableTypes().HasAll(types));
  sync_service_->OnUserChoseDatatypes(sync_everything, types);
}

void ProfileSyncServiceAndroid::SetSetupInProgress(
    JNIEnv* env, jobject obj, jboolean in_progress) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  sync_service_->SetSetupInProgress(in_progress);
}

void ProfileSyncServiceAndroid::SetSyncSetupCompleted(
    JNIEnv* env, jobject obj) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  sync_service_->SetSyncSetupCompleted();
}

jboolean ProfileSyncServiceAndroid::HasSyncSetupCompleted(
    JNIEnv* env, jobject obj) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return sync_service_->HasSyncSetupCompleted();
}

void ProfileSyncServiceAndroid::EnableEncryptEverything(
    JNIEnv* env, jobject obj) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  sync_service_->EnableEncryptEverything();
}

jboolean ProfileSyncServiceAndroid::HasKeepEverythingSynced(
    JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  browser_sync::SyncPrefs prefs(profile_->GetPrefs());
  return prefs.HasKeepEverythingSynced();
}

jboolean ProfileSyncServiceAndroid::HasUnrecoverableError(
    JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return sync_service_->HasUnrecoverableError();
}

ScopedJavaLocalRef<jstring> ProfileSyncServiceAndroid::GetAboutInfoForTest(
    JNIEnv* env, jobject) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<DictionaryValue> about_info =
      sync_ui_util::ConstructAboutInformation(sync_service_);
  std::string about_info_json;
  base::JSONWriter::Write(about_info.get(), &about_info_json);

  return ConvertUTF8ToJavaString(env, about_info_json);
}

void ProfileSyncServiceAndroid::InvalidateAuthToken() {
  // Get the token from token-db. If there's no token yet, this must be the
  // the first time the user is signing in so we don't need to invalidate
  // anything.
  TokenService* token_service = TokenServiceFactory::GetForProfile(profile_);
  std::string invalid_token;
  if (token_service->HasTokenForService(GaiaConstants::kSyncService)) {
    invalid_token = token_service->GetTokenForService(
        GaiaConstants::kSyncService);
  }
  const std::string& sync_username =
      SigninManagerFactory::GetForProfile(profile_)->GetAuthenticatedUsername();
  // Call into java to invalidate the current token and get a new one.
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_sync_username =
      ConvertUTF8ToJavaString(env, sync_username);
  ScopedJavaLocalRef<jstring> j_invalid_token =
      ConvertUTF8ToJavaString(env, invalid_token);
  Java_ProfileSyncService_getNewAuthToken(
      env, weak_java_profile_sync_service_.get(env).obj(),
      j_sync_username.obj(), j_invalid_token.obj());
}

void ProfileSyncServiceAndroid::NudgeSyncer(JNIEnv* env,
                                            jobject obj,
                                            jstring objectId,
                                            jlong version,
                                            jstring state) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  SendNudgeNotification(ConvertJavaStringToUTF8(env, objectId), version,
                        ConvertJavaStringToUTF8(env, state));
}

// static
ProfileSyncServiceAndroid*
    ProfileSyncServiceAndroid::GetProfileSyncServiceAndroid() {
  return reinterpret_cast<ProfileSyncServiceAndroid*>(
          Java_ProfileSyncService_getProfileSyncServiceAndroid(
      AttachCurrentThread(), base::android::GetApplicationContext()));
}

static int Init(JNIEnv* env, jobject obj) {
  ProfileSyncServiceAndroid* profile_sync_service_android =
      new ProfileSyncServiceAndroid(env, obj);
  profile_sync_service_android->Init();
  return reinterpret_cast<jint>(profile_sync_service_android);
}

// static
bool ProfileSyncServiceAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
