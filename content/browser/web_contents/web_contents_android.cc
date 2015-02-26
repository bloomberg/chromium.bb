// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "content/browser/android/interstitial_page_delegate_android.h"
#include "content/browser/frame_host/interstitial_page_impl.h"
#include "content/browser/media/android/browser_media_player_manager.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/devtools_messages.h"
#include "content/common/frame_messages.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "jni/WebContentsImpl_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaGlobalRef;

namespace {

void JavaScriptResultCallback(const ScopedJavaGlobalRef<jobject>& callback,
                              const base::Value* result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::string json;
  base::JSONWriter::Write(result, &json);
  ScopedJavaLocalRef<jstring> j_json = ConvertUTF8ToJavaString(env, json);
  content::Java_WebContentsImpl_onEvaluateJavaScriptResult(
      env, j_json.obj(), callback.obj());
}

void ReleaseAllMediaPlayers(content::WebContents* web_contents,
                            content::RenderFrameHost* render_frame_host) {
  content::BrowserMediaPlayerManager* manager =
      static_cast<content::WebContentsImpl*>(web_contents)->
          media_web_contents_observer()->GetMediaPlayerManager(
              render_frame_host);
  if (manager)
    manager->ReleaseAllMediaPlayers();
}

}  // namespace

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
static void DestroyWebContents(JNIEnv* env,
                               jclass clazz,
                               jlong jweb_contents_android_ptr) {
  WebContentsAndroid* web_contents_android =
      reinterpret_cast<WebContentsAndroid*>(jweb_contents_android_ptr);
  if (!web_contents_android)
    return;

  content::WebContents* web_contents = web_contents_android->web_contents();
  if (!web_contents)
    return;

  delete web_contents;
}

// static
bool WebContentsAndroid::Register(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

WebContentsAndroid::WebContentsAndroid(WebContents* web_contents)
    : web_contents_(web_contents),
      navigation_controller_(&(web_contents->GetController())),
      weak_factory_(this) {
  JNIEnv* env = AttachCurrentThread();
  obj_.Reset(env,
             Java_WebContentsImpl_create(
                 env,
                 reinterpret_cast<intptr_t>(this),
                 navigation_controller_.GetJavaObject().obj()).obj());
}

WebContentsAndroid::~WebContentsAndroid() {
  Java_WebContentsImpl_clearNativePtr(AttachCurrentThread(), obj_.obj());
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

bool WebContentsAndroid::IsLoading(JNIEnv* env, jobject obj) const {
  return web_contents_->IsLoading();
}

bool WebContentsAndroid::IsLoadingToDifferentDocument(JNIEnv* env,
                                                      jobject obj) const {
  return web_contents_->IsLoadingToDifferentDocument();
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
    rwhv = web_contents_->GetInterstitialPage()
               ->GetMainFrame()
               ->GetRenderViewHost()
               ->GetView();
  }
  return static_cast<RenderWidgetHostViewAndroid*>(rwhv);
}

jint WebContentsAndroid::GetBackgroundColor(JNIEnv* env, jobject obj) {
  RenderWidgetHostViewAndroid* rwhva = GetRenderWidgetHostViewAndroid();
  if (!rwhva)
    return SK_ColorWHITE;
  return rwhva->GetCachedBackgroundColor();
}

ScopedJavaLocalRef<jstring> WebContentsAndroid::GetURL(JNIEnv* env,
                                                       jobject obj) const {
  return ConvertUTF8ToJavaString(env, web_contents_->GetURL().spec());
}

ScopedJavaLocalRef<jstring> WebContentsAndroid::GetLastCommittedURL(
    JNIEnv* env,
    jobject) const {
  return ConvertUTF8ToJavaString(env,
                                 web_contents_->GetLastCommittedURL().spec());
}


jboolean WebContentsAndroid::IsIncognito(JNIEnv* env, jobject obj) {
  return web_contents_->GetBrowserContext()->IsOffTheRecord();
}

void WebContentsAndroid::ResumeResponseDeferredAtStart(JNIEnv* env,
                                                       jobject obj) {
  static_cast<WebContentsImpl*>(web_contents_)->ResumeResponseDeferredAtStart();
}

void WebContentsAndroid::SetHasPendingNavigationTransitionForTesting(
    JNIEnv* env,
    jobject obj) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalWebPlatformFeatures);
  RenderFrameHost* frame =
      static_cast<WebContentsImpl*>(web_contents_)->GetMainFrame();
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(
          &TransitionRequestManager::AddPendingTransitionRequestDataForTesting,
          base::Unretained(TransitionRequestManager::GetInstance()),
          frame->GetProcess()->GetID(),
          frame->GetRoutingID()));
}

