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
#include "chrome/browser/ntp_snippets/content_suggestions_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/history/core/browser/history_service.h"
#include "components/ntp_snippets/content_suggestion.h"
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
using ntp_snippets::Category;
using ntp_snippets::CategoryStatus;
using ntp_snippets::KnownCategories;

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
                          const JavaParamRef<jclass>& caller,
                          jboolean j_force_request) {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  ContentSuggestionsServiceFactory::GetForProfile(profile)
      ->ntp_snippets_service()
      ->FetchSnippets(j_force_request);
}

// Reschedules the fetching of snippets. Used to support different fetching
// intervals for different times of day.
static void RescheduleFetching(JNIEnv* env,
                               const JavaParamRef<jclass>& caller) {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  ContentSuggestionsServiceFactory::GetForProfile(profile)
      ->ntp_snippets_service()
      ->RescheduleFetching();
}

NTPSnippetsBridge::NTPSnippetsBridge(JNIEnv* env,
                                     const JavaParamRef<jobject>& j_profile)
    : content_suggestions_service_observer_(this), weak_ptr_factory_(this) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  content_suggestions_service_ =
      ContentSuggestionsServiceFactory::GetForProfile(profile);
  history_service_ =
      HistoryServiceFactory::GetForProfile(profile,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  content_suggestions_service_observer_.Add(content_suggestions_service_);
}

void NTPSnippetsBridge::Destroy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  delete this;
}

void NTPSnippetsBridge::SetObserver(JNIEnv* env,
                                    const JavaParamRef<jobject>& obj,
                                    const JavaParamRef<jobject>& j_observer) {
  observer_.Reset(env, j_observer);
  OnNewSuggestions();
}

void NTPSnippetsBridge::FetchImage(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj,
                                   const JavaParamRef<jstring>& snippet_id,
                                   const JavaParamRef<jobject>& j_callback) {
  base::android::ScopedJavaGlobalRef<jobject> callback(j_callback);
  content_suggestions_service_->FetchSuggestionImage(
      ConvertJavaStringToUTF8(env, snippet_id),
      base::Bind(&NTPSnippetsBridge::OnImageFetched,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void NTPSnippetsBridge::DiscardSnippet(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj,
                                       const JavaParamRef<jstring>& id) {
  content_suggestions_service_->DismissSuggestion(
      ConvertJavaStringToUTF8(env, id));
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

int NTPSnippetsBridge::GetCategoryStatus(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj) {
  return static_cast<int>(content_suggestions_service_->GetCategoryStatus(
      content_suggestions_service_->category_factory()->FromKnownCategory(
          KnownCategories::ARTICLES)));
}

NTPSnippetsBridge::~NTPSnippetsBridge() {}

void NTPSnippetsBridge::OnNewSuggestions() {
  if (observer_.is_null())
    return;

  std::vector<std::string> ids;
  std::vector<base::string16> titles;
  // URL for the article. This will also be used to find the favicon for the
  // article.
  std::vector<std::string> urls;
  // URL for the AMP version of the article if it exists. This will be used as
  // the URL to direct the user to on tap.
  std::vector<std::string> amp_urls;
  std::vector<base::string16> snippet_texts;
  std::vector<int64_t> timestamps;
  std::vector<base::string16> publisher_names;
  std::vector<float> scores;

  // Show all suggestions from all categories, even though we currently display
  // them in a single section on the UI.
  // TODO(pke): This is only for debugging new sections and will be replaced
  // with proper multi-section UI support.
  for (Category category : content_suggestions_service_->GetCategories()) {
    if (content_suggestions_service_->GetCategoryStatus(category) !=
        CategoryStatus::AVAILABLE) {
      continue;
    }
    for (const ntp_snippets::ContentSuggestion& suggestion :
         content_suggestions_service_->GetSuggestionsForCategory(category)) {
      ids.push_back(suggestion.id());
      titles.push_back(suggestion.title());
      // The url from source_info is a url for a site that is one of the
      // HOST_RESTRICT parameters, so this is preferred.
      urls.push_back(suggestion.url().spec());
      amp_urls.push_back(suggestion.amp_url().spec());
      snippet_texts.push_back(suggestion.snippet_text());
      timestamps.push_back(suggestion.publish_date().ToJavaTime());
      publisher_names.push_back(suggestion.publisher_name());
      scores.push_back(suggestion.score());
    }
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SnippetsBridge_onSnippetsAvailable(
      env, observer_.obj(), ToJavaArrayOfStrings(env, ids).obj(),
      ToJavaArrayOfStrings(env, titles).obj(),
      ToJavaArrayOfStrings(env, urls).obj(),
      ToJavaArrayOfStrings(env, amp_urls).obj(),
      ToJavaArrayOfStrings(env, snippet_texts).obj(),
      ToJavaLongArray(env, timestamps).obj(),
      ToJavaArrayOfStrings(env, publisher_names).obj(),
      ToJavaFloatArray(env, scores).obj());
}

void NTPSnippetsBridge::OnCategoryStatusChanged(Category category,
                                                CategoryStatus new_status) {
  if (!category.IsKnownCategory(KnownCategories::ARTICLES))
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SnippetsBridge_onCategoryStatusChanged(env, observer_.obj(),
                                              static_cast<int>(new_status));
}

void NTPSnippetsBridge::ContentSuggestionsServiceShutdown() {
  observer_.Reset();
  content_suggestions_service_observer_.Remove(content_suggestions_service_);
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
