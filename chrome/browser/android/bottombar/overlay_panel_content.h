// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BOTTOMBAR_OVERLAY_PANEL_CONTENT_H_
#define CHROME_BROWSER_ANDROID_BOTTOMBAR_OVERLAY_PANEL_CONTENT_H_

#include "base/android/jni_android.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/android/contextualsearch/contextual_search_context.h"

namespace content {
class WebContents;
}  // namespace content

namespace web_contents_delegate_android {
class WebContentsDelegateAndroid;
}  // namespace web_contents_delegate_android

// Manages the native extraction and request logic for Contextual Search,
// and interacts with the Java OverlayPanelContent for UX.
// Most of the work is done by the associated ContextualSearchDelegate.
class OverlayPanelContent {
 public:
  // Constructs a native manager associated with the Java manager.
  OverlayPanelContent(JNIEnv* env, jobject obj);
  virtual ~OverlayPanelContent();

  // Called by the Java OverlayPanelContent when it is being destroyed.
  void Destroy(JNIEnv* env, jobject obj);

  // Removes a search URL from history. |search_start_time_ms| represents the
  // time at which |search_url| was committed.
  void RemoveLastHistoryEntry(JNIEnv* env,
                              jobject obj,
                              jstring search_url,
                              jlong search_start_time_ms);

  // Takes ownership of the WebContents associated with the given
  // |ContentViewCore| which holds the panel content.
  void SetWebContents(JNIEnv* env, jobject obj, jobject jcontent_view_core,
                      jobject jweb_contents_delegate);

  // Destroys the WebContents.
  void DestroyWebContents(JNIEnv* env, jobject jobj);

  // Sets the delegate used to convert navigations to intents.
  void SetInterceptNavigationDelegate(JNIEnv* env,
                                      jobject obj,
                                      jobject delegate,
                                      jobject jweb_contents);

 private:
  // Our global reference to the Java OverlayPanelContent.
  base::android::ScopedJavaGlobalRef<jobject> java_manager_;

  // Used if we need to clear history.
  base::CancelableTaskTracker history_task_tracker_;

  // The WebContents that holds the panel content.
  scoped_ptr<content::WebContents> web_contents_;
  scoped_ptr<web_contents_delegate_android::WebContentsDelegateAndroid>
      web_contents_delegate_;

  DISALLOW_COPY_AND_ASSIGN(OverlayPanelContent);
};

bool RegisterOverlayPanelContent(JNIEnv* env);

#endif  // CHROME_BROWSER_ANDROID_BOTTOMBAR_OVERLAY_PANEL_CONTENT_H_
