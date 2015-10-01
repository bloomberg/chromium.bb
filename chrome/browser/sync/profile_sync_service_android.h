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
#include "components/invalidation/public/invalidation_util.h"
#include "components/sync_driver/sync_prefs.h"
#include "components/sync_driver/sync_service_observer.h"
#include "google/cacheinvalidation/include/types.h"
#include "google_apis/gaia/google_service_auth_error.h"

class Profile;
class ProfileSyncService;

// Android wrapper of the ProfileSyncService which provides access from the Java
// layer. Note that on Android, there's only a single profile, and therefore
// a single instance of this wrapper. The name of the Java class is
// ProfileSyncService.
// This class should only be accessed from the UI thread.
class ProfileSyncServiceAndroid : public sync_driver::SyncServiceObserver {
 public:

  ProfileSyncServiceAndroid(JNIEnv* env, jobject obj);

  // This method should be called once right after contructing the object.
  void Init();

  // sync_driver::SyncServiceObserver:
  void OnStateChanged() override;

  // Pure ProfileSyncService calls.
  jboolean IsSyncRequested(JNIEnv* env, jobject obj);
  void RequestStart(JNIEnv* env, jobject obj);
  void RequestStop(JNIEnv* env, jobject obj);
  void SignOutSync(JNIEnv* env, jobject obj);
  jboolean IsSyncActive(JNIEnv* env, jobject obj);
  jboolean IsBackendInitialized(JNIEnv* env, jobject obj);
  jboolean IsFirstSetupInProgress(JNIEnv* env, jobject obj);
  void SetSetupInProgress(JNIEnv* env, jobject obj, jboolean in_progress);
  jboolean HasSyncSetupCompleted(JNIEnv* env, jobject obj);
  void SetSyncSetupCompleted(JNIEnv* env, jobject obj);
  base::android::ScopedJavaLocalRef<jintArray> GetActiveDataTypes(JNIEnv* env,
                                                                  jobject obj);
  base::android::ScopedJavaLocalRef<jintArray> GetPreferredDataTypes(
      JNIEnv* env,
      jobject obj);
  void SetPreferredDataTypes(JNIEnv* env,
                             jobject obj,
                             jboolean sync_everything,
                             jintArray model_type_selection);
  jboolean IsCryptographerReady(JNIEnv* env, jobject);
  jboolean IsEncryptEverythingAllowed(JNIEnv* env, jobject obj);
  jboolean IsEncryptEverythingEnabled(JNIEnv* env, jobject obj);
  void EnableEncryptEverything(JNIEnv* env, jobject obj);
  jboolean IsPassphraseRequired(JNIEnv* env, jobject obj);
  jboolean IsPassphraseRequiredForDecryption(JNIEnv* env, jobject obj);
  jboolean IsUsingSecondaryPassphrase(JNIEnv* env, jobject obj);
  jint GetPassphraseType(JNIEnv* env, jobject obj);
  void SetEncryptionPassphrase(JNIEnv* env,
                               jobject obj,
                               jstring passphrase);
  jboolean SetDecryptionPassphrase(JNIEnv* env,
                                   jobject obj,
                                   jstring passphrase);
  jboolean HasExplicitPassphraseTime(JNIEnv* env, jobject);
  jlong GetExplicitPassphraseTime(JNIEnv* env, jobject);
  void FlushDirectory(JNIEnv* env, jobject obj);
  base::android::ScopedJavaLocalRef<jstring> QuerySyncStatusSummary(
      JNIEnv* env, jobject obj);
  void GetAllNodes(JNIEnv* env, jobject obj, jobject callback);
  jint GetAuthError(JNIEnv* env, jobject obj);
  jboolean HasUnrecoverableError(JNIEnv* env, jobject obj);

  // Pure SyncPrefs calls.
  jboolean IsPassphrasePrompted(JNIEnv* env, jobject obj);
  void SetPassphrasePrompted(JNIEnv* env, jobject obj, jboolean prompted);
  void SetSyncSessionsId(JNIEnv* env, jobject obj, jstring tag);
  jboolean HasKeepEverythingSynced(JNIEnv* env, jobject obj);

  // UI string getters.

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

  // Functionality only available for testing purposes.

  // Returns sync internals in a JSON-formatted Java string.
  base::android::ScopedJavaLocalRef<jstring> GetAboutInfoForTest(JNIEnv* env,
                                                                 jobject obj);

  // Returns a timestamp for when a sync was last executed. The return value is
  // the internal value of base::Time.
  jlong GetLastSyncedTimeForTest(JNIEnv* env, jobject obj);

  // Overrides ProfileSyncService's NetworkResources object. This is used to
  // set up the Sync FakeServer for testing.
  void OverrideNetworkResourcesForTest(JNIEnv* env,
                                       jobject obj,
                                       jlong network_resources);

  static ProfileSyncServiceAndroid* GetProfileSyncServiceAndroid();

  // Registers the ProfileSyncServiceAndroid's native methods through JNI.
  static bool Register(JNIEnv* env);

 private:
  ~ProfileSyncServiceAndroid() override;

  // A reference to the Chrome profile object.
  Profile* profile_;

  // A reference to the sync service for this profile.
  ProfileSyncService* sync_service_;

  // The class that handles getting, setting, and persisting sync
  // preferences.
  scoped_ptr<sync_driver::SyncPrefs> sync_prefs_;

  // Java-side ProfileSyncService object.
  JavaObjectWeakGlobalRef weak_java_profile_sync_service_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncServiceAndroid);
};

#endif  // CHROME_BROWSER_SYNC_PROFILE_SYNC_SERVICE_ANDROID_H_
