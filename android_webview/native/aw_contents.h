// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_H_
#define ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_H_

#include <jni.h>
#include <list>
#include <string>
#include <utility>

#include "android_webview/browser/aw_browser_permission_request_delegate.h"
#include "android_webview/browser/aw_message_port_message_filter.h"
#include "android_webview/browser/browser_view_renderer.h"
#include "android_webview/browser/browser_view_renderer_client.h"
#include "android_webview/browser/find_helper.h"
#include "android_webview/browser/gl_view_renderer_manager.h"
#include "android_webview/browser/icon_helper.h"
#include "android_webview/browser/renderer_host/aw_render_view_host_ext.h"
#include "android_webview/native/permission/permission_request_handler_client.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"

class SkBitmap;
class TabContents;
struct AwDrawGLInfo;

namespace content {
class WebContents;
}

namespace android_webview {

class AwContentsContainer;
class AwContentsClientBridge;
class AwPdfExporter;
class AwWebContentsDelegate;
class HardwareRenderer;
class PermissionRequestHandler;

// Native side of java-class of same name.
// Provides the ownership of and access to browser components required for
// WebView functionality; analogous to chrome's TabContents, but with a
// level of indirection provided by the AwContentsContainer abstraction.
//
// Object lifetime:
// For most purposes the java and native objects can be considered to have
// 1:1 lifetime and relationship. The exception is the java instance that
// hosts a popup will be rebound to a second native instance (carrying the
// popup content) and discard the 'default' native instance it made on
// construction. A native instance is only bound to at most one Java peer over
// its entire lifetime - see Init() and SetPendingWebContentsForPopup() for the
// construction points, and SetJavaPeers() where these paths join.
class AwContents : public FindHelper::Listener,
                   public IconHelper::Listener,
                   public AwRenderViewHostExtClient,
                   public BrowserViewRendererClient,
                   public PermissionRequestHandlerClient,
                   public AwBrowserPermissionRequestDelegate {
 public:
  // Returns the AwContents instance associated with |web_contents|, or NULL.
  static AwContents* FromWebContents(content::WebContents* web_contents);

  // Returns the AwContents instance associated with with the given
  // render_process_id and render_view_id, or NULL.
  static AwContents* FromID(int render_process_id, int render_view_id);

  AwContents(scoped_ptr<content::WebContents> web_contents);
  ~AwContents() override;

  AwRenderViewHostExt* render_view_host_ext() {
    return render_view_host_ext_.get();
  }

  // |handler| is an instance of
  // org.chromium.android_webview.AwHttpAuthHandler.
  bool OnReceivedHttpAuthRequest(const base::android::JavaRef<jobject>& handler,
                                 const std::string& host,
                                 const std::string& realm);

  void SetOffscreenPreRaster(bool enabled);

  // Methods called from Java.
  void SetJavaPeers(JNIEnv* env,
                    jobject obj,
                    jobject aw_contents,
                    jobject web_contents_delegate,
                    jobject contents_client_bridge,
                    jobject io_thread_client,
                    jobject intercept_navigation_delegate);
  base::android::ScopedJavaLocalRef<jobject> GetWebContents(JNIEnv* env,
                                                            jobject obj);

  void Destroy(JNIEnv* env, jobject obj);
  void DocumentHasImages(JNIEnv* env, jobject obj, jobject message);
  void GenerateMHTML(JNIEnv* env, jobject obj, jstring jpath, jobject callback);
  void CreatePdfExporter(JNIEnv* env, jobject obj, jobject pdfExporter);
  void AddVisitedLinks(JNIEnv* env, jobject obj, jobjectArray jvisited_links);
  base::android::ScopedJavaLocalRef<jbyteArray> GetCertificate(
      JNIEnv* env, jobject obj);
  void RequestNewHitTestDataAt(JNIEnv* env,
                               jobject obj,
                               jfloat x,
                               jfloat y,
                               jfloat touch_major);
  void UpdateLastHitTestData(JNIEnv* env, jobject obj);
  void OnSizeChanged(JNIEnv* env, jobject obj, int w, int h, int ow, int oh);
  void SetViewVisibility(JNIEnv* env, jobject obj, bool visible);
  void SetWindowVisibility(JNIEnv* env, jobject obj, bool visible);
  void SetIsPaused(JNIEnv* env, jobject obj, bool paused);
  void OnAttachedToWindow(JNIEnv* env, jobject obj, int w, int h);
  void OnDetachedFromWindow(JNIEnv* env, jobject obj);
  base::android::ScopedJavaLocalRef<jbyteArray> GetOpaqueState(
      JNIEnv* env, jobject obj);
  jboolean RestoreFromOpaqueState(JNIEnv* env, jobject obj, jbyteArray state);
  void FocusFirstNode(JNIEnv* env, jobject obj);
  void SetBackgroundColor(JNIEnv* env, jobject obj, jint color);
  bool OnDraw(JNIEnv* env,
              jobject obj,
              jobject canvas,
              jboolean is_hardware_accelerated,
              jint scroll_x,
              jint scroll_y,
              jint visible_left,
              jint visible_top,
              jint visible_right,
              jint visible_bottom);
  jlong GetAwDrawGLViewContext(JNIEnv* env, jobject obj);
  jlong CapturePicture(JNIEnv* env, jobject obj, int width, int height);
  void EnableOnNewPicture(JNIEnv* env, jobject obj, jboolean enabled);
  void InsertVisualStateCallback(JNIEnv* env,
                        jobject obj,
                        long request_id,
                        jobject callback);
  void ClearView(JNIEnv* env, jobject obj);
  void SetExtraHeadersForUrl(JNIEnv* env, jobject obj,
                             jstring url, jstring extra_headers);

  void InvokeGeolocationCallback(JNIEnv* env,
                                 jobject obj,
                                 jboolean value,
                                 jstring origin);

  // PermissionRequestHandlerClient implementation.
  void OnPermissionRequest(AwPermissionRequest* request) override;
  void OnPermissionRequestCanceled(AwPermissionRequest* request) override;

  PermissionRequestHandler* GetPermissionRequestHandler() {
    return permission_request_handler_.get();
  }

  void PreauthorizePermission(JNIEnv* env,
                              jobject obj,
                              jstring origin,
                              jlong resources);

  // AwBrowserPermissionRequestDelegate implementation.
  void RequestProtectedMediaIdentifierPermission(
      const GURL& origin,
      const base::Callback<void(bool)>& callback) override;
  void CancelProtectedMediaIdentifierPermissionRequests(
      const GURL& origin) override;
  void RequestGeolocationPermission(
      const GURL& origin,
      const base::Callback<void(bool)>& callback) override;
  void CancelGeolocationPermissionRequests(const GURL& origin) override;

  // Find-in-page API and related methods.
  void FindAllAsync(JNIEnv* env, jobject obj, jstring search_string);
  void FindNext(JNIEnv* env, jobject obj, jboolean forward);
  void ClearMatches(JNIEnv* env, jobject obj);
  FindHelper* GetFindHelper();

  // Per WebView Cookie Policy
  bool AllowThirdPartyCookies();

  // FindHelper::Listener implementation.
  void OnFindResultReceived(int active_ordinal,
                            int match_count,
                            bool finished) override;
  // IconHelper::Listener implementation.
  bool ShouldDownloadFavicon(const GURL& icon_url) override;
  void OnReceivedIcon(const GURL& icon_url, const SkBitmap& bitmap) override;
  void OnReceivedTouchIconUrl(const std::string& url,
                              const bool precomposed) override;

  // AwRenderViewHostExtClient implementation.
  void OnWebLayoutPageScaleFactorChanged(float page_scale_factor) override;
  void OnWebLayoutContentsSizeChanged(const gfx::Size& contents_size) override;

  // BrowserViewRendererClient implementation.
  bool RequestDrawGL(bool wait_for_completion) override;
  void PostInvalidate() override;
  void DetachFunctorFromView() override;
  void OnNewPicture() override;
  gfx::Point GetLocationOnScreen() override;
  void ScrollContainerViewTo(gfx::Vector2d new_value) override;
  bool IsFlingActive() const override;
  void UpdateScrollState(gfx::Vector2d max_scroll_offset,
                         gfx::SizeF contents_size_dip,
                         float page_scale_factor,
                         float min_page_scale_factor,
                         float max_page_scale_factor) override;
  void DidOverscroll(gfx::Vector2d overscroll_delta) override;
  void ParentDrawConstraintsUpdated(
      const ParentCompositorDrawConstraints& draw_constraints) override {}

  void ClearCache(JNIEnv* env, jobject obj, jboolean include_disk_files);
  void SetPendingWebContentsForPopup(scoped_ptr<content::WebContents> pending);
  jlong ReleasePopupAwContents(JNIEnv* env, jobject obj);

  void ScrollTo(JNIEnv* env, jobject obj, jint x, jint y);
  void SetDipScale(JNIEnv* env, jobject obj, jfloat dip_scale);
  void SetSaveFormData(bool enabled);

  // Sets the java client
  void SetAwAutofillClient(jobject client);

  void SetJsOnlineProperty(JNIEnv* env, jobject obj, jboolean network_up);
  void TrimMemory(JNIEnv* env, jobject obj, jint level, jboolean visible);

  scoped_refptr<AwMessagePortMessageFilter> GetMessagePortMessageFilter();
  void PostMessageToFrame(JNIEnv* env, jobject obj, jstring frame_id,
      jstring message, jstring target_origin, jintArray sent_ports);
  void CreateMessageChannel(JNIEnv* env, jobject obj, jobjectArray ports);

 private:
  void InitDataReductionProxyIfNecessary();
  void InitAutofillIfNecessary(bool enabled);

  // Geolocation API support
  void ShowGeolocationPrompt(const GURL& origin, base::Callback<void(bool)>);
  void HideGeolocationPrompt(const GURL& origin);

  JavaObjectWeakGlobalRef java_ref_;
  scoped_ptr<AwWebContentsDelegate> web_contents_delegate_;
  scoped_ptr<AwContentsClientBridge> contents_client_bridge_;
  scoped_ptr<content::WebContents> web_contents_;
  scoped_ptr<AwRenderViewHostExt> render_view_host_ext_;
  scoped_ptr<FindHelper> find_helper_;
  scoped_ptr<IconHelper> icon_helper_;
  scoped_ptr<AwContents> pending_contents_;
  BrowserViewRenderer browser_view_renderer_;
  scoped_ptr<AwPdfExporter> pdf_exporter_;
  scoped_ptr<PermissionRequestHandler> permission_request_handler_;
  scoped_refptr<AwMessagePortMessageFilter> message_port_message_filter_;

  // GURL is supplied by the content layer as requesting frame.
  // Callback is supplied by the content layer, and is invoked with the result
  // from the permission prompt.
  typedef std::pair<const GURL, base::Callback<void(bool)> > OriginCallback;
  // The first element in the list is always the currently pending request.
  std::list<OriginCallback> pending_geolocation_prompts_;

  GLViewRendererManager::Key renderer_manager_key_;

  DISALLOW_COPY_AND_ASSIGN(AwContents);
};

bool RegisterAwContents(JNIEnv* env);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_H_
