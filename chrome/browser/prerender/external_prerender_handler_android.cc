// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/external_prerender_handler_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prerender/prerender_handle.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/prerender/prerender_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "content/public/browser/web_contents.h"

#include "jni/ExternalPrerenderHandler_jni.h"

using base::android::ConvertJavaStringToUTF16;

namespace prerender {

bool ExternalPrerenderHandlerAndroid::AddPrerender(JNIEnv* env,
                                                   jobject obj,
                                                   jobject jprofile,
                                                   jlong web_contents_ptr,
                                                   jstring jurl,
                                                   jstring jreferrer,
                                                   jint width,
                                                   jint height) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);

  GURL url = GURL(ConvertJavaStringToUTF16(env, jurl));
  if (!url.is_valid())
    return false;

  GURL referrer_url = GURL(ConvertJavaStringToUTF16(env, jreferrer));
  content::Referrer referrer = referrer_url.is_valid() ?
      content::Referrer(referrer_url, blink::WebReferrerPolicyDefault) :
      content::Referrer();
  PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForProfile(profile);
  if (!prerender_manager)
    return false;
  content::WebContents* web_contents =
      reinterpret_cast<content::WebContents*>(web_contents_ptr);
  if (prerender_handle_.get()) {
    prerender_handle_->OnNavigateAway();
  }
  prerender_handle_.reset(
      prerender_manager->AddPrerenderFromExternalRequest(
          url,
          referrer,
          web_contents->GetController().GetDefaultSessionStorageNamespace(),
          gfx::Size(width, height)));
  if (!prerender_handle_)
    return false;
  return true;
}

void ExternalPrerenderHandlerAndroid::CancelCurrentPrerender(JNIEnv* env,
                                                             jobject object) {
  if (!prerender_handle_)
    return;
  prerender_handle_->OnCancel();
  prerender_handle_.reset();
}

static jboolean HasPrerenderedUrl(JNIEnv* env,
                                  jclass clazz,
                                  jobject jprofile,
                                  jstring jurl,
                                  jlong web_contents_ptr) {
  if (jurl == NULL)
    return false;

  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  GURL url = GURL(ConvertJavaStringToUTF16(env, jurl));
  if (!url.is_valid())
    return false;
  prerender::PrerenderManager* prerender_manager =
          prerender::PrerenderManagerFactory::GetForProfile(profile);
  if (!prerender_manager)
    return false;
  content::WebContents* web_contents =
      reinterpret_cast<content::WebContents*>(web_contents_ptr);
  return prerender_manager->HasPrerenderedUrl(url, web_contents);
}

static jboolean HasCookieStoreLoaded(JNIEnv* env,
                                     jclass clazz,
                                     jobject jprofile) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(jprofile);
  prerender::PrerenderManager* prerender_manager =
      prerender::PrerenderManagerFactory::GetForProfile(profile);
  if (!prerender_manager)
    return false;
  return prerender_manager->cookie_store_loaded();
}

ExternalPrerenderHandlerAndroid::ExternalPrerenderHandlerAndroid() {}

ExternalPrerenderHandlerAndroid::~ExternalPrerenderHandlerAndroid() {}

static jlong Init(JNIEnv* env, jclass clazz) {
  ExternalPrerenderHandlerAndroid* external_handler =
      new ExternalPrerenderHandlerAndroid();
  return reinterpret_cast<intptr_t>(external_handler);
}

bool ExternalPrerenderHandlerAndroid::RegisterExternalPrerenderHandlerAndroid(
    JNIEnv* env) {
  return RegisterNativesImpl(env);
}

} // namespace prerender
