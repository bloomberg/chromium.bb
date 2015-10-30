// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FOREIGN_SESSION_HELPER_H_
#define CHROME_BROWSER_ANDROID_FOREIGN_SESSION_HELPER_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/scoped_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sync_driver/sync_service_observer.h"

using base::android::ScopedJavaLocalRef;

struct SessionWindow;

namespace browser_sync {
struct SyncedSession;
}  // namespace browser_sync

namespace sync_driver {
class SyncService;
}  // namespace sync_driver

class ForeignSessionHelper : public sync_driver::SyncServiceObserver {
 public:
  explicit ForeignSessionHelper(Profile* profile);
  void Destroy(JNIEnv* env, jobject obj);
  jboolean IsTabSyncEnabled(JNIEnv* env, jobject obj);
  void TriggerSessionSync(JNIEnv* env, jobject obj);
  void SetOnForeignSessionCallback(JNIEnv* env, jobject obj, jobject callback);
  jboolean GetForeignSessions(JNIEnv* env, jobject obj, jobject result);
  jboolean OpenForeignSessionTab(JNIEnv* env,
                                 jobject obj,
                                 jobject j_tab,
                                 jstring session_tag,
                                 jint tab_id,
                                 jint disposition);
  void DeleteForeignSession(JNIEnv* env, jobject obj, jstring session_tag);

  // sync_driver::SyncServiceObserver implementation
  void OnStateChanged() override {}
  void OnSyncConfigurationCompleted() override;
  void OnForeignSessionUpdated() override;

  static bool RegisterForeignSessionHelper(JNIEnv* env);

 private:
  ~ForeignSessionHelper() override;

  // Fires |callback_| if it is not null.
  void FireForeignSessionCallback();

  Profile* profile_;  // weak
  base::android::ScopedJavaGlobalRef<jobject> callback_;
  ScopedObserver<sync_driver::SyncService, sync_driver::SyncServiceObserver>
      scoped_observer_;

  DISALLOW_COPY_AND_ASSIGN(ForeignSessionHelper);
};

#endif  // CHROME_BROWSER_ANDROID_FOREIGN_SESSION_HELPER_H_
