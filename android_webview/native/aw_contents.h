// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_H_
#define ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_H_

#include <jni.h>
#include <string>

#include "android_webview/browser/find_helper.h"
#include "android_webview/browser/icon_helper.h"
#include "android_webview/browser/renderer_host/aw_render_view_host_ext.h"
#include "android_webview/public/browser/draw_gl.h"
#include "base/android/scoped_java_ref.h"
#include "base/android/jni_helper.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkPicture.h"

typedef void* EGLContext;
class SkBitmap;
class TabContents;

namespace cc {
class Layer;
}

namespace content {
class Compositor;
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
                   public content::Compositor::Client,
                   public AwRenderViewHostExt::Client {
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

  void DrawGL(AwDrawGLInfo* draw_info);
  bool DrawSW(JNIEnv* env,
              jobject obj,
              jobject canvas,
              jint clip_x,
              jint clip_y,
              jint clip_w,
              jint clip_h);

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
  void SetScrollForHWFrame(JNIEnv* env, jobject obj,
                           int scroll_x, int scroll_y);
  void FocusFirstNode(JNIEnv* env, jobject obj);
  base::android::ScopedJavaLocalRef<jobject> CapturePicture(JNIEnv* env,
                                                            jobject obj);
  void EnableOnNewPicture(JNIEnv* env,
                          jobject obj,
                          jboolean enabled,
                          jboolean invalidation_only);

  // Geolocation API support
  void OnGeolocationShowPrompt(int render_process_id,
                             int render_view_id,
                             int bridge_id,
                             const GURL& requesting_frame);
  void OnGeolocationHidePrompt();

  // Find-in-page API and related methods.
  jint FindAllSync(JNIEnv* env, jobject obj, jstring search_string);
  void FindAllAsync(JNIEnv* env, jobject obj, jstring search_string);
  void FindNext(JNIEnv* env, jobject obj, jboolean forward);
  void ClearMatches(JNIEnv* env, jobject obj);
  void ClearCache(JNIEnv* env, jobject obj, jboolean include_disk_files);

  FindHelper* GetFindHelper();

  // FindHelper::Listener implementation.
  virtual void OnFindResultReceived(int active_ordinal,
                                    int match_count,
                                    bool finished) OVERRIDE;
  // IconHelper::Listener implementation.
  virtual void OnReceivedIcon(const SkBitmap& bitmap) OVERRIDE;
  virtual void OnReceivedTouchIconUrl(const std::string& url,
                                      const bool precomposed) OVERRIDE;

  // content::Compositor::Client implementation.
  virtual void ScheduleComposite() OVERRIDE;
  virtual void OnSwapBuffersCompleted() OVERRIDE;

  void SetPendingWebContentsForPopup(scoped_ptr<content::WebContents> pending);
  jint ReleasePopupWebContents(JNIEnv* env, jobject obj);

  // AwRenderViewHostExt::Client implementation.
  virtual void OnPictureUpdated(int process_id, int render_view_id) OVERRIDE;

  // Returns the latest locally available picture if any.
  // If none is available will synchronously request the latest one
  // and block until the result is received.
  skia::RefPtr<SkPicture> GetLastCapturedPicture();

 private:
  void Invalidate();
  void SetWebContents(content::WebContents* web_contents);
  void SetCompositorVisibility(bool visible);
  void ResetCompositor();
  void AttachLayerTree();
  bool RenderSW(SkCanvas* canvas);
  bool RenderPicture(SkCanvas* canvas);

  JavaObjectWeakGlobalRef java_ref_;
  scoped_ptr<content::WebContents> web_contents_;
  scoped_ptr<AwWebContentsDelegate> web_contents_delegate_;
  scoped_ptr<AwRenderViewHostExt> render_view_host_ext_;
  scoped_ptr<FindHelper> find_helper_;
  scoped_ptr<IconHelper> icon_helper_;
  scoped_ptr<content::WebContents> pending_contents_;

  // Compositor-specific state.
  scoped_ptr<content::Compositor> compositor_;
  scoped_refptr<cc::Layer> scissor_clip_layer_;
  scoped_refptr<cc::Layer> transform_layer_;
  scoped_refptr<cc::Layer> view_clip_layer_;
  gfx::Point hw_rendering_scroll_;
  gfx::Size view_size_;
  bool view_visible_;
  bool compositor_visible_;
  bool is_composite_pending_;
  float dpi_scale_;
  OnNewPictureMode on_new_picture_mode_;

  // Used only for detecting Android View System context changes.
  // Not to be used between draw calls.
  EGLContext last_frame_context_;

  DISALLOW_COPY_AND_ASSIGN(AwContents);
};

bool RegisterAwContents(JNIEnv* env);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_H_
