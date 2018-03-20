// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_view_core.h"

#include <stddef.h>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "cc/layers/layer.h"
#include "cc/layers/solid_color_layer.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "content/browser/android/interstitial_page_delegate_android.h"
#include "content/browser/frame_host/interstitial_page_impl.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/renderer_host/compositor_impl_android.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_android.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "content/common/frame_messages.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/android/compositor.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/ssl_host_state_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
#include "jni/ContentViewCoreImpl_jni.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "ui/android/view_android.h"
#include "ui/android/window_android.h"
#include "ui/events/android/gesture_event_type.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/motion_event.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/size_conversions.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using blink::WebGestureEvent;
using blink::WebInputEvent;

namespace content {

namespace {

int GetRenderProcessIdFromRenderViewHost(RenderViewHost* host) {
  DCHECK(host);
  RenderProcessHost* render_process = host->GetProcess();
  DCHECK(render_process);
  if (render_process->HasConnection())
    return render_process->GetHandle();
  return 0;
}

void SetContentViewCore(WebContents* web_contents, ContentViewCore* cvc) {
  WebContentsViewAndroid* wcva = static_cast<WebContentsViewAndroid*>(
      static_cast<WebContentsImpl*>(web_contents)->GetView());
  DCHECK(wcva);
  wcva->SetContentViewCore(cvc);
}

}  // namespace

ContentViewCore::ContentViewCore(JNIEnv* env,
                                 const JavaRef<jobject>& obj,
                                 WebContents* web_contents,
                                 float dpi_scale)
    : WebContentsObserver(web_contents),
      java_ref_(env, obj),
      web_contents_(static_cast<WebContentsImpl*>(web_contents)),
      dpi_scale_(dpi_scale),
      device_orientation_(0) {
  GetViewAndroid()->SetLayer(cc::Layer::Create());

  // Currently, the only use case we have for overriding a user agent involves
  // spoofing a desktop Linux user agent for "Request desktop site".
  // Automatically set it for all WebContents so that it is available when a
  // NavigationEntry requires the user agent to be overridden.
  const char kLinuxInfoStr[] = "X11; Linux x86_64";
  std::string product = content::GetContentClient()->GetProduct();
  std::string spoofed_ua =
      BuildUserAgentFromOSAndProduct(kLinuxInfoStr, product);
  web_contents->SetUserAgentOverride(spoofed_ua, false);

  InitWebContents();
}

ContentViewCore::~ContentViewCore() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_ref_.get(env);
  java_ref_.reset();
  if (!j_obj.is_null()) {
    Java_ContentViewCoreImpl_onNativeContentViewCoreDestroyed(
        env, j_obj, reinterpret_cast<intptr_t>(this));
  }
}

void ContentViewCore::UpdateWindowAndroid(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jwindow_android) {
  ui::WindowAndroid* window =
      ui::WindowAndroid::FromJavaWindowAndroid(jwindow_android);
  auto* old_window = GetWindowAndroid();
  if (window == old_window)
    return;

  auto* view = GetViewAndroid();
  if (old_window)
    view->RemoveFromParent();
  if (window)
    window->AddChild(view);
}

base::android::ScopedJavaLocalRef<jobject>
ContentViewCore::GetWebContentsAndroid(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  return web_contents_->GetJavaWebContents();
}

base::android::ScopedJavaLocalRef<jobject>
ContentViewCore::GetJavaWindowAndroid(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj) {
  if (!GetWindowAndroid())
    return ScopedJavaLocalRef<jobject>();
  return GetWindowAndroid()->GetJavaObject();
}

void ContentViewCore::OnJavaContentViewCoreDestroyed(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  DCHECK(env->IsSameObject(java_ref_.get(env).obj(), obj));
  java_ref_.reset();
  // Java peer has gone, ContentViewCore is not functional and waits to
  // be destroyed with WebContents.
  // We need to reset WebContentsViewAndroid's reference, otherwise, there
  // could have call in when swapping the WebContents,
  // see http://crbug.com/383939 .
  DCHECK(web_contents_);
  SetContentViewCore(web_contents(), nullptr);
}

