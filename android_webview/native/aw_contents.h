// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_H_
#define ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_H_

#include <jni.h>
#include <list>
#include <string>
#include <utility>

#include "android_webview/browser/browser_view_renderer.h"
#include "android_webview/browser/find_helper.h"
#include "android_webview/browser/icon_helper.h"
#include "android_webview/browser/renderer_host/aw_render_view_host_ext.h"
#include "base/android/scoped_java_ref.h"
#include "base/android/jni_helper.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/javascript_dialog_manager.h"

class SkBitmap;
class TabContents;

namespace content {
class WebContents;
}

namespace android_webview {

class AwContentsContainer;
class AwWebContentsDelegate;

// Native side of java-class of same name.
// Provides the ownership of and access to browser components required for
// WebView functionality; analogous to chrome's TabContents, but with a
// level of indirection provided by the AwContentsContainer abstraction.
class AwContents : public FindHelper::Listener,
                   public IconHelper::Listener,
                   public BrowserViewRenderer::Client {
 public:
  enum OnNewPictureMode {
    kOnNewPictureDisabled = 0,
    kOnNewPictureEnabled,
    kOnNewPictureInvalidationOnly,
  };

  // Returns the AwContents instance associated with |web_contents|, or NULL.
  static AwContents* FromWebContents(content::WebContents* web_contents);

  // Returns the AwContents instance associated with with the given
  // render_process_id and render_view_id, or NULL.
  static AwContents* FromID(int render_process_id, int render_view_id);

  AwContents(JNIEnv* env,
             jobject obj,
             jobject web_contents_delegate);
  virtual ~AwContents();

  AwRenderViewHostExt* render_view_host_ext() {
    return render_view_host_ext_.get();
  }

  void RunJavaScriptDialog(
      content::JavaScriptMessageType message_type,
      const GURL& origin_url,
      const string16& message_text,
      const string16& default_prompt_text,
      const base::android::ScopedJavaLocalRef<jobject>& js_result);

  void RunBeforeUnloadDialog(
      const GURL& origin_url,
      const string16& message_text,
      const base::android::ScopedJavaLocalRef<jobject>& js_result);

  void PerformLongClick();

  // |handler| is an instance of
  // org.chromium.android_webview.AwHttpAuthHandler.
  void OnReceivedHttpAuthRequest(const base::android::JavaRef<jobject>& handler,
                                 const std::string& host,
                                 const std::string& realm);

  // Methods called from Java.
  jint GetWebContents(JNIEnv* env, jobject obj);
  void SetWebContents(JNIEnv* env, jobject obj, jint web_contents);

  void DidInitializeContentViewCore(JNIEnv* env, jobject obj,
                                    jint content_view_core);
  void Destroy(JNIEnv* env, jobject obj);
  void DocumentHasImages(JNIEnv* env, jobject obj, jobject message);
  void GenerateMHTML(JNIEnv* env, jobject obj, jstring jpath, jobject callback);
  void SetIoThreadClient(JNIEnv* env, jobject obj, jobject client);
  void SetInterceptNavigationDelegate(JNIEnv* env, jobject obj,
                                      jobject delegate);
  void AddVisitedLinks(JNIEnv* env, jobject obj, jobjectArray jvisited_links);
  base::android::ScopedJavaLocalRef<jbyteArray> GetCertificate(
      JNIEnv* env, jobject obj);
  void RequestNewHitTestDataAt(JNIEnv* env, jobject obj, jint x, jint y);
  void UpdateLastHitTestData(JNIEnv* env, jobject obj);
  void OnSizeChanged(JNIEnv* env, jobject obj, int w, int h, int ow, int oh);
  void SetWindowViewVisibility(JNIEnv* env, jobject obj,
                               bool window_visible,
                               bool view_visible);
  void OnAttachedToWindow(JNIEnv* env, jobject obj, int w, int h);
  void OnDetachedFromWindow(JNIEnv* env, jobject obj);
  base::android::ScopedJavaLocalRef<jbyteArray> GetOpaqueState(
      JNIEnv* env, jobject obj);
  jboolean RestoreFromOpaqueState(JNIEnv* env, jobject obj, jbyteArray state);
  void FocusFirstNode(JNIEnv* env, jobject obj);
  bool DrawSW(JNIEnv* env,
              jobject obj,
              jobject canvas,
              jint clip_x,
              jint clip_y,
              jint clip_w,
              jint clip_h);
  void SetScrollForHWFrame(JNIEnv* env, jobject obj,
                           int scroll_x, int scroll_y);
  jint GetAwDrawGLViewContext(JNIEnv* env, jobject obj);
  base::android::ScopedJavaLocalRef<jobject> CapturePicture(JNIEnv* env,
                                                            jobject obj);
  void EnableOnNewPicture(JNIEnv* env,
                          jobject obj,
                          jboolean enabled,
                          jboolean invalidation_only);

  // Geolocation API support
  void ShowGeolocationPrompt(const GURL& origin, base::Callback<void(bool)>);
  void HideGeolocationPrompt(const GURL& origin);
  void InvokeGeolocationCallback(JNIEnv* env,
                                 jobject obj,
                                 jboolean value,
                                 jstring origin);

  // Find-in-page API and related methods.
  jint FindAllSync(JNIEnv* env, jobject obj, jstring search_string);
  void FindAllAsync(JNIEnv* env, jobject obj, jstring search_string);
  void FindNext(JNIEnv* env, jobject obj, jboolean forward);
  void ClearMatches(JNIEnv* env, jobject obj);
  FindHelper* GetFindHelper();

  // FindHelper::Listener implementation.
  virtual void OnFindResultReceived(int active_ordinal,
                                    int match_count,
                                    bool finished) OVERRIDE;
  // IconHelper::Listener implementation.
  virtual void OnReceivedIcon(const SkBitmap& bitmap) OVERRIDE;
  virtual void OnReceivedTouchIconUrl(const std::string& url,
                                      const bool precomposed) OVERRIDE;

  // BrowserViewRenderer::Client implementation.
  virtual void Invalidate() OVERRIDE;
  virtual void OnNewPicture(
      const base::android::JavaRef<jobject>& picture) OVERRIDE;

  void ClearCache(JNIEnv* env, jobject obj, jboolean include_disk_files);
  void SetPendingWebContentsForPopup(scoped_ptr<content::WebContents> pending);
  jint ReleasePopupWebContents(JNIEnv* env, jobject obj);

  void ResetScrollAndScaleState(JNIEnv* env, jobject obj);

 private:
  void SetWebContents(content::WebContents* web_contents);

  JavaObjectWeakGlobalRef java_ref_;
  scoped_ptr<content::WebContents> web_contents_;
  scoped_ptr<AwWebContentsDelegate> web_contents_delegate_;
  scoped_ptr<AwRenderViewHostExt> render_view_host_ext_;
  scoped_ptr<FindHelper> find_helper_;
  scoped_ptr<IconHelper> icon_helper_;
  scoped_ptr<content::WebContents> pending_contents_;
  scoped_ptr<BrowserViewRenderer> browser_view_renderer_;

  // GURL is supplied by the content layer as requesting frame.
  // Callback is supplied by the content layer, and is invoked with the result
  // from the permission prompt.
  typedef std::pair<const GURL, base::Callback<void(bool)> > OriginCallback;
  // The first element in the list is always the currently pending request.
  std::list<OriginCallback> pending_geolocation_prompts_;

  DISALLOW_COPY_AND_ASSIGN(AwContents);
};

bool RegisterAwContents(JNIEnv* env);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_H_
