// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_view_client.h"

#include <android/keycodes.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "content/browser/android/content_util.h"
#include "content/browser/android/content_view_impl.h"
#include "content/browser/android/download_controller.h"
#include "content/browser/android/ime_utils.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/common/find_match_rect_android.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/referrer.h"
#include "jni/content_view_client_jni.h"
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

namespace content {

ContentViewClient::ContentViewClient(JNIEnv* env, jobject obj)
    : weak_java_client_(env, obj),
      find_helper_(NULL),
      javascript_dialog_creator_(NULL) {
}

ContentViewClient::~ContentViewClient() {
}

// static
ContentViewClient* ContentViewClient::CreateNativeContentViewClient(
    JNIEnv* env, jobject obj) {
  DCHECK(obj);
  return new ContentViewClient(env, obj);
}

void ContentViewClient::OnInternalPageLoadRequest(
    WebContents* source, const GURL& url) {
  last_requested_navigation_url_ = url;
}


void ContentViewClient::OnPageStarted(const GURL& url) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jstring_url =
      ConvertUTF8ToJavaString(env, url.spec());
  Java_ContentViewClient_onPageStarted(env, weak_java_client_.get(env).obj(),
                                       jstring_url.obj());
}

void ContentViewClient::OnPageFinished(const GURL& url) {
  if (url == last_requested_navigation_url_)
    last_requested_navigation_url_ = GURL::EmptyGURL();

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jstring_url =
      ConvertUTF8ToJavaString(env, url.spec());

  Java_ContentViewClient_onPageFinished(env, weak_java_client_.get(env).obj(),
                                        jstring_url.obj());
  CheckException(env);
}

void ContentViewClient::OnReceivedError(int error_code,
                                       const string16& description,
                                       const GURL& url) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jstring_error_description =
      ConvertUTF8ToJavaString(env, url.spec());
  ScopedJavaLocalRef<jstring> jstring_url =
      ConvertUTF8ToJavaString(env, url.spec());

  Java_ContentViewClient_onReceivedError(
      env, weak_java_client_.get(env).obj(),
      ToContentViewClientError(error_code),
      jstring_error_description.obj(), jstring_url.obj());
}

void ContentViewClient::OnReceivedHttpAuthRequest(
    jobject auth_handler,
    const string16& host,
    const string16& realm) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jstring_host =
      ConvertUTF16ToJavaString(env, host);
  ScopedJavaLocalRef<jstring> jstring_realm =
      ConvertUTF16ToJavaString(env, realm);
  Java_ContentViewClient_onReceivedHttpAuthRequest(
      env, weak_java_client_.get(env).obj(),
      auth_handler,
      jstring_host.obj(),
      jstring_realm.obj());
}

void ContentViewClient::OnDidCommitMainFrame(const GURL& url,
                                             const GURL& base_url) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jstring_url =
      ConvertUTF8ToJavaString(env, url.spec());
  ScopedJavaLocalRef<jstring> jstring_base_url =
      ConvertUTF8ToJavaString(env, base_url.spec());

  Java_ContentViewClient_onMainFrameCommitted(
      env, weak_java_client_.get(env).obj(),
      jstring_url.obj(), jstring_base_url.obj());
}

void ContentViewClient::OnInterstitialShown() {
  JNIEnv* env = AttachCurrentThread();
  Java_ContentViewClient_onInterstitialShown(
      env, weak_java_client_.get(env).obj());
}

void ContentViewClient::OnInterstitialHidden() {
  JNIEnv* env = AttachCurrentThread();
  Java_ContentViewClient_onInterstitialHidden(
      env, weak_java_client_.get(env).obj());
}

void ContentViewClient::SetFindHelper(FindHelper* find_helper) {
  find_helper_ = find_helper;
}

void ContentViewClient::SetJavaScriptDialogCreator(
    JavaScriptDialogCreator* javascript_dialog_creator) {
  javascript_dialog_creator_ = javascript_dialog_creator;
}