void ContentViewCore::InitWebContents() {
  DCHECK(web_contents_);
  SetContentViewCore(web_contents(), this);
}

void ContentViewCore::RenderViewReady() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null())
    Java_ContentViewCoreImpl_onRenderProcessChange(env, obj);

  if (device_orientation_ != 0)
    SendOrientationChangeEventInternal();
}

void ContentViewCore::RenderViewHostChanged(RenderViewHost* old_host,
                                            RenderViewHost* new_host) {
  int old_pid = 0;
  if (old_host) {
    old_pid = GetRenderProcessIdFromRenderViewHost(old_host);

    RenderWidgetHostViewAndroid* view =
        static_cast<RenderWidgetHostViewAndroid*>(
            old_host->GetWidget()->GetView());
    if (view)
      view->UpdateNativeViewTree(nullptr);

    view = static_cast<RenderWidgetHostViewAndroid*>(
        new_host->GetWidget()->GetView());
    if (view)
      view->UpdateNativeViewTree(GetViewAndroid());
  }
  int new_pid =
      GetRenderProcessIdFromRenderViewHost(web_contents_->GetRenderViewHost());
  if (new_pid != old_pid) {
    // Notify the Java side that the renderer process changed.
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
    if (!obj.is_null()) {
      Java_ContentViewCoreImpl_onRenderProcessChange(env, obj);
    }
  }

  SetFocusInternal(GetViewAndroid()->HasFocus());
}

RenderWidgetHostViewAndroid* ContentViewCore::GetRenderWidgetHostViewAndroid()
    const {
  RenderWidgetHostView* rwhv = NULL;
  if (web_contents_) {
    rwhv = web_contents_->GetRenderWidgetHostView();
    if (web_contents_->ShowingInterstitialPage()) {
      rwhv = web_contents_->GetInterstitialPage()
                 ->GetMainFrame()
                 ->GetRenderViewHost()
                 ->GetWidget()
                 ->GetView();
    }
  }
  return static_cast<RenderWidgetHostViewAndroid*>(rwhv);
}

ScopedJavaLocalRef<jobject> ContentViewCore::GetJavaObject() {
  JNIEnv* env = AttachCurrentThread();
  return java_ref_.get(env);
}

void ContentViewCore::SendScreenRectsAndResizeWidget() {
  RenderWidgetHostViewAndroid* view = GetRenderWidgetHostViewAndroid();
  if (view) {
    // |SendScreenRects()| indirectly calls GetViewSize() that asks Java layer.
    web_contents_->SendScreenRects();
    view->WasResized();
  }
}

ui::WindowAndroid* ContentViewCore::GetWindowAndroid() const {
  return GetViewAndroid()->GetWindowAndroid();
}

ui::ViewAndroid* ContentViewCore::GetViewAndroid() const {
  return web_contents_->GetView()->GetNativeView();
}

// ----------------------------------------------------------------------------
// Methods called from Java via JNI
// ----------------------------------------------------------------------------

WebContents* ContentViewCore::GetWebContents() const {
  return web_contents_;
}

void ContentViewCore::SetFocus(JNIEnv* env,
                               const JavaParamRef<jobject>& obj,
                               jboolean focused) {
  SetFocusInternal(focused);
}

void ContentViewCore::SetDIPScale(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj,
                                  jfloat dpi_scale) {
  if (dpi_scale_ == dpi_scale)
    return;

  dpi_scale_ = dpi_scale;
  SendScreenRectsAndResizeWidget();
}

void ContentViewCore::SetFocusInternal(bool focused) {
  if (!GetRenderWidgetHostViewAndroid())
    return;

  if (focused)
    GetRenderWidgetHostViewAndroid()->GotFocus();
  else
    GetRenderWidgetHostViewAndroid()->LostFocus();
}