void WebContentsAndroid::SetupTransitionView(JNIEnv* env,
                                             jobject jobj,
                                             jstring markup) {
  web_contents_->GetMainFrame()->Send(new FrameMsg_SetupTransitionView(
      web_contents_->GetMainFrame()->GetRoutingID(),
      ConvertJavaStringToUTF8(env, markup)));
}

void WebContentsAndroid::BeginExitTransition(JNIEnv* env,
                                             jobject jobj,
                                             jstring css_selector,
                                             jboolean exit_to_native_app) {
  web_contents_->GetMainFrame()->Send(new FrameMsg_BeginExitTransition(
      web_contents_->GetMainFrame()->GetRoutingID(),
      ConvertJavaStringToUTF8(env, css_selector),
      exit_to_native_app));
}

void WebContentsAndroid::RevertExitTransition(JNIEnv* env,
                                              jobject jobj) {
  web_contents_->GetMainFrame()->Send(new FrameMsg_RevertExitTransition(
      web_contents_->GetMainFrame()->GetRoutingID()));
}

void WebContentsAndroid::HideTransitionElements(JNIEnv* env,
                                                jobject jobj,
                                                jstring css_selector) {
  web_contents_->GetMainFrame()->Send(
      new FrameMsg_HideTransitionElements(
          web_contents_->GetMainFrame()->GetRoutingID(),
          ConvertJavaStringToUTF8(env, css_selector)));
}

void WebContentsAndroid::ShowTransitionElements(JNIEnv* env,
                                                jobject jobj,
                                                jstring css_selector) {
  web_contents_->GetMainFrame()->Send(
      new FrameMsg_ShowTransitionElements(
          web_contents_->GetMainFrame()->GetRoutingID(),
          ConvertJavaStringToUTF8(env, css_selector)));
}


void WebContentsAndroid::ClearNavigationTransitionData(JNIEnv* env,
                                                       jobject jobj) {
  static_cast<WebContentsImpl*>(web_contents_)->ClearNavigationTransitionData();
}

void WebContentsAndroid::FetchTransitionElements(JNIEnv* env,
                                                 jobject jobj,
                                                 jstring jurl) {
  GURL url(base::android::ConvertJavaStringToUTF8(env, jurl));
  RenderFrameHost* frame = web_contents_->GetMainFrame();

  scoped_ptr<TransitionLayerData> transition_data(new TransitionLayerData());
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&TransitionRequestManager::GetPendingTransitionRequest,
                 base::Unretained(TransitionRequestManager::GetInstance()),
                 frame->GetProcess()->GetID(),
                 frame->GetRoutingID(),
                 url,
                 transition_data.get()),
      base::Bind(&WebContentsAndroid::OnTransitionElementsFetched,
                 weak_factory_.GetWeakPtr(),
                 base::Passed(&transition_data)));
}

void WebContentsAndroid::OnTransitionElementsFetched(
    scoped_ptr<const TransitionLayerData> transition_data,
    bool has_transition_data) {
  // FetchTransitionElements is called after the navigation transition state
  // machine starts, which means there must be transition data.
  DCHECK(has_transition_data);
  JNIEnv* env = AttachCurrentThread();

  std::vector<TransitionElement>::const_iterator it =
      transition_data->elements.begin();
  for (; it != transition_data->elements.end(); ++it) {
    ScopedJavaLocalRef<jstring> jstring_name(ConvertUTF8ToJavaString(env,
                                                                     it->id));
    Java_WebContentsImpl_addNavigationTransitionElements(
        env, obj_.obj(), jstring_name.obj(),
        it->rect.x(), it->rect.y(), it->rect.width(), it->rect.height());
  }

  ScopedJavaLocalRef<jstring> jstring_css_selector(
      ConvertUTF8ToJavaString(env, transition_data->css_selector));
  Java_WebContentsImpl_onTransitionElementsFetched(
      env, obj_.obj(), jstring_css_selector.obj());
}

void WebContentsAndroid::OnHide(JNIEnv* env, jobject obj) {
  web_contents_->WasHidden();
}

void WebContentsAndroid::OnShow(JNIEnv* env, jobject obj) {
  web_contents_->WasShown();
}