bool ContentViewClient::OnJSModalDialog(JavaScriptMessageType type,
                                        bool is_before_unload_dialog,
                                        const GURL& url,
                                        const string16& message,
                                        const string16& default_value) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jurl(ConvertUTF8ToJavaString(env, url.spec()));
  ScopedJavaLocalRef<jstring> jmessage(ConvertUTF16ToJavaString(env, message));

  // Special case for beforeunload dialogs, as that isn't encoded in the
  // |type| of the dialog.
  if (is_before_unload_dialog) {
    return Java_ContentViewClient_onJsBeforeUnload(env,
        weak_java_client_.get(env).obj(),
        jurl.obj(),
        jmessage.obj());
  }

  switch (type) {
    case JAVASCRIPT_MESSAGE_TYPE_ALERT:
      return Java_ContentViewClient_onJsAlert(env,
                                              weak_java_client_.get(env).obj(),
                                              jurl.obj(),
                                              jmessage.obj());

    case JAVASCRIPT_MESSAGE_TYPE_CONFIRM:
      return Java_ContentViewClient_onJsConfirm(env,
          weak_java_client_.get(env).obj(),
          jurl.obj(),
          jmessage.obj());

    case JAVASCRIPT_MESSAGE_TYPE_PROMPT: {
      ScopedJavaLocalRef<jstring> jdefault_value(
          ConvertUTF16ToJavaString(env, default_value));
      return Java_ContentViewClient_onJsPrompt(env,
                                               weak_java_client_.get(env).obj(),
                                               jurl.obj(),
                                               jmessage.obj(),
                                               jdefault_value.obj());
    }

    default:
      NOTREACHED();
      return false;
  }
}

// ----------------------------------------------------------------------------
// WebContentsDelegate methods
// ----------------------------------------------------------------------------

// OpenURLFromTab() will be called when we're performing a browser-intiated
// navigation. The most common scenario for this is opening new tabs (see
// RenderViewImpl::decidePolicyForNavigation for more details).
WebContents* ContentViewClient::OpenURLFromTab(
    WebContents* source,
    const OpenURLParams& params) {
  const GURL& url = params.url;
  WindowOpenDisposition disposition = params.disposition;
  PageTransition transition(
      PageTransitionFromInt(params.transition));

  if (!source || (disposition != CURRENT_TAB &&
                  disposition != NEW_FOREGROUND_TAB &&
                  disposition != NEW_BACKGROUND_TAB &&
                  disposition != OFF_THE_RECORD)) {
    NOTIMPLEMENTED();
    return NULL;
  }

  if (disposition == NEW_FOREGROUND_TAB ||
      disposition == NEW_BACKGROUND_TAB ||
      disposition == OFF_THE_RECORD) {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jstring> java_url =
        ConvertUTF8ToJavaString(env, url.spec());
    Java_ContentViewClient_openNewTab(env,
                                      weak_java_client_.get(env).obj(),
                                      java_url.obj(),
                                      disposition == OFF_THE_RECORD);
    return NULL;
  }

  // TODO(mkosiba): This should be in platform_utils OpenExternal, b/6174564.
  if (transition == PAGE_TRANSITION_LINK && ShouldOverrideLoading(url))
    return NULL;

  source->GetController().LoadURL(url, params.referrer, transition,
                                  std::string());
  return source;
}

// ShouldIgnoreNavigation will be called for every non-local top level
// navigation made by the renderer. If true is returned the renderer will
// not perform the navigation. This is done by using synchronous IPC so we
// should avoid blocking calls from this method.
bool ContentViewClient::ShouldIgnoreNavigation(
    WebContents* source,
    const GURL& url,
    const Referrer& referrer,
    WindowOpenDisposition disposition,
    PageTransition transition_type) {

  // Don't override new tabs.
  if (disposition == NEW_FOREGROUND_TAB ||
      disposition == NEW_BACKGROUND_TAB ||
      disposition == OFF_THE_RECORD)
    return false;

  // Don't override the navigation that has just been requested via the
  // ContentView.loadUrl method.
  if (url == last_requested_navigation_url_) {
    last_requested_navigation_url_ = GURL::EmptyGURL();
    return false;
  }

  return ShouldOverrideLoading(url);
}

void ContentViewClient::NavigationStateChanged(
    const WebContents* source, unsigned changed_flags) {
  if (changed_flags & (
      INVALIDATE_TYPE_TAB | INVALIDATE_TYPE_TITLE)) {
    JNIEnv* env = AttachCurrentThread();
    Java_ContentViewClient_onTabHeaderStateChanged(
        env, weak_java_client_.get(env).obj());
  }
}

