// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_H_
#define ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_H_

#include <jni.h>

#include <list>
#include <memory>
#include <string>
#include <utility>

#include "android_webview/browser/aw_browser_permission_request_delegate.h"
#include "android_webview/browser/aw_render_process_gone_delegate.h"
#include "android_webview/browser/aw_safe_browsing_ui_manager.h"
#include "android_webview/browser/browser_view_renderer.h"
#include "android_webview/browser/browser_view_renderer_client.h"
#include "android_webview/browser/find_helper.h"
#include "android_webview/browser/gl_view_renderer_manager.h"
#include "android_webview/browser/icon_helper.h"
#include "android_webview/browser/render_thread_manager.h"
#include "android_webview/browser/render_thread_manager_client.h"
#include "android_webview/browser/renderer_host/aw_render_view_host_ext.h"
#include "android_webview/native/aw_renderer_priority_manager.h"
#include "android_webview/native/permission/permission_request_handler_client.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback_forward.h"
#include "base/macros.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/web_contents_observer.h"

class SkBitmap;
class TabContents;

namespace content {
class WebContents;
}

namespace android_webview {

class AwContentsContainer;
class AwContentsClientBridge;
class AwGLFunctor;
class AwPdfExporter;
class AwWebContentsDelegate;
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
                   public AwBrowserPermissionRequestDelegate,
                   public AwRenderProcessGoneDelegate,
                   public content::WebContentsObserver,
                   public content::RenderProcessHostObserver,
                   public AwSafeBrowsingUIManager::UIManagerClient {
 public:
  // Returns the AwContents instance associated with |web_contents|, or NULL.
  static AwContents* FromWebContents(content::WebContents* web_contents);

  // Returns the AwContents instance associated with with the given
  // render_process_id and render_view_id, or NULL.
  static AwContents* FromID(int render_process_id, int render_view_id);

  static std::string GetLocale();

  static std::string GetLocaleList();

  AwContents(std::unique_ptr<content::WebContents> web_contents);
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
  void SetJavaPeers(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& aw_contents,
      const base::android::JavaParamRef<jobject>& web_contents_delegate,
      const base::android::JavaParamRef<jobject>& contents_client_bridge,
      const base::android::JavaParamRef<jobject>& io_thread_client,
      const base::android::JavaParamRef<jobject>&
          intercept_navigation_delegate);
  base::android::ScopedJavaLocalRef<jobject> GetWebContents(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void SetAwGLFunctor(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj,
                      jlong gl_functor);

  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void DocumentHasImages(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         const base::android::JavaParamRef<jobject>& message);
  void GenerateMHTML(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj,
                     const base::android::JavaParamRef<jstring>& jpath,
                     const base::android::JavaParamRef<jobject>& callback);
  void CreatePdfExporter(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& pdfExporter);
  void AddVisitedLinks(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobjectArray>& jvisited_links);
  base::android::ScopedJavaLocalRef<jbyteArray> GetCertificate(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void RequestNewHitTestDataAt(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj,
                               jfloat x,
                               jfloat y,
                               jfloat touch_major);
  void UpdateLastHitTestData(JNIEnv* env,
                             const base::android::JavaParamRef<jobject>& obj);
  void OnSizeChanged(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& obj,
                     int w,
                     int h,
                     int ow,
                     int oh);
  void SetViewVisibility(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj,
                         bool visible);
  void SetWindowVisibility(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           bool visible);
  void SetIsPaused(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   bool paused);
  void OnAttachedToWindow(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          int w,
                          int h);
  void OnDetachedFromWindow(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& obj);
  bool IsVisible(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  base::android::ScopedJavaLocalRef<jbyteArray> GetOpaqueState(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean RestoreFromOpaqueState(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jbyteArray>& state);
  void FocusFirstNode(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& obj);
  void SetBackgroundColor(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          jint color);
  void ZoomBy(JNIEnv* env,
              const base::android::JavaParamRef<jobject>& obj,
              jfloat delta);
  void OnComputeScroll(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       jlong animation_time_millis);
  bool OnDraw(JNIEnv* env,
              const base::android::JavaParamRef<jobject>& obj,
              const base::android::JavaParamRef<jobject>& canvas,
              jboolean is_hardware_accelerated,
              jint scroll_x,
              jint scroll_y,
              jint visible_left,
              jint visible_top,
              jint visible_right,
              jint visible_bottom);
  jlong CapturePicture(JNIEnv* env,
                       const base::android::JavaParamRef<jobject>& obj,
                       int width,
                       int height);
  void EnableOnNewPicture(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj,
                          jboolean enabled);
  void InsertVisualStateCallback(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jlong request_id,
      const base::android::JavaParamRef<jobject>& callback);
  void ClearView(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);
  void SetExtraHeadersForUrl(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& url,
      const base::android::JavaParamRef<jstring>& extra_headers);

  void InvokeGeolocationCallback(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jboolean value,
      const base::android::JavaParamRef<jstring>& origin);

  AwRendererPriorityManager::RendererPriority GetCurrentRendererPriority();
  jint GetRendererCurrentPriority(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jint GetRendererRequestedPriority(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  jboolean GetRendererPriorityWaivedWhenNotVisible(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void SetRendererPriorityPolicy(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      jint rendererRequestedPriority,
      jboolean waivedhenNotVisible);

  // PermissionRequestHandlerClient implementation.
  void OnPermissionRequest(base::android::ScopedJavaLocalRef<jobject> j_request,
                           AwPermissionRequest* request) override;
  void OnPermissionRequestCanceled(AwPermissionRequest* request) override;

  PermissionRequestHandler* GetPermissionRequestHandler() {
    return permission_request_handler_.get();
  }

  void PreauthorizePermission(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& origin,
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
  void RequestMIDISysexPermission(
      const GURL& origin,
      const base::Callback<void(bool)>& callback) override;
  void CancelMIDISysexPermissionRequests(const GURL& origin) override;

  // Find-in-page API and related methods.
  void FindAllAsync(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    const base::android::JavaParamRef<jstring>& search_string);
  void FindNext(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& obj,
                jboolean forward);
  void ClearMatches(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj);
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
  void PostInvalidate() override;
  void OnNewPicture() override;
  gfx::Point GetLocationOnScreen() override;
  void ScrollContainerViewTo(const gfx::Vector2d& new_value) override;
  void UpdateScrollState(const gfx::Vector2d& max_scroll_offset,
                         const gfx::SizeF& contents_size_dip,
                         float page_scale_factor,
                         float min_page_scale_factor,
                         float max_page_scale_factor) override;
  void DidOverscroll(const gfx::Vector2d& overscroll_delta,
                     const gfx::Vector2dF& overscroll_velocity) override;
  ui::TouchHandleDrawable* CreateDrawable() override;

  void ClearCache(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& obj,
                  jboolean include_disk_files);
  void KillRenderProcess(JNIEnv* env,
                         const base::android::JavaParamRef<jobject>& obj);
  void SetPendingWebContentsForPopup(
      std::unique_ptr<content::WebContents> pending);
  jlong ReleasePopupAwContents(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj);

  void ScrollTo(JNIEnv* env,
                const base::android::JavaParamRef<jobject>& obj,
                jint x,
                jint y);
  void SmoothScroll(JNIEnv* env,
                    const base::android::JavaParamRef<jobject>& obj,
                    jint target_x,
                    jint target_y,
                    jlong duration_ms);
  void SetDipScale(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj,
                   jfloat dip_scale);
  void SetSaveFormData(bool enabled);

  // Sets the java client
  void SetAwAutofillClient(const base::android::JavaRef<jobject>& client);

  void SetJsOnlineProperty(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& obj,
                           jboolean network_up);
  void TrimMemory(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& obj,
                  jint level,
                  jboolean visible);
  void PostMessageToFrame(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jstring>& frame_id,
      const base::android::JavaParamRef<jstring>& message,
      const base::android::JavaParamRef<jstring>& target_origin,
      const base::android::JavaParamRef<jobjectArray>& ports);

  void GrantFileSchemeAccesstoChildProcess(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  void ResumeLoadingCreatedPopupWebContents(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  // content::WebContentsObserver overrides
  void RenderViewHostChanged(content::RenderViewHost* old_host,
                             content::RenderViewHost* new_host) override;
  void DidAttachInterstitialPage() override;
  void DidDetachInterstitialPage() override;

  // content::RenderProcessHostObserver overrides
  void RenderProcessReady(content::RenderProcessHost* host) override;

  // AwSafeBrowsingUIManager::UIManagerClient implementation
  bool CanShowInterstitial() override;

  void CallProceedOnInterstitialForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);
  void CallDontProceedOnInterstitialForTesting(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  // AwRenderProcessGoneDelegate overrides
  void OnRenderProcessGone(int child_process_id) override;
  bool OnRenderProcessGoneDetail(int child_process_id, bool crashed) override;

 private:
  void InitAutofillIfNecessary(bool enabled);

  // Geolocation API support
  void ShowGeolocationPrompt(const GURL& origin, base::Callback<void(bool)>);
  void HideGeolocationPrompt(const GURL& origin);

  void SetDipScaleInternal(float dip_scale);

  void SetAwGLFunctor(AwGLFunctor* functor);

  AwRendererPriorityManager* GetAwRendererPriorityManager();
  AwRendererPriorityManager::RendererPriority GetComputedRendererPriority();
  void UpdateRendererPriority(
      AwRendererPriorityManager::RendererPriority base_priority);
  void UpdateRendererPriority();

  JavaObjectWeakGlobalRef java_ref_;
  AwGLFunctor* functor_;
  BrowserViewRenderer browser_view_renderer_;  // Must outlive |web_contents_|.
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<AwWebContentsDelegate> web_contents_delegate_;
  std::unique_ptr<AwContentsClientBridge> contents_client_bridge_;
  std::unique_ptr<AwRenderViewHostExt> render_view_host_ext_;
  std::unique_ptr<FindHelper> find_helper_;
  std::unique_ptr<IconHelper> icon_helper_;
  std::unique_ptr<AwContents> pending_contents_;
  std::unique_ptr<AwPdfExporter> pdf_exporter_;
  std::unique_ptr<PermissionRequestHandler> permission_request_handler_;

  // GURL is supplied by the content layer as requesting frame.
  // Callback is supplied by the content layer, and is invoked with the result
  // from the permission prompt.
  typedef std::pair<const GURL, base::Callback<void(bool)>> OriginCallback;
  // The first element in the list is always the currently pending request.
  std::list<OriginCallback> pending_geolocation_prompts_;

  GLViewRendererManager::Key renderer_manager_key_;

  AwRendererPriorityManager::RendererPriority renderer_requested_priority_;
  bool renderer_priority_waived_when_not_visible_;

  DISALLOW_COPY_AND_ASSIGN(AwContents);
};

bool RegisterAwContents(JNIEnv* env);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_H_
