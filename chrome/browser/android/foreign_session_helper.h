// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FOREIGN_SESSION_HELPER_H_
#define CHROME_BROWSER_ANDROID_FOREIGN_SESSION_HELPER_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

using base::android::ScopedJavaLocalRef;

struct SessionWindow;

namespace browser_sync {
struct SyncedSession;
}  // namespace browser_sync

class ForeignSessionHelper : public content::NotificationObserver {
 public:
  explicit ForeignSessionHelper(Profile* profile);
  void Destroy(JNIEnv* env, jobject obj);
  jboolean IsTabSyncEnabled(JNIEnv* env, jobject obj);
  void SetOnForeignSessionCallback(JNIEnv* env, jobject obj, jobject callback);
  jboolean GetForeignSessions(JNIEnv* env, jobject obj, jobject result);
  jboolean OpenForeignSessionTab(JNIEnv* env,
                                 jobject obj,
                                 jobject j_tab,
                                 jstring session_tag,
                                 jint tab_id,
                                 jint disposition);
  void DeleteForeignSession(JNIEnv* env, jobject obj, jstring session_tag);

  // NotificationObserver implemenation
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  static bool RegisterForeignSessionHelper(JNIEnv* env);

 private:
  virtual ~ForeignSessionHelper();

  Profile* profile_;  // weak
  base::android::ScopedJavaGlobalRef<jobject> callback_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ForeignSessionHelper);
};

#endif  // CHROME_BROWSER_ANDROID_FOREIGN_SESSION_HELPER_H_