void ContentViewClient::AddNewContents(WebContents* source,
                                       WebContents* new_contents,
                                       WindowOpenDisposition disposition,
                                       const gfx::Rect& initial_pos,
                                       bool user_gesture) {
  JNIEnv* env = AttachCurrentThread();
  bool handled = Java_ContentViewClient_addNewContents(
      env,
      weak_java_client_.get(env).obj(),
      reinterpret_cast<jint>(source),
      reinterpret_cast<jint>(new_contents),
      static_cast<jint>(disposition),
      NULL,
      user_gesture);
  if (!handled)
    delete new_contents;
}

void ContentViewClient::ActivateContents(WebContents* contents) {
  // TODO(dtrainor) When doing the merge I came across this.  Should we be
  // activating this tab here?
}

void ContentViewClient::DeactivateContents(WebContents* contents) {
  // Do nothing.
}

void ContentViewClient::LoadingStateChanged(WebContents* source) {
  JNIEnv* env = AttachCurrentThread();
  bool has_stopped = source == NULL || !source->IsLoading();

  if (has_stopped)
    Java_ContentViewClient_onLoadStopped(
        env, weak_java_client_.get(env).obj());
  else
    Java_ContentViewClient_onLoadStarted(
        env, weak_java_client_.get(env).obj());
}

void ContentViewClient::LoadProgressChanged(double progress) {
  JNIEnv* env = AttachCurrentThread();
  Java_ContentViewClient_onLoadProgressChanged(
      env,
      weak_java_client_.get(env).obj(),
      progress);
}

void ContentViewClient::CloseContents(WebContents* source) {
  JNIEnv* env = AttachCurrentThread();
  Java_ContentViewClient_closeContents(env, weak_java_client_.get(env).obj());
}

void ContentViewClient::MoveContents(WebContents* source,
                                    const gfx::Rect& pos) {
  // Do nothing.
}

// TODO(merge): WARNING! method no longer available on the base class.
// See http://b/issue?id=5862108
void ContentViewClient::URLStarredChanged(WebContents* source, bool starred) {
  JNIEnv* env = AttachCurrentThread();
  Java_ContentViewClient_onUrlStarredChanged(env,
                                             weak_java_client_.get(env).obj(),
                                             starred);
}

// This is either called from TabContents::DidNavigateMainFramePostCommit() with
// an empty GURL or responding to RenderViewHost::OnMsgUpateTargetURL(). In
// Chrome, the latter is not always called, especially not during history
// navigation. So we only handle the first case and pass the source TabContents'
// url to Java to update the UI.
void ContentViewClient::UpdateTargetURL(WebContents* source,
                                        int32 page_id,
                                        const GURL& url) {
  if (url.is_empty()) {
    JNIEnv* env = AttachCurrentThread();
    ScopedJavaLocalRef<jstring> java_url =
        ConvertUTF8ToJavaString(env, source->GetURL().spec());
    Java_ContentViewClient_onUpdateUrl(env,
                                       weak_java_client_.get(env).obj(),
                                       java_url.obj());
  }
}

bool ContentViewClient::CanDownload(RenderViewHost* source,
                                    int request_id,
                                    const std::string& request_method) {
  if (request_method == net::HttpRequestHeaders::kGetMethod) {
    DownloadController::GetInstance()->CreateGETDownload(
        source, request_id);
    return false;
  }
  return true;
}

void ContentViewClient::OnStartDownload(WebContents* source,
                                        DownloadItem* download) {
  DownloadController::GetInstance()->OnPostDownloadStarted(
      source, download);
}

void ContentViewClient::FindReply(WebContents* web_contents,
                                  int request_id,
                                  int number_of_matches,
                                  const gfx::Rect& selection_rect,
                                  int active_match_ordinal,
                                  bool final_update) {
  /* TODO(jrg): upstream this; requires
     content/browser/android/find_helper.h to be upstreamed */
}