void WebContentsAndroid::ReleaseMediaPlayers(JNIEnv* env, jobject jobj) {
#if defined(ENABLE_BROWSER_CDMS)
  web_contents_->ForEachFrame(
      base::Bind(&ReleaseAllMediaPlayers, base::Unretained(web_contents_)));
#endif // defined(ENABLE_BROWSER_CDMS)
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
  web_contents_->ExitFullscreen();
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

bool WebContentsAndroid::WillHandleDeferAfterResponseStarted() {
  JNIEnv* env = AttachCurrentThread();
  return Java_WebContentsImpl_willHandleDeferAfterResponseStarted(env,
                                                                  obj_.obj());
}

void WebContentsAndroid::DidDeferAfterResponseStarted(
    const TransitionLayerData& transition_data) {
  JNIEnv* env = AttachCurrentThread();
  std::vector<GURL> entering_stylesheets;
  std::string transition_color;
  if (transition_data.response_headers.get()) {
    TransitionRequestManager::ParseTransitionStylesheetsFromHeaders(
        transition_data.response_headers,
        entering_stylesheets,
        transition_data.request_url);

    transition_data.response_headers->EnumerateHeader(
        NULL, "X-Transition-Entering-Color", &transition_color);
  }

  ScopedJavaLocalRef<jstring> jstring_markup(
      ConvertUTF8ToJavaString(env, transition_data.markup));

  ScopedJavaLocalRef<jstring> jstring_css_selector(
      ConvertUTF8ToJavaString(env, transition_data.css_selector));

  ScopedJavaLocalRef<jstring> jstring_transition_color(
      ConvertUTF8ToJavaString(env, transition_color));

  Java_WebContentsImpl_didDeferAfterResponseStarted(
      env,
      obj_.obj(),
      jstring_markup.obj(),
      jstring_css_selector.obj(),
      jstring_transition_color.obj());

  std::vector<GURL>::const_iterator iter = entering_stylesheets.begin();
  for (; iter != entering_stylesheets.end(); ++iter) {
    ScopedJavaLocalRef<jstring> jstring_url(
        ConvertUTF8ToJavaString(env, iter->spec()));
    Java_WebContentsImpl_addEnteringStylesheetToTransition(
        env, obj_.obj(), jstring_url.obj());
  }
}

void WebContentsAndroid::DidStartNavigationTransitionForFrame(int64 frame_id) {
  JNIEnv* env = AttachCurrentThread();
  Java_WebContentsImpl_didStartNavigationTransitionForFrame(
      env, obj_.obj(), frame_id);
}

void WebContentsAndroid::EvaluateJavaScript(JNIEnv* env,
                                            jobject obj,
                                            jstring script,
                                            jobject callback) {
  RenderViewHost* rvh = web_contents_->GetRenderViewHost();
  DCHECK(rvh);

  if (!rvh->IsRenderViewLive()) {
    if (!static_cast<WebContentsImpl*>(web_contents_)->
        CreateRenderViewForInitialEmptyDocument()) {
      LOG(ERROR) << "Failed to create RenderView in EvaluateJavaScript";
      return;
    }
  }

  if (!callback) {
    // No callback requested.
    web_contents_->GetMainFrame()->ExecuteJavaScript(
        ConvertJavaStringToUTF16(env, script));
    return;
  }

  // Secure the Java callback in a scoped object and give ownership of it to the
  // base::Callback.
  ScopedJavaGlobalRef<jobject> j_callback;
  j_callback.Reset(env, callback);
  content::RenderFrameHost::JavaScriptResultCallback js_callback =
      base::Bind(&JavaScriptResultCallback, j_callback);

  web_contents_->GetMainFrame()->ExecuteJavaScript(
      ConvertJavaStringToUTF16(env, script), js_callback);
}

void WebContentsAndroid::AddMessageToDevToolsConsole(JNIEnv* env,
                                                     jobject jobj,
                                                     jint level,
                                                     jstring message) {
  DCHECK_GE(level, 0);
  DCHECK_LE(level, CONSOLE_MESSAGE_LEVEL_LAST);

  web_contents_->GetMainFrame()->Send(new DevToolsAgentMsg_AddMessageToConsole(
      web_contents_->GetMainFrame()->GetRoutingID(),
      static_cast<ConsoleMessageLevel>(level),
      ConvertJavaStringToUTF8(env, message)));
}

jboolean WebContentsAndroid::HasAccessedInitialDocument(
    JNIEnv* env,
    jobject jobj) {
  return static_cast<content::WebContentsImpl*>(web_contents_)->
      HasAccessedInitialDocument();
}

}  // namespace content
