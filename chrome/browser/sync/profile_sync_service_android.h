// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_ANDROID_H_
#define CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_ANDROID_H_

#include <jni.h>
#include <map>

#include "base/android/jni_weak_ref.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "chrome/browser/sync/profile_sync_service_observer.h"
#include "components/invalidation/invalidation_util.h"
#include "components/sync_driver/sync_prefs.h"
#include "google/cacheinvalidation/include/types.h"
#include "google_apis/gaia/google_service_auth_error.h"

class Profile;
class ProfileSyncService;

// Android wrapper of the ProfileSyncService which provides access from the Java
// layer. Note that on Android, there's only a single profile, and therefore
// a single instance of this wrapper. The name of the Java class is
// ProfileSyncService.
// This class should only be accessed from the UI thread.
class ProfileSyncServiceAndroid : public ProfileSyncServiceObserver {
 public:

  ProfileSyncServiceAndroid(JNIEnv* env, jobject obj);

  // This method should be called once right after contructing the object.
  void Init();

  // Called from Java when we need to nudge native syncer. The |objectSource|,
  // |objectId|, |version| and |payload| values should come from an
  // invalidation.
  void NudgeSyncer(JNIEnv* env,
                   jobject obj,
                   jint objectSource,
                   jstring objectId,
                   jlong version,
                   jstring payload);

  // Called from Java when we need to nudge native syncer but have lost state on
  // which types have changed.
  void NudgeSyncerForAllTypes(JNIEnv* env, jobject obj);

  // Called from Java when the user manually enables sync
  void EnableSync(JNIEnv* env, jobject obj);

  // Called from Java when the user manually disables sync
  void DisableSync(JNIEnv* env, jobject obj);

  // Called from Java when the user signs in to Chrome. Starts up sync.
  void SignInSync(JNIEnv* env, jobject obj);

  // Called from Java when the user signs out of Chrome
  void SignOutSync(JNIEnv* env, jobject obj);

  // Returns a string version of browser_sync::SyncBackendHost::StatusSummary
  base::android::ScopedJavaLocalRef<jstring> QuerySyncStatusSummary(
      JNIEnv* env, jobject obj);

  // Called from Java early during startup to ensure we use the correct
  // unique machine tag in session sync. Returns true if the machine tag was
  // succesfully set.
  // This must be called before the |SessionModelAssociator| is initialized.
  jboolean SetSyncSessionsId(JNIEnv* env, jobject obj, jstring tag);

  // Returns true if the sync backend is initialized.
  jboolean IsSyncInitialized(JNIEnv* env, jobject obj);

  // Returns true if the sync is currently being setup for the first time.
  jboolean IsFirstSetupInProgress(JNIEnv* env, jobject obj);

  // Returns true if the user is currently encrypting everything.
  jboolean IsEncryptEverythingEnabled(JNIEnv* env, jobject obj);

  // Returns true if the sync code needs a passphrase for either encryption or
  // decryption (can need a passphrase for encryption if the user is turning on
  // encryption and no passphrase has been set yet).
  jboolean IsPassphraseRequired(JNIEnv* env, jobject obj);

  // Returns true if the sync code needs a decryption passphrase for one of the
  // currently enabled types.
  jboolean IsPassphraseRequiredForDecryption(JNIEnv* env, jobject obj);

  // Returns true if the sync code needs a decryption passphrase for *any* type,
  // even types that aren't supported on this platform (like passwords).
  jboolean IsPassphraseRequiredForExternalType(JNIEnv* env, jobject obj);

  // Returns true if the sync code needs a custom decryption passphrase.
  // Can not be called if the sync backend is not initialized.
  jboolean IsUsingSecondaryPassphrase(JNIEnv* env, jobject obj);

  // Returns true if the decryption passphrase works (was able to decrypt the
  // stored sync data). Should only be called if
  // IsPassphraseRequiredForDecryption() returns true.
  jboolean SetDecryptionPassphrase(JNIEnv* env,
                                   jobject obj,
                                   jstring passphrase);

  // Encrypts the user's data with the passed passphrase. If |is_gaia| == true
  // then the passphrase is treated as a google (GAIA) passphrase, otherwise
  // it's treated like an explicit/custom passphrase.
  void SetEncryptionPassphrase(JNIEnv* env,
                               jobject obj,
                               jstring passphrase,
                               jboolean is_gaia);

  // Returns whether the cryptographer is ready (i.e. encrypted types can be
  // handled).
  jboolean IsCryptographerReady(JNIEnv* env, jobject);

  // Returns the actual passphrase type being used for encryption. This is a
  // value from the enum defined in syncer::PassphraseType and must be matched
  // in Java.
  jint GetPassphraseType(JNIEnv* env, jobject obj);