void ContentViewClient::OnReceiveFindMatchRects(
    int version, const std::vector<FindMatchRect>& rects,
    const FindMatchRect& active_rect) {
  JNIEnv* env = AttachCurrentThread();

  // Constructs an float[] of (left, top, right, bottom) tuples and passes it on
  // to the Java onReceiveFindMatchRects handler which will use it to create
  // RectF objects equivalent to the std::vector<FindMatchRect>.
  ScopedJavaLocalRef<jfloatArray> rect_data(env,
      env->NewFloatArray(rects.size() * 4));
  jfloat* rect_data_floats = env->GetFloatArrayElements(rect_data.obj(), NULL);
  for (size_t i = 0; i < rects.size(); ++i) {
    rect_data_floats[4 * i]     = rects[i].left;
    rect_data_floats[4 * i + 1] = rects[i].top;
    rect_data_floats[4 * i + 2] = rects[i].right;
    rect_data_floats[4 * i + 3] = rects[i].bottom;
  }
  env->ReleaseFloatArrayElements(rect_data.obj(), rect_data_floats, 0);

  ScopedJavaLocalRef<jobject> active_rect_object;
  if (active_rect.left < active_rect.right &&
      active_rect.top < active_rect.bottom) {
    ScopedJavaLocalRef<jclass> rect_clazz =
        GetClass(env, "android/graphics/RectF");
    jmethodID rect_constructor =
        GetMethodID(env, rect_clazz, "<init>", "(FFFF)V");
    active_rect_object.Reset(env, env->NewObject(rect_clazz.obj(),
                                                 rect_constructor,
                                                 active_rect.left,
                                                 active_rect.top,
                                                 active_rect.right,
                                                 active_rect.bottom));
    DCHECK(!active_rect_object.is_null());
  }


  Java_ContentViewClient_onReceiveFindMatchRects(
      env,
      weak_java_client_.get(env).obj(),
      version, rect_data.obj(),
      active_rect_object.obj());
}

bool ContentViewClient::ShouldOverrideLoading(const GURL& url) {
  if (!url.is_valid())
    return false;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jstring_url =
      ConvertUTF8ToJavaString(env, url.spec());
  bool ret = Java_ContentViewClient_shouldOverrideUrlLoading(
      env, weak_java_client_.get(env).obj(), jstring_url.obj());
  return ret;
}

void ContentViewClient::HandleKeyboardEvent(
    const NativeWebKeyboardEvent& event) {
  jobject key_event = KeyEventFromNative(event);
  if (key_event) {
    JNIEnv* env = AttachCurrentThread();
    Java_ContentViewClient_handleKeyboardEvent(
        env,
        weak_java_client_.get(env).obj(),
        key_event);
  }
}

bool ContentViewClient::TakeFocus(bool reverse) {
  JNIEnv* env = AttachCurrentThread();
  return Java_ContentViewClient_takeFocus(env,
                                          weak_java_client_.get(env).obj(),
                                          reverse);
}

