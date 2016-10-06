// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_NTP_NTP_SNIPPETS_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_NTP_NTP_SNIPPETS_BRIDGE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/history/core/browser/history_service.h"
#include "components/ntp_snippets/category.h"
#include "components/ntp_snippets/category_status.h"
#include "components/ntp_snippets/content_suggestions_service.h"

namespace gfx {
class Image;
}

// The C++ counterpart to SnippetsBridge.java. Enables Java code to access
// the list of snippets to show on the NTP
class NTPSnippetsBridge
    : public ntp_snippets::ContentSuggestionsService::Observer {
 public:
  NTPSnippetsBridge(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& j_profile);
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  void SetObserver(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   const base::android::JavaParamRef<jobject>& j_observer);

  base::android::ScopedJavaLocalRef<jintArray> GetCategories(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  int GetCategoryStatus(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& obj,
                        jint category);

  base::android::ScopedJavaLocalRef<jobject> GetCategoryInfo(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint category);

  base::android::ScopedJavaLocalRef<jobject> GetSuggestionsForCategory(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint category);

  void FetchSuggestionImage(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint category,
      const base::android::JavaParamRef<jstring>& id_within_category,
      const base::android::JavaParamRef<jobject>& j_callback);

  void DismissSuggestion(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& jurl,
      jint global_position,
      jint category,
      jint category_position,
      const base::android::JavaParamRef<jstring>& id_within_category);

  void DismissCategory(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       jint category);

  // Checks if the URL has been visited. The callback will not be called
  // synchronously.
  void GetURLVisited(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj,
                     const base::android::JavaParamRef<jobject>& callback,
                     const base::android::JavaParamRef<jstring>& jurl);

  void OnPageShown(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jintArray>& jcategories,
      const base::android::JavaParamRef<jintArray>& jsuggestions_per_category);

  void OnSuggestionShown(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         jint global_position,
                         jint category,
                         jint category_position,
                         jlong publish_timestamp_ms,
                         jfloat score);

  void OnSuggestionOpened(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          jint global_position,
                          jint category,
                          jint category_position,
                          jlong publish_timestamp_ms,
                          jfloat score,
                          int windowOpenDisposition);

  void OnSuggestionMenuOpened(JNIEnv* env,
                              const base::android::JavaParamRef<jobject>& obj,
                              jint global_position,
                              jint category,
                              jint category_position,
                              jlong publish_timestamp_ms,
                              jfloat score);

  void OnMoreButtonShown(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         jint category,
                         jint position);

  void OnMoreButtonClicked(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           jint category,
                           jint position);

  static bool Register(JNIEnv* env);

 private:
  ~NTPSnippetsBridge() override;

  // ContentSuggestionsService::Observer overrides
  void OnNewSuggestions(ntp_snippets::Category category) override;
  void OnCategoryStatusChanged(
      ntp_snippets::Category category,
      ntp_snippets::CategoryStatus new_status) override;
  void OnSuggestionInvalidated(
      const ntp_snippets::ContentSuggestion::ID& suggestion_id) override;
  void ContentSuggestionsServiceShutdown() override;

  void OnImageFetched(base::android::ScopedJavaGlobalRef<jobject> callback,
                      const gfx::Image& image);

  ntp_snippets::Category CategoryFromIDValue(jint id);

  ntp_snippets::ContentSuggestionsService* content_suggestions_service_;
  history::HistoryService* history_service_;
  base::CancelableTaskTracker tracker_;

  ScopedObserver<ntp_snippets::ContentSuggestionsService,
                 ntp_snippets::ContentSuggestionsService::Observer>
      content_suggestions_service_observer_;

  // Used to notify the Java side when new snippets have been fetched.
  base::android::ScopedJavaGlobalRef<jobject> observer_;

  base::WeakPtrFactory<NTPSnippetsBridge> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NTPSnippetsBridge);
};

#endif  // CHROME_BROWSER_ANDROID_NTP_NTP_SNIPPETS_BRIDGE_H_
