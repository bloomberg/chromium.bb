// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_ANDROID_H_
#define CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/supports_user_data.h"
#include "content/browser/frame_host/navigation_controller_android.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/common/content_export.h"

namespace content {

class SynchronousCompositorClient;
class WebContents;

// Android wrapper around WebContents that provides safer passage from java and
// back to native and provides java with a means of communicating with its
// native counterpart.
class CONTENT_EXPORT WebContentsAndroid
    : public base::SupportsUserData::Data {
 public:
  static bool Register(JNIEnv* env);

  explicit WebContentsAndroid(WebContents* web_contents);
  ~WebContentsAndroid() override;

  WebContents* web_contents() const { return web_contents_; }

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  // Methods called from Java
  base::android::ScopedJavaLocalRef<jstring> GetTitle(JNIEnv* env,
                                                      jobject obj) const;
  base::android::ScopedJavaLocalRef<jstring> GetVisibleURL(JNIEnv* env,
                                                           jobject obj) const;

  bool IsLoading(JNIEnv* env, jobject obj) const;
  bool IsLoadingToDifferentDocument(JNIEnv* env, jobject obj) const;

  void Stop(JNIEnv* env, jobject obj);
  void Cut(JNIEnv* env, jobject obj);
  void Copy(JNIEnv* env, jobject obj);
  void Paste(JNIEnv* env, jobject obj);
  void Replace(JNIEnv* env, jobject obj, jstring jstr);
  void SelectAll(JNIEnv* env, jobject obj);
  void Unselect(JNIEnv* env, jobject obj);
  jint GetBackgroundColor(JNIEnv* env, jobject obj);
  base::android::ScopedJavaLocalRef<jstring> GetURL(JNIEnv* env, jobject) const;
  base::android::ScopedJavaLocalRef<jstring> GetLastCommittedURL(JNIEnv* env,
                                                                 jobject) const;
  jboolean IsIncognito(JNIEnv* env, jobject obj);

  void ResumeLoadingCreatedWebContents(JNIEnv* env, jobject obj);

  void OnHide(JNIEnv* env, jobject obj);
  void OnShow(JNIEnv* env, jobject obj);
  void ReleaseMediaPlayers(JNIEnv* env, jobject jobj);

  void ShowInterstitialPage(
      JNIEnv* env, jobject obj, jstring jurl, jlong delegate_ptr);
  jboolean IsShowingInterstitialPage(JNIEnv* env, jobject obj);
  jboolean FocusLocationBarByDefault(JNIEnv* env, jobject obj);
  jboolean IsRenderWidgetHostViewReady(JNIEnv* env, jobject obj);
  void ExitFullscreen(JNIEnv* env, jobject obj);
  void UpdateTopControlsState(
      JNIEnv* env,
      jobject obj,
      bool enable_hiding,
      bool enable_showing,
      bool animate);
  void ShowImeIfNeeded(JNIEnv* env, jobject obj);
  void ScrollFocusedEditableNodeIntoView(JNIEnv* env, jobject obj);
  void SelectWordAroundCaret(JNIEnv* env, jobject obj);
  void AdjustSelectionByCharacterOffset(JNIEnv* env,
                                        jobject obj,
                                        jint start_adjust,
                                        jint end_adjust);
  void InsertCSS(JNIEnv* env, jobject jobj, jstring jcss);
  void EvaluateJavaScript(JNIEnv* env,
                          jobject obj,
                          jstring script,
                          jobject callback);
  void EvaluateJavaScriptForTests(JNIEnv* env,
                                  jobject obj,
                                  jstring script,
                                  jobject callback);

  void AddMessageToDevToolsConsole(JNIEnv* env,
                                   jobject jobj,
                                   jint level,
                                   jstring message);

  void SendMessageToFrame(JNIEnv* env,
                          jobject obj,
                          jstring frame_name,
                          jstring message,
                          jstring target_origin);

  jboolean HasAccessedInitialDocument(JNIEnv* env, jobject jobj);

  jint GetThemeColor(JNIEnv* env, jobject obj);

  void RequestAccessibilitySnapshot(JNIEnv* env,
                                    jobject obj,
                                    jobject callback,
                                    jfloat y_offset,
                                    jfloat x_scroll);

  void ResumeMediaSession(JNIEnv* env, jobject obj);
  void SuspendMediaSession(JNIEnv* env, jobject obj);
  void StopMediaSession(JNIEnv* env, jobject obj);

  base::android::ScopedJavaLocalRef<jstring> GetEncoding(JNIEnv* env,
                                                         jobject obj) const;

  void set_synchronous_compositor_client(SynchronousCompositorClient* client) {
    synchronous_compositor_client_ = client;
  }
  SynchronousCompositorClient* synchronous_compositor_client() const {
    return synchronous_compositor_client_;
  }

 private:
  RenderWidgetHostViewAndroid* GetRenderWidgetHostViewAndroid();

  WebContents* web_contents_;
  NavigationControllerAndroid navigation_controller_;
  base::android::ScopedJavaGlobalRef<jobject> obj_;
  SynchronousCompositorClient* synchronous_compositor_client_;

  base::WeakPtrFactory<WebContentsAndroid> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsAndroid);
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_WEB_CONTENTS_ANDROID_H_
