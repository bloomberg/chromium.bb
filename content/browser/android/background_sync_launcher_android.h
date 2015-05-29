// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_BACKGROUND_SYNC_LAUNCHER_ANDROID_H_
#define CONTENT_BROWSER_ANDROID_BACKGROUND_SYNC_LAUNCHER_ANDROID_H_

#include <set>

#include "base/android/jni_android.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "content/common/content_export.h"

namespace content {

class BackgroundSyncManager;

// The BackgroundSyncLauncherAndroid singleton owns the Java
// BackgroundSyncLauncher object and is used to register interest in starting
// the browser the next time the device goes online. This class runs on the UI
// thread.
class CONTENT_EXPORT BackgroundSyncLauncherAndroid {
 public:
  static BackgroundSyncLauncherAndroid* Get();

  // Register the |registrant|'s interest (or disinterest) in starting the
  // browser the next time the device goes online after the browser has closed.
  // |registrant| is used to count the number of interested objects and is not
  // accessed, therefore it is okay for |registrant| to be deleted before this
  // class. Interest is reset the next time the BackgroundSyncLauncherAndroid is
  // created (browser restart). The caller can remove interest in launching the
  // browser by calling with |launch_when_next_online| set to false.
  static void LaunchBrowserWhenNextOnline(
      const BackgroundSyncManager* registrant,
      bool launch_when_next_online);

  static bool RegisterLauncher(JNIEnv* env);

 private:
  friend struct base::DefaultLazyInstanceTraits<BackgroundSyncLauncherAndroid>;

  // Constructor and destructor marked private to enforce singleton
  BackgroundSyncLauncherAndroid();
  ~BackgroundSyncLauncherAndroid();

  void LaunchBrowserWhenNextOnlineImpl(const BackgroundSyncManager* registrant,
                                       bool launch_when_next_online);

  std::set<const BackgroundSyncManager*> launch_when_next_online_registrants_;
  base::android::ScopedJavaGlobalRef<jobject> java_launcher_;
  DISALLOW_COPY_AND_ASSIGN(BackgroundSyncLauncherAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_BACKGROUND_SYNC_LAUNCHER_ANDROID_H_