ContentViewClientError ContentViewClient::ToContentViewClientError(
    int net_error) {
    // Note: many net::Error constants don't have an obvious mapping.
    // These will be handled by the default case, ERROR_UNKNOWN.
    switch(net_error) {
    case net::ERR_UNSUPPORTED_AUTH_SCHEME:
      return CONTENT_VIEW_CLIENT_ERROR_UNSUPPORTED_AUTH_SCHEME;

    case net::ERR_INVALID_AUTH_CREDENTIALS:
    case net::ERR_MISSING_AUTH_CREDENTIALS:
    case net::ERR_MISCONFIGURED_AUTH_ENVIRONMENT:
      return CONTENT_VIEW_CLIENT_ERROR_AUTHENTICATION;

    case net::ERR_TOO_MANY_REDIRECTS:
      return CONTENT_VIEW_CLIENT_ERROR_REDIRECT_LOOP;

    case net::ERR_UPLOAD_FILE_CHANGED:
      return CONTENT_VIEW_CLIENT_ERROR_FILE_NOT_FOUND;

    case net::ERR_INVALID_URL:
      return CONTENT_VIEW_CLIENT_ERROR_BAD_URL;

    case net::ERR_DISALLOWED_URL_SCHEME:
    case net::ERR_UNKNOWN_URL_SCHEME:
      return CONTENT_VIEW_CLIENT_ERROR_UNSUPPORTED_SCHEME;

    case net::ERR_IO_PENDING:
    case net::ERR_NETWORK_IO_SUSPENDED:
      return CONTENT_VIEW_CLIENT_ERROR_IO;

    case net::ERR_CONNECTION_TIMED_OUT:
    case net::ERR_TIMED_OUT:
      return CONTENT_VIEW_CLIENT_ERROR_TIMEOUT;

    case net::ERR_FILE_TOO_BIG:
      return CONTENT_VIEW_CLIENT_ERROR_FILE;

    case net::ERR_HOST_RESOLVER_QUEUE_TOO_LARGE:
    case net::ERR_INSUFFICIENT_RESOURCES:
    case net::ERR_OUT_OF_MEMORY:
      return CONTENT_VIEW_CLIENT_ERROR_TOO_MANY_REQUESTS;

    case net::ERR_CONNECTION_CLOSED:
    case net::ERR_CONNECTION_RESET:
    case net::ERR_CONNECTION_REFUSED:
    case net::ERR_CONNECTION_ABORTED:
    case net::ERR_CONNECTION_FAILED:
    case net::ERR_SOCKET_NOT_CONNECTED:
      return CONTENT_VIEW_CLIENT_ERROR_CONNECT;

    case net::ERR_INTERNET_DISCONNECTED:
    case net::ERR_ADDRESS_INVALID:
    case net::ERR_ADDRESS_UNREACHABLE:
    case net::ERR_NAME_NOT_RESOLVED:
    case net::ERR_NAME_RESOLUTION_FAILED:
      return CONTENT_VIEW_CLIENT_ERROR_HOST_LOOKUP;

    case net::ERR_SSL_PROTOCOL_ERROR:
    case net::ERR_SSL_CLIENT_AUTH_CERT_NEEDED:
    case net::ERR_TUNNEL_CONNECTION_FAILED:
    case net::ERR_NO_SSL_VERSIONS_ENABLED:
    case net::ERR_SSL_VERSION_OR_CIPHER_MISMATCH:
    case net::ERR_SSL_RENEGOTIATION_REQUESTED:
    case net::ERR_CERT_ERROR_IN_SSL_RENEGOTIATION:
    case net::ERR_BAD_SSL_CLIENT_AUTH_CERT:
    case net::ERR_SSL_NO_RENEGOTIATION:
    case net::ERR_SSL_DECOMPRESSION_FAILURE_ALERT:
    case net::ERR_SSL_BAD_RECORD_MAC_ALERT:
    case net::ERR_SSL_UNSAFE_NEGOTIATION:
    case net::ERR_SSL_WEAK_SERVER_EPHEMERAL_DH_KEY:
    case net::ERR_SSL_CLIENT_AUTH_PRIVATE_KEY_ACCESS_DENIED:
    case net::ERR_SSL_CLIENT_AUTH_CERT_NO_PRIVATE_KEY:
      return CONTENT_VIEW_CLIENT_ERROR_FAILED_SSL_HANDSHAKE;

    case net::ERR_PROXY_AUTH_UNSUPPORTED:
    case net::ERR_PROXY_AUTH_REQUESTED:
    case net::ERR_PROXY_CONNECTION_FAILED:
    case net::ERR_UNEXPECTED_PROXY_AUTH:
      return CONTENT_VIEW_CLIENT_ERROR_PROXY_AUTHENTICATION;

      /* The certificate errors are handled by onReceivedSslError
       * and don't need to be reported here.
       */
    case net::ERR_CERT_COMMON_NAME_INVALID:
    case net::ERR_CERT_DATE_INVALID:
    case net::ERR_CERT_AUTHORITY_INVALID:
    case net::ERR_CERT_CONTAINS_ERRORS:
    case net::ERR_CERT_NO_REVOCATION_MECHANISM:
    case net::ERR_CERT_UNABLE_TO_CHECK_REVOCATION:
    case net::ERR_CERT_REVOKED:
    case net::ERR_CERT_INVALID:
    case net::ERR_CERT_WEAK_SIGNATURE_ALGORITHM:
    case net::ERR_CERT_NOT_IN_DNS:
    case net::ERR_CERT_NON_UNIQUE_NAME:
      return CONTENT_VIEW_CLIENT_ERROR_OK;

    default:
      VLOG(1) << "ContentViewClient::ToContentViewClientError: Unknown "
              << "chromium error: "
              << net_error;
      return CONTENT_VIEW_CLIENT_ERROR_UNKNOWN;
    }
}

JavaScriptDialogCreator* ContentViewClient::GetJavaScriptDialogCreator() {
  return javascript_dialog_creator_;
}

void ContentViewClient::RunFileChooser(
    WebContents* tab, const FileChooserParams& params) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jparams = ToJavaFileChooserParams(env, params);
  Java_ContentViewClient_runFileChooser(env, weak_java_client_.get(env).obj(),
                                        jparams.obj());
}

// ----------------------------------------------------------------------------
// Native JNI methods
// ----------------------------------------------------------------------------

// Register native methods

bool RegisterContentViewClient(JNIEnv* env) {
  if (!HasClass(env, kContentViewClientClassPath)) {
    DLOG(ERROR) << "Unable to find class ContentViewClient!";
    return false;
  }
  return RegisterNativesImpl(env);
}

}  // namespace content
