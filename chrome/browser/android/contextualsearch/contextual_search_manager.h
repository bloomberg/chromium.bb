// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_MANAGER_H_
#define CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_MANAGER_H_

#include "base/android/jni_android.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/android/contextualsearch/contextual_search_context.h"
#include "chrome/browser/android/contextualsearch/contextual_search_delegate.h"

namespace content {
class WebContents;
}  // namespace content

namespace web_contents_delegate_android {
class WebContentsDelegateAndroid;
}  // namespace web_contents_delegate_android

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

  // Removes a search URL from history. |search_start_time_ms| represents the
  // time at which |search_url| was committed.
  void RemoveLastSearchVisit(JNIEnv* env,
                             jobject obj,
                             jstring search_url,
                             jlong search_start_time_ms);

  // Takes ownership of the WebContents associated with the given
  // |ContentViewCore| which holds the Contextual Search Results.
  void SetWebContents(JNIEnv* env, jobject obj, jobject jcontent_view_core,
                      jobject jweb_contents_delegate);

  // Destroys the WebContents.
  void DestroyWebContents(JNIEnv* env, jobject jobj);

  // Release ownership of WebContents.
  void ReleaseWebContents(JNIEnv* env, jobject jobj);

  // Destroys the WebContents of a ContentViewCore.
  void DestroyWebContentsFromContentViewCore(JNIEnv* env,
                                             jobject jobj,
                                             jobject jcontent_view_core);

  // Sets the delegate used to convert navigations to intents.
  void SetInterceptNavigationDelegate(JNIEnv* env,
                                      jobject obj,
                                      jobject delegate,
                                      jobject jweb_contents);

 private:
  // TODO(donnd): encapsulate these response parameters?
  void OnSearchTermResolutionResponse(bool is_invalid,
                                      int response_code,
                                      const std::string& search_term,
                                      const std::string& display_text,
                                      const std::string& alternate_term,
                                      bool prevent_preload);

  // Calls back to Java with the surrounding text to be displayed.
  void OnSurroundingTextAvailable(const std::string& before_text,
                                  const std::string& after_text);

  // Calls back to Java with notification for Icing selection.
  void OnIcingSelectionAvailable(const std::string& encoding,
                                 const base::string16& surrounding_text,
                                 size_t start_offset,
                                 size_t end_offset);

  // Our global reference to the Java ContextualSearchManager.
  base::android::ScopedJavaGlobalRef<jobject> java_manager_;

  // The delegate we're using the do the real work.
  scoped_ptr<ContextualSearchDelegate> delegate_;

  // Used if we need to clear history.
  base::CancelableTaskTracker history_task_tracker_;

  // The WebContents that holds the Contextual Search Results.
  scoped_ptr<content::WebContents> web_contents_;
  scoped_ptr<web_contents_delegate_android::WebContentsDelegateAndroid>
      web_contents_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ContextualSearchManager);
};

bool RegisterContextualSearchManager(JNIEnv* env);

#endif  // CHROME_BROWSER_ANDROID_CONTEXTUALSEARCH_CONTEXTUAL_SEARCH_MANAGER_H_
