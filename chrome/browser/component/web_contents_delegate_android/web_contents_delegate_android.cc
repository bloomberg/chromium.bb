// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component/web_contents_delegate_android/web_contents_delegate_android.h"

#include <android/keycodes.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/referrer.h"
#include "jni/WebContentsDelegateAndroid_jni.h"
#include "net/http/http_request_headers.h"
#include "ui/gfx/rect.h"
#include "webkit/glue/window_open_disposition.h"

using base::android::AttachCurrentThread;
using base::android::CheckException;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::GetClass;
using base::android::GetMethodID;
using base::android::HasClass;
using base::android::ScopedJavaLocalRef;
using content::DownloadItem;
using content::JavaScriptDialogCreator;
using content::RenderViewHost;
using content::WebContents;

namespace web_contents_delegate_android {

WebContentsDelegateAndroid::WebContentsDelegateAndroid(JNIEnv* env, jobject obj)
    : weak_java_delegate_(env, obj),
      javascript_dialog_creator_(NULL) {
}

WebContentsDelegateAndroid::~WebContentsDelegateAndroid() {
}

ScopedJavaLocalRef<jobject>
WebContentsDelegateAndroid::GetJavaDelegate(JNIEnv* env) const {
  return weak_java_delegate_.get(env);
}

// ----------------------------------------------------------------------------
// WebContentsDelegate methods
// ----------------------------------------------------------------------------

// OpenURLFromTab() will be called when we're performing a browser-intiated
// navigation. The most common scenario for this is opening new tabs (see
// RenderViewImpl::decidePolicyForNavigation for more details).
WebContents* WebContentsDelegateAndroid::OpenURLFromTab(
    WebContents* source,
    const content::OpenURLParams& params) {
  const GURL& url = params.url;
  WindowOpenDisposition disposition = params.disposition;
  content::PageTransition transition(
      PageTransitionFromInt(params.transition));

  if (!source || (disposition != CURRENT_TAB &&
                  disposition != NEW_FOREGROUND_TAB &&
                  disposition != NEW_BACKGROUND_TAB &&
                  disposition != OFF_THE_RECORD)) {
    NOTIMPLEMENTED();
    return NULL;
  }

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return WebContentsDelegate::OpenURLFromTab(source, params);

  if (disposition == NEW_FOREGROUND_TAB ||
      disposition == NEW_BACKGROUND_TAB ||
      disposition == OFF_THE_RECORD) {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jstring> java_url =
        ConvertUTF8ToJavaString(env, url.spec());
    Java_WebContentsDelegateAndroid_openNewTab(env,
                                               obj.obj(),
                                               java_url.obj(),
                                               disposition == OFF_THE_RECORD);
    return NULL;
  }

  // TODO(mkosiba): This should be in platform_utils OpenExternal, b/6174564.
  if (transition == content::PAGE_TRANSITION_LINK && ShouldOverrideLoading(url))
    return NULL;

  source->GetController().LoadURL(url, params.referrer, transition,
                                  std::string());
  return source;
}

// ShouldIgnoreNavigation will be called for every non-local top level
// navigation made by the renderer. If true is returned the renderer will
// not perform the navigation. This is done by using synchronous IPC so we
// should avoid blocking calls from this method.
bool WebContentsDelegateAndroid::ShouldIgnoreNavigation(
    WebContents* source,
    const GURL& url,
    const content::Referrer& referrer,
    WindowOpenDisposition disposition,
    content::PageTransition transition_type) {

  // Don't override new tabs.
  if (disposition == NEW_FOREGROUND_TAB ||
      disposition == NEW_BACKGROUND_TAB ||
      disposition == OFF_THE_RECORD)
    return false;

  return ShouldOverrideLoading(url);
}

void WebContentsDelegateAndroid::NavigationStateChanged(
    const WebContents* source, unsigned changed_flags) {
  if (changed_flags & (
      content::INVALIDATE_TYPE_TAB | content::INVALIDATE_TYPE_TITLE)) {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
    if (obj.is_null())
      return;
    Java_WebContentsDelegateAndroid_onTabHeaderStateChanged(
        env, obj.obj());
  }
}

void WebContentsDelegateAndroid::AddNewContents(
    WebContents* source,
    WebContents* new_contents,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_pos,
    bool user_gesture,
    bool* was_blocked) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  bool handled = false;
  if (!obj.is_null()) {
    handled = Java_WebContentsDelegateAndroid_addNewContents(
        env,
        obj.obj(),
        reinterpret_cast<jint>(source),
        reinterpret_cast<jint>(new_contents),
        static_cast<jint>(disposition),
        NULL,
        user_gesture);
  }
  if (!handled)
    delete new_contents;
}

void WebContentsDelegateAndroid::ActivateContents(WebContents* contents) {
  // TODO(dtrainor) When doing the merge I came across this.  Should we be
  // activating this tab here?
}

void WebContentsDelegateAndroid::DeactivateContents(WebContents* contents) {
  // Do nothing.
}

void WebContentsDelegateAndroid::LoadingStateChanged(WebContents* source) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  bool has_stopped = source == NULL || !source->IsLoading();

  if (has_stopped)
    Java_WebContentsDelegateAndroid_onLoadStopped(env, obj.obj());
  else
    Java_WebContentsDelegateAndroid_onLoadStarted(env, obj.obj());
}

