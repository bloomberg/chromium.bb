// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/background_sync_launcher_android.h"

#include "content/public/browser/browser_thread.h"
#include "jni/BackgroundSyncLauncher_jni.h"

using content::BrowserThread;

namespace {
base::LazyInstance<BackgroundSyncLauncherAndroid> g_background_sync_launcher =
    LAZY_INSTANCE_INITIALIZER;
}

// static
BackgroundSyncLauncherAndroid* BackgroundSyncLauncherAndroid::Get() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  return g_background_sync_launcher.Pointer();
}

// static
void BackgroundSyncLauncherAndroid::LaunchBrowserWhenNextOnline(
    bool launch_when_next_online) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  Get()->LaunchBrowserWhenNextOnlineImpl(launch_when_next_online);
}

void BackgroundSyncLauncherAndroid::LaunchBrowserWhenNextOnlineImpl(
    bool launch_when_next_online) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BackgroundSyncLauncher_launchBrowserWhenNextOnlineIfStopped(
      env, java_launcher_.obj(), base::android::GetApplicationContext(),
      launch_when_next_online);
}

// static
bool BackgroundSyncLauncherAndroid::RegisterLauncher(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

BackgroundSyncLauncherAndroid::BackgroundSyncLauncherAndroid() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  JNIEnv* env = base::android::AttachCurrentThread();
  java_launcher_.Reset(Java_BackgroundSyncLauncher_create(
      env, base::android::GetApplicationContext()));
}

BackgroundSyncLauncherAndroid::~BackgroundSyncLauncherAndroid() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_BackgroundSyncLauncher_destroy(env, java_launcher_.obj());
}
