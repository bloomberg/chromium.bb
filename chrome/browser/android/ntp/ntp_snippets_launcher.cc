// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/ntp/ntp_snippets_launcher.h"

#include "base/android/context_utils.h"
#include "content/public/browser/browser_thread.h"
#include "jni/SnippetsLauncher_jni.h"

using content::BrowserThread;

namespace {

base::LazyInstance<NTPSnippetsLauncher> g_snippets_launcher =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

// static
NTPSnippetsLauncher* NTPSnippetsLauncher::Get() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return g_snippets_launcher.Pointer();
}

// static
bool NTPSnippetsLauncher::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

bool NTPSnippetsLauncher::Schedule(base::TimeDelta period_wifi_charging,
                                   base::TimeDelta period_wifi,
                                   base::TimeDelta period_fallback,
                                   base::Time reschedule_time) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_SnippetsLauncher_schedule(
      env, java_launcher_.obj(), period_wifi_charging.InSeconds(),
      period_wifi.InSeconds(), period_fallback.InSeconds(),
      reschedule_time.ToJavaTime());
}

bool NTPSnippetsLauncher::Unschedule() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_SnippetsLauncher_unschedule(env, java_launcher_.obj());
}

NTPSnippetsLauncher::NTPSnippetsLauncher() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  JNIEnv* env = base::android::AttachCurrentThread();
  java_launcher_.Reset(Java_SnippetsLauncher_create(
      env, base::android::GetApplicationContext()));
}

NTPSnippetsLauncher::~NTPSnippetsLauncher() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SnippetsLauncher_destroy(env, java_launcher_.obj());
  java_launcher_.Reset();
}
