// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/contextual_suggestions/contextual_suggestions_bridge.h"

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/callback.h"
#include "chrome/browser/ntp_snippets/content_suggestions_service_factory.h"
#include "chrome/browser/ntp_snippets/contextual_content_suggestions_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_android.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/ukm/content/source_url_recorder.h"
#include "content/public/browser/web_contents.h"
#include "jni/ContextualSuggestionsBridge_jni.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/image/image.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using Cluster = ntp_snippets::ContextualContentSuggestionsService::Cluster;

namespace contextual_suggestions {

namespace {

// Min size for site attribution.
static const int kPublisherFaviconMinimumSizePx = 16;
// Desired size for site attribution.
static const int kPublisherFaviconDesiredSizePx = 32;

ntp_snippets::ContentSuggestion::ID ToContentSuggestionID(
    JNIEnv* env,
    const JavaParamRef<jstring>& j_suggestion_id) {
  ntp_snippets::Category category(ntp_snippets::Category::FromKnownCategory(
      ntp_snippets::KnownCategories::CONTEXTUAL));
  std::string suggestion_id(ConvertJavaStringToUTF8(env, j_suggestion_id));
  return ntp_snippets::ContentSuggestion::ID(category, suggestion_id);
}

}  // namespace

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
  content_suggestions_service_ =
      ContentSuggestionsServiceFactory::GetForProfile(profile);
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
  if (!contextual_content_suggestions_service_)
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
    const JavaParamRef<jobject>& j_callback) {
  if (!contextual_content_suggestions_service_)
    return;

  contextual_content_suggestions_service_->FetchContextualSuggestionImage(
      ToContentSuggestionID(env, j_suggestion_id),
      base::BindOnce(&ContextualSuggestionsBridge::OnImageFetched,
                     weak_ptr_factory_.GetWeakPtr(),
                     ScopedJavaGlobalRef<jobject>(j_callback)));
}

void ContextualSuggestionsBridge::FetchSuggestionFavicon(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& j_suggestion_id,
    const JavaParamRef<jobject>& j_callback) {
  if (contextual_content_suggestions_service_ == nullptr)
    return;

  content_suggestions_service_->FetchSuggestionFavicon(
      ToContentSuggestionID(env, j_suggestion_id),
      kPublisherFaviconMinimumSizePx, kPublisherFaviconDesiredSizePx,
      base::BindOnce(&ContextualSuggestionsBridge::OnImageFetched,
                     weak_ptr_factory_.GetWeakPtr(),
                     ScopedJavaGlobalRef<jobject>(j_callback)));
}

void ContextualSuggestionsBridge::ClearState(JNIEnv* env,
                                             const JavaParamRef<jobject>& obj) {
}

void ContextualSuggestionsBridge::ReportEvent(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& j_web_contents,
    jint j_event_id) {
  if (!contextual_content_suggestions_service_)
    return;

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(j_web_contents);

  ukm::SourceId ukm_source_id =
      ukm::GetSourceIdForWebContentsDocument(web_contents);

  contextual_content_suggestions_service_->ReportEvent(ukm_source_id,
                                                       j_event_id);
}

void ContextualSuggestionsBridge::OnSuggestionsAvailable(
    ScopedJavaGlobalRef<jobject> j_callback,
    std::string peek_text,
    std::vector<Cluster> clusters) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_result =
      Java_ContextualSuggestionsBridge_createContextualSuggestionsResult(
          env, ConvertUTF8ToJavaString(env, peek_text));
  for (auto& cluster : clusters) {
    Java_ContextualSuggestionsBridge_addNewClusterToResult(
        env, j_result, ConvertUTF8ToJavaString(env, cluster.title));
    for (auto& suggestion : cluster.suggestions) {
      Java_ContextualSuggestionsBridge_addSuggestionToLastCluster(
          env, j_result, ConvertUTF8ToJavaString(env, suggestion->id()),
          ConvertUTF8ToJavaString(env, suggestion->title()),
          ConvertUTF8ToJavaString(env, suggestion->publisher_name()),
          ConvertUTF8ToJavaString(env, suggestion->url().spec()));
    }
  }
  RunCallbackAndroid(j_callback, j_result);
}

void ContextualSuggestionsBridge::OnImageFetched(
    ScopedJavaGlobalRef<jobject> j_callback,
    const gfx::Image& image) {
  ScopedJavaLocalRef<jobject> j_bitmap;
  if (!image.IsEmpty())
    j_bitmap = gfx::ConvertToJavaBitmap(image.ToSkBitmap());

  RunCallbackAndroid(j_callback, j_bitmap);
}

}  // namespace contextual_suggestions
