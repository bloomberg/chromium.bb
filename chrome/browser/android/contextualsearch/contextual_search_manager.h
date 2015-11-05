// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_MANAGER_H_
#define CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_MANAGER_H_

#include "base/android/jni_android.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/android/contextualsearch/contextual_search_context.h"
#include "chrome/browser/android/contextualsearch/contextual_search_delegate.h"

// Manages the native extraction and request logic for Contextual Search,
// and interacts with the Java ContextualSearchManager for UX.
// Most of the work is done by the associated ContextualSearchDelegate.
class ContextualSearchManager {
 public:
  // Constructs a native manager associated with the Java manager.
  ContextualSearchManager(JNIEnv* env, jobject obj);
  virtual ~ContextualSearchManager();

  // Called by the Java ContextualSearchManager when it is being destroyed.
  void Destroy(JNIEnv* env, jobject obj);

  // Starts the request to get the search terms to use for the given selection,
  // by accessing our server with the content of the page (from the given
  // content view core object).
  // Any outstanding server requests are canceled.
  // When the server responds with the search term, the Java object is notified
  // by
  // calling OnSearchTermResolutionResponse().
  void StartSearchTermResolutionRequest(
      JNIEnv* env,
      jobject obj,
      jstring j_selection,
      jboolean j_use_resolved_search_term,
      jobject j_base_content_view_core,
      jboolean j_may_send_base_page_url);

  // Gathers the surrounding text around the selection and saves it locally.
  // Does not send a search term resolution request to the server.
  void GatherSurroundingText(JNIEnv* env,
                             jobject obj,
                             jstring j_selection,
                             jboolean j_use_resolved_search_term,
                             jobject j_base_content_view_core,
                             jboolean j_may_send_base_page_url);

  // Gets the target language for translation purposes.
  base::android::ScopedJavaLocalRef<jstring> GetTargetLanguage(JNIEnv* env,
                                                               jobject obj);

  // Gets the accept-languages preference string.
  base::android::ScopedJavaLocalRef<jstring> GetAcceptLanguages(JNIEnv* env,
                                                                jobject obj);

 private:
  void OnSearchTermResolutionResponse(
      const ResolvedSearchTerm& resolved_search_term);

  // Calls back to Java with the surrounding text to be displayed.
  void OnSurroundingTextAvailable(const std::string& after_text);

  // Calls back to Java with notification for Icing selection.
  void OnIcingSelectionAvailable(const std::string& encoding,
                                 const base::string16& surrounding_text,
                                 size_t start_offset,
                                 size_t end_offset);

  // Our global reference to the Java ContextualSearchManager.
  base::android::ScopedJavaGlobalRef<jobject> java_manager_;

  // The delegate we're using the do the real work.
  scoped_ptr<ContextualSearchDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(ContextualSearchManager);
};

bool RegisterContextualSearchManager(JNIEnv* env);

#endif  // CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_MANAGER_H_
