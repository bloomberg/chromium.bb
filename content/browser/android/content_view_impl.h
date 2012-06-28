// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_CONTENT_VIEW_IMPL_H_
#define CONTENT_BROWSER_ANDROID_CONTENT_VIEW_IMPL_H_
#pragma once

#include "base/android/jni_helper.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/process.h"
#include "content/public/browser/android/content_view.h"
#include "content/public/browser/notification_observer.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/rect.h"

class ContentViewClient;
namespace content {
class RenderWidgetHostViewAndroid;

// TODO(jrg): this is a shell.  Upstream the rest.
class ContentViewImpl : public ContentView,
                        public NotificationObserver {
 public:
  ContentViewImpl(JNIEnv* env,
                  jobject obj,
                  WebContents* web_contents);
  virtual void Destroy(JNIEnv* env, jobject obj);

  // --------------------------------------------------------------------------
  // Methods called from Java via JNI
  // --------------------------------------------------------------------------

  void LoadUrlWithoutUrlSanitization(JNIEnv* env,
                                     jobject,
                                     jstring jurl,
                                     int page_transition);
  void LoadUrlWithoutUrlSanitizationWithUserAgentOverride(
      JNIEnv* env,
      jobject,
      jstring jurl,
      int page_transition,
      jstring user_agent_override);
  base::android::ScopedJavaLocalRef<jstring> GetURL(JNIEnv* env, jobject) const;
  base::android::ScopedJavaLocalRef<jstring> GetTitle(
      JNIEnv* env, jobject obj) const;
  jboolean IsIncognito(JNIEnv* env, jobject obj);
  jboolean Crashed(JNIEnv* env, jobject obj) const { return tab_crashed_; }
  jboolean CanGoBack(JNIEnv* env, jobject obj);
  jboolean CanGoForward(JNIEnv* env, jobject obj);
  jboolean CanGoToOffset(JNIEnv* env, jobject obj, jint offset);
  void GoBack(JNIEnv* env, jobject obj);
  void GoForward(JNIEnv* env, jobject obj);
  void GoToOffset(JNIEnv* env, jobject obj, jint offset);
  jdouble GetLoadProgress(JNIEnv* env, jobject obj) const;
  void StopLoading(JNIEnv* env, jobject obj);
  void Reload(JNIEnv* env, jobject obj);
  jboolean NeedsReload(JNIEnv* env, jobject obj);
  void ClearHistory(JNIEnv* env, jobject obj);

  // --------------------------------------------------------------------------
  // Public methods that call to Java via JNI
  // --------------------------------------------------------------------------

  void OnTabCrashed(const base::ProcessHandle handle);
  void SetTitle(const string16& title);
  bool HasFocus();
  void OnSelectionChanged(const std::string& text);
  void OnSelectionBoundsChanged(int startx,
                                int starty,
                                base::i18n::TextDirection start_dir,
                                int endx,
                                int endy,
                                base::i18n::TextDirection end_dir);
  void OnAcceleratedCompositingStateChange(RenderWidgetHostViewAndroid* rwhva,
                                           bool activated,
                                           bool force);

  // --------------------------------------------------------------------------
  // Methods called from native code
  // --------------------------------------------------------------------------

  gfx::Rect GetBounds() const;

  WebContents* web_contents() const { return web_contents_; }
  void LoadUrl(const GURL& url, int page_transition);
  void LoadUrlWithUserAgentOverride(
      const GURL& url,
      int page_transition,
      const std::string& user_agent_override);

  // --------------------------------------------------------------------------
 private:
  // NotificationObserver implementation.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

  // --------------------------------------------------------------------------
  // Private methods that call to Java via JNI
  // --------------------------------------------------------------------------
  virtual ~ContentViewImpl();

  // --------------------------------------------------------------------------
  // Other private methods and data
  // --------------------------------------------------------------------------

  void PostLoadUrl(const GURL& url);

  // Reference to the current WebContents used to determine how and what to
  // display in the ContentView.
  WebContents* web_contents_;

  // Whether the renderer backing this ContentView has crashed.
  bool tab_crashed_;

  DISALLOW_COPY_AND_ASSIGN(ContentViewImpl);
};

bool RegisterContentView(JNIEnv* env);

};  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_CONTENT_VIEW_IMPL_H_
