// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PRECACHE_PRECACHE_LAUNCHER_H_
#define CHROME_BROWSER_ANDROID_PRECACHE_PRECACHE_LAUNCHER_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/memory/weak_ptr.h"

class PrecacheLauncher {
 public:
  PrecacheLauncher(JNIEnv* env, jobject obj);
  void Destroy(JNIEnv* env, jobject obj);
  void Start(JNIEnv* env, jobject obj);
  void Cancel(JNIEnv* env, jobject obj);

 private:
  ~PrecacheLauncher();
  // Called when precaching completes. |try_again_soon| is true iff the precache
  // failed to start due to a transient error and should be attempted again
  // soon.
  void OnPrecacheCompleted(bool try_again_soon);

  JavaObjectWeakGlobalRef weak_java_precache_launcher_;

  // This must be the last member field in the class.
  base::WeakPtrFactory<PrecacheLauncher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PrecacheLauncher);
};

// Registers the native methods to be called from Java.
bool RegisterPrecacheLauncher(JNIEnv* env);

#endif  // CHROME_BROWSER_ANDROID_PRECACHE_PRECACHE_LAUNCHER_H_
