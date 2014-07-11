// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/web_contents_observer_android.h"

#include <string>

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "jni/WebContentsObserverAndroid_jni.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaLocalRef;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;

namespace content {

// TODO(dcheng): File a bug. This class incorrectly passes just a frame ID,
// which is not sufficient to identify a frame (since frame IDs are scoped per
// render process, and so may collide).
WebContentsObserverAndroid::WebContentsObserverAndroid(
    JNIEnv* env,
    jobject obj,
    WebContents* web_contents)
    : WebContentsObserver(web_contents),
      weak_java_observer_(env, obj){
}

WebContentsObserverAndroid::~WebContentsObserverAndroid() {
}

jlong Init(JNIEnv* env, jobject obj, jobject java_web_contents) {
  WebContents* web_contents =
      WebContents::FromJavaWebContents(java_web_contents);
  CHECK(web_contents);

  WebContentsObserverAndroid* native_observer = new WebContentsObserverAndroid(
      env, obj, web_contents);
  return reinterpret_cast<intptr_t>(native_observer);
}

void WebContentsObserverAndroid::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void WebContentsObserverAndroid::WebContentsDestroyed() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(weak_java_observer_.get(env));
  if (obj.is_null()) {
    delete this;
  } else {
    // The java side will destroy |this|
    Java_WebContentsObserverAndroid_detachFromWebContents(env, obj.obj());
  }
}

void WebContentsObserverAndroid::RenderProcessGone(
    base::TerminationStatus termination_status) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(weak_java_observer_.get(env));
  if (obj.is_null())
    return;
  jboolean was_oom_protected =
      termination_status == base::TERMINATION_STATUS_OOM_PROTECTED;
  Java_WebContentsObserverAndroid_renderProcessGone(
      env, obj.obj(), was_oom_protected);
}

void WebContentsObserverAndroid::DidStartLoading(
    RenderViewHost* render_view_host) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(weak_java_observer_.get(env));
  if (obj.is_null())
    return;
  ScopedJavaLocalRef<jstring> jstring_url(ConvertUTF8ToJavaString(
      env, web_contents()->GetVisibleURL().spec()));
  Java_WebContentsObserverAndroid_didStartLoading(
      env, obj.obj(), jstring_url.obj());
}

void WebContentsObserverAndroid::DidStopLoading(
    RenderViewHost* render_view_host) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(weak_java_observer_.get(env));
  if (obj.is_null())
    return;
  ScopedJavaLocalRef<jstring> jstring_url(ConvertUTF8ToJavaString(
      env, web_contents()->GetLastCommittedURL().spec()));
  Java_WebContentsObserverAndroid_didStopLoading(
      env, obj.obj(), jstring_url.obj());
}

void WebContentsObserverAndroid::DidFailProvisionalLoad(
    RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description) {
  DidFailLoadInternal(true,
                      !render_frame_host->GetParent(),
                      error_code,
                      error_description,
                      validated_url);
}

void WebContentsObserverAndroid::DidFailLoad(
    RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description) {
  DidFailLoadInternal(false,
                      !render_frame_host->GetParent(),
                      error_code,
                      error_description,
                      validated_url);
}

void WebContentsObserverAndroid::DidNavigateMainFrame(
    const LoadCommittedDetails& details,
    const FrameNavigateParams& params) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(weak_java_observer_.get(env));
  if (obj.is_null())
    return;
  ScopedJavaLocalRef<jstring> jstring_url(
      ConvertUTF8ToJavaString(env, params.url.spec()));
  ScopedJavaLocalRef<jstring> jstring_base_url(
      ConvertUTF8ToJavaString(env, params.base_url.spec()));

  // See http://crbug.com/251330 for why it's determined this way.
  url::Replacements<char> replacements;
  replacements.ClearRef();
  bool urls_same_ignoring_fragment =
      params.url.ReplaceComponents(replacements) ==
      details.previous_url.ReplaceComponents(replacements);

  // is_fragment_navigation is indicative of the intent of this variable.
  // However, there isn't sufficient information here to determine whether this
  // is actually a fragment navigation, or a history API navigation to a URL
  // that would also be valid for a fragment navigation.
  bool is_fragment_navigation = urls_same_ignoring_fragment &&
      (details.type == NAVIGATION_TYPE_IN_PAGE || details.is_in_page);
  Java_WebContentsObserverAndroid_didNavigateMainFrame(
      env, obj.obj(), jstring_url.obj(), jstring_base_url.obj(),
      details.is_navigation_to_different_page(), is_fragment_navigation);
}

void WebContentsObserverAndroid::DidNavigateAnyFrame(
    const LoadCommittedDetails& details,
    const FrameNavigateParams& params) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(weak_java_observer_.get(env));
  if (obj.is_null())
    return;
  ScopedJavaLocalRef<jstring> jstring_url(
      ConvertUTF8ToJavaString(env, params.url.spec()));
  ScopedJavaLocalRef<jstring> jstring_base_url(
      ConvertUTF8ToJavaString(env, params.base_url.spec()));
  jboolean jboolean_is_reload =
      PageTransitionCoreTypeIs(params.transition, PAGE_TRANSITION_RELOAD);

  Java_WebContentsObserverAndroid_didNavigateAnyFrame(
      env, obj.obj(), jstring_url.obj(), jstring_base_url.obj(),
      jboolean_is_reload);
}