int ContentViewCore::GetTopControlsShrinkBlinkHeightPixForTesting(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  return !rwhv || !rwhv->DoBrowserControlsShrinkBlinkSize()
             ? 0
             : rwhv->GetTopControlsHeight() * dpi_scale_;
}

void ContentViewCore::SendOrientationChangeEvent(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint orientation) {
  if (device_orientation_ != orientation) {
    base::RecordAction(base::UserMetricsAction("ScreenOrientationChange"));
    device_orientation_ = orientation;
    SendOrientationChangeEventInternal();
  }
}

void ContentViewCore::ResetGestureDetection(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv)
    rwhv->ResetGestureDetection();
}

void ContentViewCore::SetDoubleTapSupportEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean enabled) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv)
    rwhv->SetDoubleTapSupportEnabled(enabled);
}

void ContentViewCore::SetMultiTouchZoomSupportEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean enabled) {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv)
    rwhv->SetMultiTouchZoomSupportEnabled(enabled);
}

void ContentViewCore::OnTouchDown(
    const base::android::ScopedJavaLocalRef<jobject>& event) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  Java_ContentViewCoreImpl_onTouchDown(env, obj, event);
}

void ContentViewCore::SetTextTrackSettings(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jboolean textTracksEnabled,
    const JavaParamRef<jstring>& textTrackBackgroundColor,
    const JavaParamRef<jstring>& textTrackFontFamily,
    const JavaParamRef<jstring>& textTrackFontStyle,
    const JavaParamRef<jstring>& textTrackFontVariant,
    const JavaParamRef<jstring>& textTrackTextColor,
    const JavaParamRef<jstring>& textTrackTextShadow,
    const JavaParamRef<jstring>& textTrackTextSize) {
  FrameMsg_TextTrackSettings_Params params;
  params.text_tracks_enabled = textTracksEnabled;
  params.text_track_background_color =
      ConvertJavaStringToUTF8(env, textTrackBackgroundColor);
  params.text_track_font_family =
      ConvertJavaStringToUTF8(env, textTrackFontFamily);
  params.text_track_font_style =
      ConvertJavaStringToUTF8(env, textTrackFontStyle);
  params.text_track_font_variant =
      ConvertJavaStringToUTF8(env, textTrackFontVariant);
  params.text_track_text_color =
      ConvertJavaStringToUTF8(env, textTrackTextColor);
  params.text_track_text_shadow =
      ConvertJavaStringToUTF8(env, textTrackTextShadow);
  params.text_track_text_size = ConvertJavaStringToUTF8(env, textTrackTextSize);
  web_contents_->GetMainFrame()->SetTextTrackSettings(params);
}

void ContentViewCore::SendOrientationChangeEventInternal() {
  RenderWidgetHostViewAndroid* rwhv = GetRenderWidgetHostViewAndroid();
  if (rwhv)
    rwhv->UpdateScreenInfo(GetViewAndroid());

  static_cast<WebContentsImpl*>(web_contents())->OnScreenOrientationChange();
}

jboolean ContentViewCore::UsingSynchronousCompositing(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return content::GetContentClient()->UsingSynchronousCompositing();
}

// This is called for each ContentView.
jlong JNI_ContentViewCoreImpl_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jweb_contents,
    const JavaParamRef<jobject>& jview_android_delegate,
    const JavaParamRef<jobject>& jwindow_android,
    jfloat dip_scale) {
  WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
      WebContents::FromJavaWebContents(jweb_contents));
  CHECK(web_contents)
      << "A ContentViewCore should be created with a valid WebContents.";
  ui::ViewAndroid* view_android = web_contents->GetView()->GetNativeView();
  view_android->SetDelegate(jview_android_delegate);

  ui::WindowAndroid* window_android =
      ui::WindowAndroid::FromJavaWindowAndroid(jwindow_android);
  DCHECK(window_android);
  window_android->AddChild(view_android);

  ContentViewCore* view =
      new ContentViewCore(env, obj, web_contents, dip_scale);
  return reinterpret_cast<intptr_t>(view);
}

}  // namespace content