void WebContentsDelegateAndroid::LoadProgressChanged(WebContents* source,
                                                     double progress) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_WebContentsDelegateAndroid_onLoadProgressChanged(
      env,
      obj.obj(),
      progress);
}

void WebContentsDelegateAndroid::CloseContents(WebContents* source) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_WebContentsDelegateAndroid_closeContents(env, obj.obj());
}

void WebContentsDelegateAndroid::MoveContents(WebContents* source,
                                              const gfx::Rect& pos) {
  // Do nothing.
}

bool WebContentsDelegateAndroid::AddMessageToConsole(
    WebContents* source,
    int32 level,
    const string16& message,
    int32 line_no,
    const string16& source_id) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return WebContentsDelegate::AddMessageToConsole(source, level, message,
                                                    line_no, source_id);
  ScopedJavaLocalRef<jstring> jmessage(ConvertUTF16ToJavaString(env, message));
  ScopedJavaLocalRef<jstring> jsource_id(
      ConvertUTF16ToJavaString(env, source_id));
  int jlevel = WEB_CONTENTS_DELEGATE_LOG_LEVEL_TIP;
  switch (level) {
    case logging::LOG_VERBOSE:
      jlevel = WEB_CONTENTS_DELEGATE_LOG_LEVEL_TIP;
      break;
    case logging::LOG_INFO:
      jlevel = WEB_CONTENTS_DELEGATE_LOG_LEVEL_LOG;
      break;
    case logging::LOG_WARNING:
      jlevel = WEB_CONTENTS_DELEGATE_LOG_LEVEL_WARNING;
      break;
    case logging::LOG_ERROR:
      jlevel = WEB_CONTENTS_DELEGATE_LOG_LEVEL_ERROR;
      break;
    default:
      NOTREACHED();
  }
  return Java_WebContentsDelegateAndroid_addMessageToConsole(
      env,
      GetJavaDelegate(env).obj(),
      jlevel,
      jmessage.obj(),
      line_no,
      jsource_id.obj());
}

// TODO(merge): WARNING! method no longer available on the base class.
// See http://b/issue?id=5862108
void WebContentsDelegateAndroid::URLStarredChanged(WebContents* source,
                                                   bool starred) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  Java_WebContentsDelegateAndroid_onUrlStarredChanged(env, obj.obj(), starred);
}

// This is either called from TabContents::DidNavigateMainFramePostCommit() with
// an empty GURL or responding to RenderViewHost::OnMsgUpateTargetURL(). In
// Chrome, the latter is not always called, especially not during history
// navigation. So we only handle the first case and pass the source TabContents'
// url to Java to update the UI.
void WebContentsDelegateAndroid::UpdateTargetURL(WebContents* source,
                                                 int32 page_id,
                                                 const GURL& url) {
  if (!url.is_empty())
    return;
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;
  ScopedJavaLocalRef<jstring> java_url =
      ConvertUTF8ToJavaString(env, source->GetURL().spec());
  Java_WebContentsDelegateAndroid_onUpdateUrl(env,
                                              obj.obj(),
                                              java_url.obj());
}

bool WebContentsDelegateAndroid::CanDownload(
    RenderViewHost* source,
    int request_id,
    const std::string& request_method) {
  if (request_method == net::HttpRequestHeaders::kGetMethod) {
    // TODO(leandrogracia): re-enable this when calling DownloadController
    // doesn't introduce a DEPS layering violation.
    // DownloadController::GetInstance()->CreateGETDownload(
    //     source, request_id);
    return false;
  }
  return true;
}

void WebContentsDelegateAndroid::OnStartDownload(WebContents* source,
                                                 DownloadItem* download) {
  // TODO(leandrogracia): re-enable this when calling DownloadController
  // doesn't introduce a DEPS layering violation.
  // DownloadController::GetInstance()->OnPostDownloadStarted(
  //     source, download);
}

bool WebContentsDelegateAndroid::ShouldOverrideLoading(const GURL& url) {
  if (!url.is_valid())
    return false;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return WebContentsDelegate::ShouldOverrideLoading(url);
  ScopedJavaLocalRef<jstring> jstring_url =
      ConvertUTF8ToJavaString(env, url.spec());
  bool ret = Java_WebContentsDelegateAndroid_shouldOverrideUrlLoading(
      env, obj.obj(), jstring_url.obj());
  return ret;
}

void WebContentsDelegateAndroid::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  jobject key_event = event.os_event;
  if (key_event) {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
    if (obj.is_null())
      return;
    Java_WebContentsDelegateAndroid_handleKeyboardEvent(
        env, obj.obj(), key_event);
  }
}

bool WebContentsDelegateAndroid::TakeFocus(WebContents* source, bool reverse) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return WebContentsDelegate::TakeFocus(source, reverse);
  return Java_WebContentsDelegateAndroid_takeFocus(
      env, obj.obj(), reverse);
}

JavaScriptDialogCreator*
    WebContentsDelegateAndroid::GetJavaScriptDialogCreator() {
  return javascript_dialog_creator_;
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

// Register native methods

bool RegisterWebContentsDelegateAndroid(JNIEnv* env) {
  if (!HasClass(env, kWebContentsDelegateAndroidClassPath)) {
    DLOG(ERROR) << "Unable to find class WebContentsDelegateAndroid!";
    return false;
  }
  return RegisterNativesImpl(env);
}

}  // namespace web_contents_delegate_android
