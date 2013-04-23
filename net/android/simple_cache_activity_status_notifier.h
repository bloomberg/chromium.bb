// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_ANDROID_SIMPLE_CACHE_ACTIVITY_STATUS_NOTIFIER_H_
#define NET_ANDROID_SIMPLE_CACHE_ACTIVITY_STATUS_NOTIFIER_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace net {

// This class is the native twin of the class with same name in
// SimpleCacheActivityStatusNotifier.java
// This is used by the SimpleIndex in net/disk_cache/simple/ to listens to
// changes in the android app state such as the app going to the background or
// foreground.
class SimpleCacheActivityStatusNotifier {
public:
  // This enum must match the constants defined in
  // ./base/android/java/src/org/chromium/base/ActivityStatus.java
  enum ActivityStatus {
    CREATED = 1,
    STARTED = 2,
    RESUMED = 3,
    PAUSED = 4,
    STOPPED = 5,
    DESTROYED = 6
  };

  typedef base::Callback<void(ActivityStatus activity_status)>
      ActivityStatusChangedCallback;

  SimpleCacheActivityStatusNotifier(
      const ActivityStatusChangedCallback& notify_callback);

  ~SimpleCacheActivityStatusNotifier();

  void NotifyActivityStatusChanged(JNIEnv* env,
                                   jobject obj,
                                   jint new_activity_status);

  static bool Register(JNIEnv* env);

private:
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;
  const ActivityStatusChangedCallback notify_callback_;
  base::ThreadChecker thread_checker_;
};

}  // namespace net

#endif  // NET_ANDROID_SIMPLE_CACHE_ACTIVITY_STATUS_NOTIFIER_H_