void WebContentsObserverAndroid::DidStartProvisionalLoadForFrame(
    RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    bool is_error_page,
    bool is_iframe_srcdoc) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(weak_java_observer_.get(env));
  if (obj.is_null())
    return;
  ScopedJavaLocalRef<jstring> jstring_url(
      ConvertUTF8ToJavaString(env, validated_url.spec()));
  // TODO(dcheng): Does Java really need the parent frame ID? It doesn't appear
  // to be used at all, and it just adds complexity here.
  Java_WebContentsObserverAndroid_didStartProvisionalLoadForFrame(
      env,
      obj.obj(),
      render_frame_host->GetRoutingID(),
      render_frame_host->GetParent()
          ? render_frame_host->GetParent()->GetRoutingID()
          : -1,
      !render_frame_host->GetParent(),
      jstring_url.obj(),
      is_error_page,
      is_iframe_srcdoc);
}

void WebContentsObserverAndroid::DidCommitProvisionalLoadForFrame(
    RenderFrameHost* render_frame_host,
    const GURL& url,
    PageTransition transition_type) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(weak_java_observer_.get(env));
  if (obj.is_null())
    return;
  ScopedJavaLocalRef<jstring> jstring_url(
      ConvertUTF8ToJavaString(env, url.spec()));
  Java_WebContentsObserverAndroid_didCommitProvisionalLoadForFrame(
      env,
      obj.obj(),
      render_frame_host->GetRoutingID(),
      !render_frame_host->GetParent(),
      jstring_url.obj(),
      transition_type);
}

void WebContentsObserverAndroid::DidFinishLoad(
    RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(weak_java_observer_.get(env));
  if (obj.is_null())
    return;

  std::string url_string = validated_url.spec();
  NavigationEntry* entry =
    web_contents()->GetController().GetLastCommittedEntry();
  // Note that GetBaseURLForDataURL is only used by the Android WebView.
  if (entry && !entry->GetBaseURLForDataURL().is_empty())
    url_string = entry->GetBaseURLForDataURL().possibly_invalid_spec();

  ScopedJavaLocalRef<jstring> jstring_url(
      ConvertUTF8ToJavaString(env, url_string));
  Java_WebContentsObserverAndroid_didFinishLoad(
      env,
      obj.obj(),
      render_frame_host->GetRoutingID(),
      jstring_url.obj(),
      !render_frame_host->GetParent());
}

void WebContentsObserverAndroid::DocumentLoadedInFrame(
    RenderFrameHost* render_frame_host) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(weak_java_observer_.get(env));
  if (obj.is_null())
    return;
  Java_WebContentsObserverAndroid_documentLoadedInFrame(
      env, obj.obj(), render_frame_host->GetRoutingID());
}

void WebContentsObserverAndroid::NavigationEntryCommitted(
    const LoadCommittedDetails& load_details) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(weak_java_observer_.get(env));
  if (obj.is_null())
    return;
  Java_WebContentsObserverAndroid_navigationEntryCommitted(env, obj.obj());
}

void WebContentsObserverAndroid::DidAttachInterstitialPage() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(weak_java_observer_.get(env));
  if (obj.is_null())
    return;
  Java_WebContentsObserverAndroid_didAttachInterstitialPage(env, obj.obj());
}

void WebContentsObserverAndroid::DidDetachInterstitialPage() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(weak_java_observer_.get(env));
  if (obj.is_null())
    return;
  Java_WebContentsObserverAndroid_didDetachInterstitialPage(env, obj.obj());
}

void WebContentsObserverAndroid::DidChangeThemeColor(SkColor color) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(weak_java_observer_.get(env));
  if (obj.is_null())
    return;
  Java_WebContentsObserverAndroid_didChangeThemeColor(env, obj.obj(), color);
}

void WebContentsObserverAndroid::DidFailLoadInternal(
    bool is_provisional_load,
    bool is_main_frame,
    int error_code,
    const base::string16& description,
    const GURL& url) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(weak_java_observer_.get(env));
  if (obj.is_null())
    return;
  ScopedJavaLocalRef<jstring> jstring_error_description(
      ConvertUTF16ToJavaString(env, description));
  ScopedJavaLocalRef<jstring> jstring_url(
      ConvertUTF8ToJavaString(env, url.spec()));

  Java_WebContentsObserverAndroid_didFailLoad(
      env, obj.obj(),
      is_provisional_load,
      is_main_frame,
      error_code,
      jstring_error_description.obj(), jstring_url.obj());
}

void WebContentsObserverAndroid::DidFirstVisuallyNonEmptyPaint() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(weak_java_observer_.get(env));
  if (obj.is_null())
    return;
  Java_WebContentsObserverAndroid_didFirstVisuallyNonEmptyPaint(
      env, obj.obj());
}

bool RegisterWebContentsObserverAndroid(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
}  // namespace content
