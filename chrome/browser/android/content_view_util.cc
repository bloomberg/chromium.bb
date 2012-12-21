// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/content_view_util.h"

#include "base/android/jni_android.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/web_contents.h"
#include "jni/ContentViewUtil_jni.h"

static jint CreateNativeWebContents(
    JNIEnv* env, jclass clazz, jboolean incognito) {
  Profile* profile = g_browser_process->profile_manager()->GetLastUsedProfile();
  if (incognito)
    profile = profile->GetOffTheRecordProfile();

  content::WebContents* web_contents =
      content::WebContents::Create(content::WebContents::CreateParams(profile));
  return reinterpret_cast<jint>(web_contents);
}

bool RegisterContentViewUtil(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
