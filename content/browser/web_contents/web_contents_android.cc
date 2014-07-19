// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "content/browser/android/interstitial_page_delegate_android.h"
#include "content/browser/frame_host/interstitial_page_impl.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/frame_messages.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "jni/WebContentsImpl_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;

namespace content {

// static
WebContents* WebContents::FromJavaWebContents(
    jobject jweb_contents_android) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!jweb_contents_android)
    return NULL;

  WebContentsAndroid* web_contents_android =
      reinterpret_cast<WebContentsAndroid*>(
          Java_WebContentsImpl_getNativePointer(AttachCurrentThread(),
                                                jweb_contents_android));
  if (!web_contents_android)
    return NULL;
  return web_contents_android->web_contents();
}

// static
bool WebContentsAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

WebContentsAndroid::WebContentsAndroid(WebContents* web_contents)
    : web_contents_(web_contents),
      navigation_controller_(&(web_contents->GetController())) {
  JNIEnv* env = AttachCurrentThread();
  obj_.Reset(env,
             Java_WebContentsImpl_create(
                 env,
                 reinterpret_cast<intptr_t>(this),
                 navigation_controller_.GetJavaObject().obj()).obj());
}

WebContentsAndroid::~WebContentsAndroid() {
  Java_WebContentsImpl_destroy(AttachCurrentThread(), obj_.obj());
}

base::android::ScopedJavaLocalRef<jobject>
WebContentsAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(obj_);
}

ScopedJavaLocalRef<jstring> WebContentsAndroid::GetTitle(
    JNIEnv* env, jobject obj) const {
  return base::android::ConvertUTF16ToJavaString(env,
                                                 web_contents_->GetTitle());
}

ScopedJavaLocalRef<jstring> WebContentsAndroid::GetVisibleURL(
    JNIEnv* env, jobject obj) const {
  return base::android::ConvertUTF8ToJavaString(
      env, web_contents_->GetVisibleURL().spec());
}

void WebContentsAndroid::Stop(JNIEnv* env, jobject obj) {
  web_contents_->Stop();
}

void WebContentsAndroid::InsertCSS(
    JNIEnv* env, jobject jobj, jstring jcss) {
  web_contents_->InsertCSS(base::android::ConvertJavaStringToUTF8(env, jcss));
}

RenderWidgetHostViewAndroid*
    WebContentsAndroid::GetRenderWidgetHostViewAndroid() {
  RenderWidgetHostView* rwhv = NULL;
  rwhv = web_contents_->GetRenderWidgetHostView();
  if (web_contents_->ShowingInterstitialPage()) {
    rwhv = static_cast<InterstitialPageImpl*>(
        web_contents_->GetInterstitialPage())->
            GetRenderViewHost()->GetView();
  }
  return static_cast<RenderWidgetHostViewAndroid*>(rwhv);
}

jint WebContentsAndroid::GetBackgroundColor(JNIEnv* env, jobject obj) {
  RenderWidgetHostViewAndroid* rwhva = GetRenderWidgetHostViewAndroid();
  if (!rwhva)
    return SK_ColorWHITE;
  return rwhva->GetCachedBackgroundColor();
}

void WebContentsAndroid::OnHide(JNIEnv* env, jobject obj) {
  web_contents_->WasHidden();
  PauseVideo();
}

void WebContentsAndroid::OnShow(JNIEnv* env, jobject obj) {
  web_contents_->WasShown();
}

void WebContentsAndroid::PauseVideo() {
  RenderViewHostImpl* rvhi = static_cast<RenderViewHostImpl*>(
      web_contents_->GetRenderViewHost());
  if (rvhi)
    rvhi->media_web_contents_observer()->PauseVideo();
}

void WebContentsAndroid::AddStyleSheetByURL(
    JNIEnv* env,
    jobject obj,
    jstring url) {
  web_contents_->GetMainFrame()->Send(new FrameMsg_AddStyleSheetByURL(
      web_contents_->GetMainFrame()->GetRoutingID(),
      ConvertJavaStringToUTF8(env, url)));
}

void WebContentsAndroid::ShowInterstitialPage(
    JNIEnv* env,
    jobject obj,
    jstring jurl,
    jlong delegate_ptr) {
  GURL url(base::android::ConvertJavaStringToUTF8(env, jurl));
  InterstitialPageDelegateAndroid* delegate =
      reinterpret_cast<InterstitialPageDelegateAndroid*>(delegate_ptr);
  InterstitialPage* interstitial = InterstitialPage::Create(
      web_contents_, false, url, delegate);
  delegate->set_interstitial_page(interstitial);
  interstitial->Show();
}

jboolean WebContentsAndroid::IsShowingInterstitialPage(JNIEnv* env,
                                                        jobject obj) {
  return web_contents_->ShowingInterstitialPage();
}

jboolean WebContentsAndroid::IsRenderWidgetHostViewReady(
    JNIEnv* env,
    jobject obj) {
  RenderWidgetHostViewAndroid* view = GetRenderWidgetHostViewAndroid();
  return view && view->HasValidFrame();
}

void WebContentsAndroid::ExitFullscreen(JNIEnv* env, jobject obj) {
  RenderViewHost* host = web_contents_->GetRenderViewHost();
  if (!host)
    return;
  host->ExitFullscreen();
}

void WebContentsAndroid::UpdateTopControlsState(
    JNIEnv* env,
    jobject obj,
    bool enable_hiding,
    bool enable_showing,
    bool animate) {
  RenderViewHost* host = web_contents_->GetRenderViewHost();
  if (!host)
    return;
  host->Send(new ViewMsg_UpdateTopControlsState(host->GetRoutingID(),
                                                enable_hiding,
                                                enable_showing,
                                                animate));
}

void WebContentsAndroid::ShowImeIfNeeded(JNIEnv* env, jobject obj) {
  RenderViewHost* host = web_contents_->GetRenderViewHost();
  if (!host)
    return;
  host->Send(new ViewMsg_ShowImeIfNeeded(host->GetRoutingID()));
}

void WebContentsAndroid::ScrollFocusedEditableNodeIntoView(
    JNIEnv* env,
    jobject obj) {
  RenderViewHost* host = web_contents_->GetRenderViewHost();
  if (!host)
    return;
  host->Send(new InputMsg_ScrollFocusedEditableNodeIntoRect(
      host->GetRoutingID(), gfx::Rect()));
}

void WebContentsAndroid::SelectWordAroundCaret(JNIEnv* env, jobject obj) {
  RenderViewHost* host = web_contents_->GetRenderViewHost();
  if (!host)
    return;
  host->SelectWordAroundCaret();
}

}  // namespace content
