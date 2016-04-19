// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/ntp/ntp_snippets_bridge.h"

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/ntp_snippets/ntp_snippets_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/ntp_snippets/ntp_snippet.h"
#include "components/ntp_snippets/ntp_snippets_service.h"
#include "jni/SnippetsBridge_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::ToJavaArrayOfStrings;
using base::android::ToJavaLongArray;

static jlong Init(JNIEnv* env,
                  const JavaParamRef<jobject>& obj,
                  const JavaParamRef<jobject>& j_profile) {
  NTPSnippetsBridge* snippets_bridge = new NTPSnippetsBridge(env, j_profile);
  return reinterpret_cast<intptr_t>(snippets_bridge);
}

static void FetchSnippets(JNIEnv* env,
                          const JavaParamRef<jclass>& caller) {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  NTPSnippetsServiceFactory::GetForProfile(profile)->FetchSnippets();
}

NTPSnippetsBridge::NTPSnippetsBridge(JNIEnv* env,
                                     const JavaParamRef<jobject>& j_profile)
    : snippet_service_observer_(this) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  ntp_snippets_service_ = NTPSnippetsServiceFactory::GetForProfile(profile);
  snippet_service_observer_.Add(ntp_snippets_service_);
}

void NTPSnippetsBridge::SetObserver(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj,
                                    const JavaParamRef<jobject>& j_observer) {
  observer_.Reset(env, j_observer);
  NTPSnippetsServiceLoaded();
}

NTPSnippetsBridge::~NTPSnippetsBridge() {}

void NTPSnippetsBridge::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

void NTPSnippetsBridge::DiscardSnippet(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj,
                                       const JavaParamRef<jstring>& url) {
  ntp_snippets_service_->DiscardSnippet(
      GURL(ConvertJavaStringToUTF8(env, url)));
}

void NTPSnippetsBridge::NTPSnippetsServiceLoaded() {
  if (observer_.is_null())
    return;

  std::vector<std::string> titles;
  std::vector<std::string> urls;
  std::vector<std::string> thumbnail_urls;
  std::vector<std::string> snippets;
  std::vector<int64_t> timestamps;
  for (const ntp_snippets::NTPSnippet& snippet : *ntp_snippets_service_) {
    titles.push_back(snippet.title());
    urls.push_back(snippet.url().spec());
    thumbnail_urls.push_back(snippet.salient_image_url().spec());
    snippets.push_back(snippet.snippet());
    timestamps.push_back(snippet.publish_date().ToJavaTime());
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SnippetsBridge_onSnippetsAvailable(
      env, observer_.obj(), ToJavaArrayOfStrings(env, titles).obj(),
      ToJavaArrayOfStrings(env, urls).obj(),
      ToJavaArrayOfStrings(env, thumbnail_urls).obj(),
      ToJavaArrayOfStrings(env, snippets).obj(),
      ToJavaLongArray(env, timestamps).obj());
}

void NTPSnippetsBridge::NTPSnippetsServiceShutdown() {
  observer_.Reset();
  snippet_service_observer_.Remove(ntp_snippets_service_);
}

// static
bool NTPSnippetsBridge::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