  // Returns true if the current explicit passphrase time is defined.
  jboolean HasExplicitPassphraseTime(JNIEnv* env, jobject);

  base::android::ScopedJavaLocalRef<jstring>
      GetSyncEnterGooglePassphraseBodyWithDateText(
          JNIEnv* env, jobject);

  base::android::ScopedJavaLocalRef<jstring>
      GetSyncEnterCustomPassphraseBodyWithDateText(
          JNIEnv* env, jobject);

  base::android::ScopedJavaLocalRef<jstring>
      GetCurrentSignedInAccountText(
          JNIEnv* env, jobject);

  base::android::ScopedJavaLocalRef<jstring>
      GetSyncEnterCustomPassphraseBodyText(
          JNIEnv* env, jobject);

  // Returns true if sync has been migrated.
  jboolean IsSyncKeystoreMigrationDone(JNIEnv* env, jobject obj);

  // Get the set of enabled data types. These are the types currently both
  // registered and preferred. Note that control types are always included here.
  // Returns a bit map of the values from
  // profile_sync_service_model_type_selection_android.h.
  jlong GetEnabledDataTypes(JNIEnv* env, jobject obj);

  // Enables the passed data types.
  // If |sync_everything| is true, then all data types are enabled and the
  // contents of |model_type_selection| is
  // ignored.
  // Otherwise, the values of |model_type_selection| must contain a bit map of
  // values from profile_sync_service_model_type_selection_android.h.
  void SetPreferredDataTypes(JNIEnv* env,
                             jobject obj,
                             jboolean sync_everything,
                             jlong model_type_selection);

  // Tells sync that we're currently configuring so no data types should be
  // downloaded yet.
  void SetSetupInProgress(JNIEnv* env, jobject obj, jboolean in_progress);

  // Tells sync that sync setup is complete so we can start syncing now.
  void SetSyncSetupCompleted(JNIEnv* env, jobject obj);

  // Returns true if sync setup has been completed.
  jboolean HasSyncSetupCompleted(JNIEnv* env, jobject obj);

  // Returns true if sync startup is currently suppressed.
  jboolean IsStartSuppressed(JNIEnv* env, jobject obj);

  // Returns true if sync is configured to "sync everything".
  jboolean HasKeepEverythingSynced(JNIEnv* env, jobject obj);

  // Turns on encryption for all data types. This is an asynchronous operation
  // which happens after the current configuration pass is done, so a call to
  // this routine must be followed by a call to SetEnabledDataTypes().
  void EnableEncryptEverything(JNIEnv* env, jobject obj);

  // Returns true if sync has encountered an unrecoverable error.
  jboolean HasUnrecoverableError(JNIEnv* env, jobject obj);

  // Returns sync internals in a JSON-formatted Java string.
  base::android::ScopedJavaLocalRef<jstring> GetAboutInfoForTest(JNIEnv* env,
                                                                 jobject obj);

  // Returns the integer value corresponding to the current auth error state
  // (GoogleServiceAuthError.State).
  jint GetAuthError(JNIEnv* env, jobject obj);

  // ProfileSyncServiceObserver:
  virtual void OnStateChanged() OVERRIDE;

  // Returns a timestamp for when a sync was last executed. The return value is
  // the internal value of base::Time.
  jlong GetLastSyncedTimeForTest(JNIEnv* env, jobject obj);

  static ProfileSyncServiceAndroid* GetProfileSyncServiceAndroid();

  // Registers the ProfileSyncServiceAndroid's native methods through JNI.
  static bool Register(JNIEnv* env);

 private:
  typedef std::map<invalidation::ObjectId,
                   int64,
                   syncer::ObjectIdLessThan> ObjectIdVersionMap;

  virtual ~ProfileSyncServiceAndroid();
  // Remove observers to profile sync service.
  void RemoveObserver();
  // Called from Java when we need to nudge native syncer. The |object_source|,
  // |objectId|, |version| and |payload| values should come from an
  // invalidation.
  void SendNudgeNotification(int object_source,
                             const std::string& str_object_id,
                             int64 version,
                             const std::string& payload);

  Profile* profile_;
  ProfileSyncService* sync_service_;
  // The class that handles getting, setting, and persisting sync
  // preferences.
  scoped_ptr<sync_driver::SyncPrefs> sync_prefs_;

  // Java-side ProfileSyncService object.
  JavaObjectWeakGlobalRef weak_java_profile_sync_service_;

  // The invalidation API spec allows for the possibility of redundant
  // invalidations, so keep track of the max versions and drop
  // invalidations with old versions.
  ObjectIdVersionMap max_invalidation_versions_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncServiceAndroid);
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_ANDROID_H_
