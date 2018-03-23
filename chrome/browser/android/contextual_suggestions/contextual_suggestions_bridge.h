// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CONTEXTUAL_SUGGESTIONS_CONTEXTUAL_SUGGESTIONS_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_CONTEXTUAL_SUGGESTIONS_CONTEXTUAL_SUGGESTIONS_BRIDGE_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/ntp_snippets/contextual/contextual_content_suggestions_service.h"

namespace contextual_suggestions {

class ContextualSuggestionsBridge
    : public ntp_snippets::ContextualContentSuggestionsService::Delegate {
 public:
  ContextualSuggestionsBridge(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& j_bridge,
      const base::android::JavaParamRef<jobject>& j_profile);

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  void FetchSuggestionImage(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& j_suggestion_id,
      const base::android::JavaParamRef<jobject>& j_callback);

  void FetchSuggestionFavicon(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& j_suggestion_id,
      const base::android::JavaParamRef<jobject>& j_callback);

  void ReportEvent(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   jint j_event_id);

  // ContextualContentSuggestionsService::Delegate implementation:
  void OnSuggestionsAvailable(
      std::vector<ntp_snippets::ContextualContentSuggestionsService::Cluster>
          clusters) override;
  void OnStateCleared() override;

 private:
  ~ContextualSuggestionsBridge() override;

  ntp_snippets::ContextualContentSuggestionsService*
      contextual_content_suggestions_service_;

  // The Java SnippetsBridge.
  base::android::ScopedJavaGlobalRef<jobject> bridge_;

  base::WeakPtrFactory<ContextualSuggestionsBridge> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ContextualSuggestionsBridge);
};

}  // namespace contextual_suggestions

#endif  // CHROME_BROWSER_ANDROID_CONTEXTUAL_SUGGESTIONS_CONTEXTUAL_SUGGESTIONS_BRIDGE_H_
