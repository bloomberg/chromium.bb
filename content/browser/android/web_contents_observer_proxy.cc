// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/web_contents_observer_proxy.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/android/media_metadata_android.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/common/media_metadata.h"
#include "jni/WebContentsObserverProxy_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;

namespace content {

// TODO(dcheng): File a bug. This class incorrectly passes just a frame ID,
// which is not sufficient to identify a frame (since frame IDs are scoped per
// render process, and so may collide).
WebContentsObserverProxy::WebContentsObserverProxy(JNIEnv* env,
                                                   jobject obj,
                                                   WebContents* web_contents)
    : WebContentsObserver(web_contents) {
  DCHECK(obj);
  java_observer_.Reset(env, obj);
}

WebContentsObserverProxy::~WebContentsObserverProxy() {
}

jlong Init(JNIEnv* env,
           const JavaParamRef<jobject>& obj,
           const JavaParamRef<jobject>& java_web_contents) {
  WebContents* web_contents =
      WebContents::FromJavaWebContents(java_web_contents);
  CHECK(web_contents);

  WebContentsObserverProxy* native_observer =
      new WebContentsObserverProxy(env, obj, web_contents);
  return reinterpret_cast<intptr_t>(native_observer);
}

void WebContentsObserverProxy::Destroy(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  delete this;
}

void WebContentsObserverProxy::WebContentsDestroyed() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  // The java side will destroy |this|
  Java_WebContentsObserverProxy_destroy(env, obj);
}

void WebContentsObserverProxy::RenderViewReady() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  Java_WebContentsObserverProxy_renderViewReady(env, obj);
}

void WebContentsObserverProxy::RenderProcessGone(
    base::TerminationStatus termination_status) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  jboolean was_oom_protected =
      termination_status == base::TERMINATION_STATUS_OOM_PROTECTED;
  Java_WebContentsObserverProxy_renderProcessGone(env, obj, was_oom_protected);
}

void WebContentsObserverProxy::DidStartLoading() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  ScopedJavaLocalRef<jstring> jstring_url(
      ConvertUTF8ToJavaString(env, web_contents()->GetVisibleURL().spec()));
  if (auto* entry = web_contents()->GetController().GetPendingEntry()) {
    base_url_of_last_started_data_url_ = entry->GetBaseURLForDataURL();
  }
  Java_WebContentsObserverProxy_didStartLoading(env, obj, jstring_url);
}

void WebContentsObserverProxy::DidStopLoading() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  std::string url_string = web_contents()->GetLastCommittedURL().spec();
  SetToBaseURLForDataURLIfNeeded(&url_string);
  // DidStopLoading is the last event we should get.
  base_url_of_last_started_data_url_ = GURL::EmptyGURL();
  ScopedJavaLocalRef<jstring> jstring_url(ConvertUTF8ToJavaString(
      env, url_string));
  Java_WebContentsObserverProxy_didStopLoading(env, obj, jstring_url);
}

void WebContentsObserverProxy::DidFailLoad(
    RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description,
    bool was_ignored_by_handler) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  ScopedJavaLocalRef<jstring> jstring_error_description(
      ConvertUTF16ToJavaString(env, error_description));
  ScopedJavaLocalRef<jstring> jstring_url(
      ConvertUTF8ToJavaString(env, validated_url.spec()));

  Java_WebContentsObserverProxy_didFailLoad(
      env, obj, !render_frame_host->GetParent(), error_code,
      jstring_error_description, jstring_url);
}

void WebContentsObserverProxy::DocumentAvailableInMainFrame() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  Java_WebContentsObserverProxy_documentAvailableInMainFrame(env, obj);
}

void WebContentsObserverProxy::DidStartNavigation(
    NavigationHandle* navigation_handle) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  ScopedJavaLocalRef<jstring> jstring_url(
      ConvertUTF8ToJavaString(env, navigation_handle->GetURL().spec()));
  Java_WebContentsObserverProxy_didStartNavigation(
      env, obj, jstring_url, navigation_handle->IsInMainFrame(),
      navigation_handle->IsSamePage(), navigation_handle->IsErrorPage());
}

