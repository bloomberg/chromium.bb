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
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
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
                                         const JavaParamRef<jobject>& obj,
                                         jint category) {
  return static_cast<int>(content_suggestions_service_->GetCategoryStatus(
      content_suggestions_service_->category_factory()->FromKnownCategory(
          KnownCategories::ARTICLES)));
}

NTPSnippetsBridge::~NTPSnippetsBridge() {}

void NTPSnippetsBridge::OnNewSuggestions() {
  if (observer_.is_null())
    return;

  // Show all suggestions from all categories, even though we currently display
  // them in a single section on the UI.
  // TODO(pke): This is only for debugging new sections and will be replaced
  // with proper multi-section UI support.
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> suggestions =
      Java_SnippetsBridge_createSuggestionList(env);
  for (Category category :
       content_suggestions_service_->GetCategories()) {
    if (content_suggestions_service_->GetCategoryStatus(category) !=
        CategoryStatus::AVAILABLE) {
      continue;
    }
    for (const ntp_snippets::ContentSuggestion& suggestion :
         content_suggestions_service_->GetSuggestionsForCategory(category)) {
      Java_SnippetsBridge_addSuggestion(
          env, suggestions.obj(),
          ConvertUTF8ToJavaString(env, suggestion.id()).obj(),
          ConvertUTF16ToJavaString(env, suggestion.title()).obj(),
          ConvertUTF16ToJavaString(env, suggestion.publisher_name()).obj(),
          ConvertUTF16ToJavaString(env, suggestion.snippet_text()).obj(),
          ConvertUTF8ToJavaString(env, suggestion.url().spec()).obj(),
          ConvertUTF8ToJavaString(env, suggestion.amp_url().spec()).obj(),
          suggestion.publish_date().ToJavaTime(), suggestion.score());
    }
  }

  // TODO(mvanouwerkerk): Do not hard code ARTICLES.
  Java_SnippetsBridge_onSuggestionsAvailable(
      env, observer_.obj(),
      static_cast<int>(
          content_suggestions_service_->category_factory()->FromKnownCategory(
              KnownCategories::ARTICLES).id()),
      suggestions.obj());
}

void NTPSnippetsBridge::OnCategoryStatusChanged(Category category,
                                                CategoryStatus new_status) {
  // TODO(mvanouwerkerk): Do not hard code ARTICLES.
  if (!category.IsKnownCategory(KnownCategories::ARTICLES))
    return;

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_SnippetsBridge_onCategoryStatusChanged(env, observer_.obj(),
                                              static_cast<int>(category.id()),
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
