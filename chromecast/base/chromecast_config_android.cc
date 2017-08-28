// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/base/chromecast_config_android.h"

#include <utility>

#include "base/android/jni_android.h"
#include "base/lazy_instance.h"
#include "jni/ChromecastConfigAndroid_jni.h"

using base::android::JavaParamRef;

namespace chromecast {
namespace android {

namespace {
base::LazyInstance<ChromecastConfigAndroid>::DestructorAtExit g_instance =
    LAZY_INSTANCE_INITIALIZER;
}  // namespace

// static
ChromecastConfigAndroid* ChromecastConfigAndroid::GetInstance() {
  return g_instance.Pointer();
}

ChromecastConfigAndroid::ChromecastConfigAndroid() {
}

ChromecastConfigAndroid::~ChromecastConfigAndroid() {
}

bool ChromecastConfigAndroid::CanSendUsageStats() {
  // TODO(gunsch): make opt-in.stats pref the source of truth for this data,
  // instead of Android prefs, then delete ChromecastConfigAndroid.
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_ChromecastConfigAndroid_canSendUsageStats(env);
}

// Registers a handler to be notified when SendUsageStats is changed.
void ChromecastConfigAndroid::SetSendUsageStatsChangedCallback(
    base::RepeatingCallback<void(bool)> callback) {
  send_usage_stats_changed_callback_ = std::move(callback);
}

void ChromecastConfigAndroid::RunSendUsageStatsChangedCallback(bool enabled) {
  send_usage_stats_changed_callback_.Run(enabled);
}

// Called from Java.
void SetSendUsageStatsEnabled(JNIEnv* env,
                              const JavaParamRef<jclass>& caller,
                              jboolean enabled) {
  ChromecastConfigAndroid::GetInstance()->RunSendUsageStatsChangedCallback(
      enabled);
}

}  // namespace android
}  // namespace chromecast
