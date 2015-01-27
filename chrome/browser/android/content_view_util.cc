// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/content_view_util.h"

#include "base/android/jni_android.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/web_contents.h"
#include "jni/ContentViewUtil_jni.h"

static jobject CreateWebContents(
    JNIEnv* env, jclass clazz, jboolean incognito, jboolean initially_hidden) {
  Profile* profile = g_browser_process->profile_manager()->GetLastUsedProfile();
  if (incognito)
    profile = profile->GetOffTheRecordProfile();

  content::WebContents::CreateParams params(profile);
  params.initially_hidden = static_cast<bool>(initially_hidden);
  return content::WebContents::Create(params)->GetJavaWebContents().Release();
}

static jobject CreateWebContentsWithSharedSiteInstance(
    JNIEnv* env,
    jclass clazz,
    jobject jcontent_view_core) {
  content::ContentViewCore* content_view_core =
      content::ContentViewCore::GetNativeContentViewCore(env,
                                                         jcontent_view_core);
  CHECK(content_view_core);

  Profile* profile = Profile::FromBrowserContext(
      content_view_core->GetWebContents()->GetBrowserContext());

  content::WebContents::CreateParams params(
      profile, content_view_core->GetWebContents()->GetSiteInstance());

  return content::WebContents::Create(params)->GetJavaWebContents().Release();
}

static jobject GetWebContentsFromNative(
    JNIEnv* env, jclass clazz, jlong web_contents_ptr) {
  content::WebContents* web_contents =
      reinterpret_cast<content::WebContents*>(web_contents_ptr);
  if (!web_contents)
    return NULL;
  return web_contents->GetJavaWebContents().Release();
}

static jlong GetNativeWebContentsPtr(
    JNIEnv* env, jclass clazz, jobject jweb_contents) {
  return reinterpret_cast<intptr_t>(
      content::WebContents::FromJavaWebContents(jweb_contents));
}

bool RegisterContentViewUtil(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
