// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/contextual_suggestions/contextual_suggestions_bridge.h"

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/callback.h"
#include "chrome/browser/ntp_snippets/contextual_content_suggestions_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "jni/ContextualSuggestionsBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using Cluster = ntp_snippets::ContextualContentSuggestionsService::Cluster;

namespace contextual_suggestions {

static jlong JNI_ContextualSuggestionsBridge_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_profile) {
  ContextualSuggestionsBridge* contextual_suggestions_bridge =
      new ContextualSuggestionsBridge(env, j_profile);
  return reinterpret_cast<intptr_t>(contextual_suggestions_bridge);
}

ContextualSuggestionsBridge::ContextualSuggestionsBridge(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_profile)
    : weak_ptr_factory_(this) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(j_profile);
  contextual_content_suggestions_service_ =
      ContextualContentSuggestionsServiceFactory::GetForProfile(profile);
}

ContextualSuggestionsBridge::~ContextualSuggestionsBridge() {}

void ContextualSuggestionsBridge::Destroy(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  delete this;
}

void ContextualSuggestionsBridge::FetchSuggestions(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_url,
    const JavaParamRef<jobject>& j_callback) {
  if (contextual_content_suggestions_service_ == nullptr)
    return;

  GURL url(ConvertJavaStringToUTF8(env, j_url));
  contextual_content_suggestions_service_->FetchContextualSuggestionClusters(
      url, base::BindOnce(&ContextualSuggestionsBridge::OnSuggestionsAvailable,
                          weak_ptr_factory_.GetWeakPtr(),
                          ScopedJavaGlobalRef<jobject>(j_callback)));
}

void ContextualSuggestionsBridge::FetchSuggestionImage(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_suggestion_id,
    const JavaParamRef<jobject>& j_callback) {}

void ContextualSuggestionsBridge::FetchSuggestionFavicon(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_suggestion_id,
    const JavaParamRef<jobject>& j_callback) {}

void ContextualSuggestionsBridge::ClearState(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj) {
}

void ContextualSuggestionsBridge::ReportEvent(JNIEnv* env,
                                              const JavaParamRef<jobject>& obj,
                                              jint j_event_id) {}

void ContextualSuggestionsBridge::OnSuggestionsAvailable(
    ScopedJavaGlobalRef<jobject> j_callback,
    std::vector<Cluster> clusters) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_clusters =
      Java_ContextualSuggestionsBridge_createContextualSuggestionsClusterList(
          env);
  for (auto& cluster : clusters) {
    Java_ContextualSuggestionsBridge_addNewClusterToList(
        env, j_clusters, ConvertUTF8ToJavaString(env, cluster.title));
    for (auto& suggestion : cluster.suggestions) {
      Java_ContextualSuggestionsBridge_addSuggestionToLastCluster(
          env, j_clusters,
          ConvertUTF8ToJavaString(env, suggestion.id().id_within_category()),
          ConvertUTF16ToJavaString(env, suggestion.title()),
          ConvertUTF16ToJavaString(env, suggestion.publisher_name()),
          ConvertUTF8ToJavaString(env, suggestion.url().spec()),
          suggestion.publish_date().ToJavaTime(), suggestion.score(),
          suggestion.fetch_date().ToJavaTime(),
          suggestion.is_video_suggestion(),
          suggestion.optional_image_dominant_color().value_or(0));
    }
  }
  RunCallbackAndroid(j_callback, j_clusters);
}

}  // namespace contextual_suggestions