void WebContentsObserverProxy::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  ScopedJavaLocalRef<jstring> jstring_url(
      ConvertUTF8ToJavaString(env, navigation_handle->GetURL().spec()));

  bool is_fragment_navigation = navigation_handle->IsSamePage();

  if (navigation_handle->HasCommitted()) {
    // See http://crbug.com/251330 for why it's determined this way.
    url::Replacements<char> replacements;
    replacements.ClearRef();
    bool urls_same_ignoring_fragment =
        navigation_handle->GetURL().ReplaceComponents(replacements) ==
        navigation_handle->GetPreviousURL().ReplaceComponents(replacements);
    is_fragment_navigation &= urls_same_ignoring_fragment;
  }

  // TODO(shaktisahu): Provide appropriate error description (crbug/690784).
  ScopedJavaLocalRef<jstring> jerror_description;

  Java_WebContentsObserverProxy_didFinishNavigation(
      env, obj, jstring_url, navigation_handle->IsInMainFrame(),
      navigation_handle->IsErrorPage(), navigation_handle->HasCommitted(),
      navigation_handle->IsSamePage(), is_fragment_navigation,
      navigation_handle->HasCommitted() ? navigation_handle->GetPageTransition()
                                        : -1,
      navigation_handle->GetNetErrorCode(), jerror_description,
      // TODO(shaktisahu): Change default status to -1 after fixing
      // crbug/690041.
      navigation_handle->GetResponseHeaders()
          ? navigation_handle->GetResponseHeaders()->response_code()
          : 200);
}

void WebContentsObserverProxy::DidFinishLoad(RenderFrameHost* render_frame_host,
                                             const GURL& validated_url) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);

  std::string url_string = validated_url.spec();
  SetToBaseURLForDataURLIfNeeded(&url_string);

  ScopedJavaLocalRef<jstring> jstring_url(
      ConvertUTF8ToJavaString(env, url_string));
  Java_WebContentsObserverProxy_didFinishLoad(
      env, obj, render_frame_host->GetRoutingID(), jstring_url,
      !render_frame_host->GetParent());
}

void WebContentsObserverProxy::DocumentLoadedInFrame(
    RenderFrameHost* render_frame_host) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  Java_WebContentsObserverProxy_documentLoadedInFrame(
      env, obj, render_frame_host->GetRoutingID(),
      !render_frame_host->GetParent());
}

void WebContentsObserverProxy::NavigationEntryCommitted(
    const LoadCommittedDetails& load_details) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  Java_WebContentsObserverProxy_navigationEntryCommitted(env, obj);
}

void WebContentsObserverProxy::DidAttachInterstitialPage() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  Java_WebContentsObserverProxy_didAttachInterstitialPage(env, obj);
}

void WebContentsObserverProxy::DidDetachInterstitialPage() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  Java_WebContentsObserverProxy_didDetachInterstitialPage(env, obj);
}

void WebContentsObserverProxy::DidChangeThemeColor(SkColor color) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  Java_WebContentsObserverProxy_didChangeThemeColor(env, obj, color);
}

void WebContentsObserverProxy::DidFirstVisuallyNonEmptyPaint() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  Java_WebContentsObserverProxy_didFirstVisuallyNonEmptyPaint(env, obj);
}

void WebContentsObserverProxy::WasShown() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  Java_WebContentsObserverProxy_wasShown(env, obj);
}

void WebContentsObserverProxy::WasHidden() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  Java_WebContentsObserverProxy_wasHidden(env, obj);
}

void WebContentsObserverProxy::TitleWasSet(NavigationEntry* entry,
                                           bool explicit_set) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj(java_observer_);
  ScopedJavaLocalRef<jstring> jstring_title = ConvertUTF8ToJavaString(
      env,
      base::UTF16ToUTF8(web_contents()->GetTitle()));
  Java_WebContentsObserverProxy_titleWasSet(env, obj, jstring_title);
}

void WebContentsObserverProxy::SetToBaseURLForDataURLIfNeeded(
    std::string* url) {
  NavigationEntry* entry =
      web_contents()->GetController().GetLastCommittedEntry();
  // Note that GetBaseURLForDataURL is only used by the Android WebView.
  // FIXME: Should we only return valid specs and "about:blank" for invalid
  // ones? This may break apps.
  if (entry && !entry->GetBaseURLForDataURL().is_empty()) {
    *url = entry->GetBaseURLForDataURL().possibly_invalid_spec();
  } else if (!base_url_of_last_started_data_url_.is_empty()) {
    // NavigationController can lose the pending entry and recreate it without
    // a base URL if there has been a loadUrl("javascript:...") after
    // loadDataWithBaseUrl.
    *url = base_url_of_last_started_data_url_.possibly_invalid_spec();
  }
}

bool RegisterWebContentsObserverProxy(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
}  // namespace content
