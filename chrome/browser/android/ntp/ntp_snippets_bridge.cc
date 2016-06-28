// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/ntp/ntp_snippets_bridge.h"

#include <jni.h>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/callback.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/ntp_snippets/ntp_snippets_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/history/core/browser/history_service.h"
#include "components/ntp_snippets/ntp_snippet.h"
#include "components/ntp_snippets/ntp_snippets_service.h"
#include "jni/SnippetsBridge_jni.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::ToJavaArrayOfStrings;
using base::android::ToJavaLongArray;
using base::android::ToJavaFloatArray;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace {

void SnippetVisitedHistoryRequestCallback(
      base::android::ScopedJavaGlobalRef<jobject> callback,
      bool success,
      const history::URLRow& row,
      const history::VisitVector& visitVector) {
  bool visited = success && row.visit_count() != 0;
  base::android::RunCallbackAndroid(callback, visited);
}

} // namespace

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

// Reschedules the fetching of snippets. Used to support different fetching
// intervals for different times of day.
static void RescheduleFetching(JNIEnv* env,
                               const JavaParamRef<jclass>& caller) {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  NTPSnippetsServiceFactory::GetForProfile(profile)->RescheduleFetching();
}

NTPSnippetsBridge::NTPSnippetsBridge(JNIEnv* env,
                                     const JavaParamRef<jobject>& j_profile)
    : snippet_service_observer_(this), weak_ptr_factory_(this) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  ntp_snippets_service_ = NTPSnippetsServiceFactory::GetForProfile(profile);
  history_service_ =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  snippet_service_observer_.Add(ntp_snippets_service_);
}

void NTPSnippetsBridge::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

void NTPSnippetsBridge::SetObserver(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj,
                                    const JavaParamRef<jobject>& j_observer) {
  observer_.Reset(env, j_observer);
  NTPSnippetsServiceLoaded();
}

void NTPSnippetsBridge::FetchImage(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj,
                                   const JavaParamRef<jstring>& snippet_id,
                                   const JavaParamRef<jobject>& j_callback) {
  base::android::ScopedJavaGlobalRef<jobject> callback(j_callback);
  ntp_snippets_service_->FetchSnippetImage(
      ConvertJavaStringToUTF8(env, snippet_id),
      base::Bind(&NTPSnippetsBridge::OnImageFetched,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void NTPSnippetsBridge::DiscardSnippet(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj,
                                       const JavaParamRef<jstring>& id) {
  ntp_snippets_service_->DiscardSnippet(ConvertJavaStringToUTF8(env, id));
}

void NTPSnippetsBridge::SnippetVisited(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj,
                                       const JavaParamRef<jobject>& jcallback,
                                       const JavaParamRef<jstring>& jurl) {
  base::android::ScopedJavaGlobalRef<jobject> callback(jcallback);

  history_service_->QueryURL(
      GURL(ConvertJavaStringToUTF8(env, jurl)),
      false,
      base::Bind(&SnippetVisitedHistoryRequestCallback, callback),
      &tracker_);
}

int NTPSnippetsBridge::GetDisabledReason(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj) {
  return static_cast<int>(ntp_snippets_service_->disabled_reason());
}

NTPSnippetsBridge::~NTPSnippetsBridge() {}

void NTPSnippetsBridge::NTPSnippetsServiceLoaded() {
  if (observer_.is_null())
    return;

  std::vector<std::string> ids;
  std::vector<std::string> titles;
  // URL for the article. This will also be used to find the favicon for the
  // article.
  std::vector<std::string> urls;
  // URL for the AMP version of the article if it exists. This will be used as
  // the URL to direct the user to on tap.
  std::vector<std::string> amp_urls;
  std::vector<std::string> thumbnail_urls;
  std::vector<std::string> snippets;
  std::vector<int64_t> timestamps;
  std::vector<std::string> publishers;
  std::vector<float> scores;
  for (const std::unique_ptr<ntp_snippets::NTPSnippet>& snippet :
       ntp_snippets_service_->snippets()) {
    ids.push_back(snippet->id());
    titles.push_back(snippet->title());
    // The url from source_info is a url for a site that is one of the
    // HOST_RESTRICT parameters, so this is preferred.
    urls.push_back(snippet->best_source().url.spec());
    amp_urls.push_back(snippet->best_source().amp_url.spec());
    thumbnail_urls.push_back(snippet->salient_image_url().spec());
    snippets.push_back(snippet->snippet());
    timestamps.push_back(snippet->publish_date().ToJavaTime());
    publishers.push_back(snippet->best_source().publisher_name);
    scores.push_back(snippet->score());
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SnippetsBridge_onSnippetsAvailable(
      env, observer_.obj(), ToJavaArrayOfStrings(env, ids).obj(),
      ToJavaArrayOfStrings(env, titles).obj(),
      ToJavaArrayOfStrings(env, urls).obj(),
      ToJavaArrayOfStrings(env, amp_urls).obj(),
      ToJavaArrayOfStrings(env, thumbnail_urls).obj(),
      ToJavaArrayOfStrings(env, snippets).obj(),
      ToJavaLongArray(env, timestamps).obj(),
      ToJavaArrayOfStrings(env, publishers).obj(),
      ToJavaFloatArray(env, scores).obj());
}

void NTPSnippetsBridge::NTPSnippetsServiceShutdown() {
  observer_.Reset();
  snippet_service_observer_.Remove(ntp_snippets_service_);
}

void NTPSnippetsBridge::NTPSnippetsServiceDisabledReasonChanged(
    ntp_snippets::DisabledReason disabled_reason) {
  // The user signed out or disabled sync. Since snippets rely on those, we
  // clear them to be consistent with the initially signed out state.
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SnippetsBridge_onDisabledReasonChanged(
      env, observer_.obj(), static_cast<int>(disabled_reason));
}

void NTPSnippetsBridge::OnImageFetched(ScopedJavaGlobalRef<jobject> callback,
                                       const std::string& snippet_id,
                                       const gfx::Image& image) {
  ScopedJavaLocalRef<jobject> j_bitmap;
  if (!image.IsEmpty())
    j_bitmap = gfx::ConvertToJavaBitmap(image.ToSkBitmap());

  base::android::RunCallbackAndroid(callback, j_bitmap);
}

// static
bool NTPSnippetsBridge::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
