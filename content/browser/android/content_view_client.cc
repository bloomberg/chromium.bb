// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/content_view_client.h"

#include <android/keycodes.h>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "content/browser/android/content_view_core_impl.h"
#include "content/browser/android/download_controller.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/page_transition_types.h"
#include "content/public/common/referrer.h"
#include "jni/ContentViewClient_jni.h"
#include "net/http/http_request_headers.h"

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
    : weak_java_client_(env, obj) {
}

ContentViewClient::~ContentViewClient() {
}

// static
ContentViewClient* ContentViewClient::CreateNativeContentViewClient(
    JNIEnv* env, jobject obj) {
  DCHECK(obj);
  return new ContentViewClient(env, obj);
}

void ContentViewClient::OnPageStarted(const GURL& url) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_client_.get(env);
  if (obj.is_null())
    return;
  ScopedJavaLocalRef<jstring> jstring_url =
      ConvertUTF8ToJavaString(env, url.spec());
  Java_ContentViewClient_onPageStarted(env, obj.obj(), jstring_url.obj());
}

void ContentViewClient::OnPageFinished(const GURL& url) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_client_.get(env);
  if (obj.is_null())
    return;
  ScopedJavaLocalRef<jstring> jstring_url =
      ConvertUTF8ToJavaString(env, url.spec());

  Java_ContentViewClient_onPageFinished(env, obj.obj(), jstring_url.obj());
  CheckException(env);
}

void ContentViewClient::OnReceivedError(int error_code,
                                        const string16& description,
                                        const GURL& url) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_client_.get(env);
  if (obj.is_null())
    return;
  ScopedJavaLocalRef<jstring> jstring_error_description =
      ConvertUTF8ToJavaString(env, url.spec());
  ScopedJavaLocalRef<jstring> jstring_url =
      ConvertUTF8ToJavaString(env, url.spec());

  Java_ContentViewClient_onReceivedError(
      env, obj.obj(),
      ToContentViewClientError(error_code),
      jstring_error_description.obj(), jstring_url.obj());
}

void ContentViewClient::OnDidCommitMainFrame(const GURL& url,
                                             const GURL& base_url) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_client_.get(env);
  if (obj.is_null())
    return;
  ScopedJavaLocalRef<jstring> jstring_url =
      ConvertUTF8ToJavaString(env, url.spec());
  ScopedJavaLocalRef<jstring> jstring_base_url =
      ConvertUTF8ToJavaString(env, base_url.spec());

  Java_ContentViewClient_onMainFrameCommitted(
      env, obj.obj(),
      jstring_url.obj(), jstring_base_url.obj());
}

void ContentViewClient::OnInterstitialShown() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_client_.get(env);
  if (obj.is_null())
    return;
  Java_ContentViewClient_onInterstitialShown(env, obj.obj());
}

void ContentViewClient::OnInterstitialHidden() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_client_.get(env);
  if (obj.is_null())
    return;
  Java_ContentViewClient_onInterstitialHidden(env, obj.obj());
}

bool ContentViewClient::OnJSModalDialog(JavaScriptMessageType type,
                                        bool is_before_unload_dialog,
                                        const GURL& url,
                                        const string16& message,
                                        const string16& default_value) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = weak_java_client_.get(env);
  if (obj.is_null())
    return false;
  ScopedJavaLocalRef<jstring> jurl(ConvertUTF8ToJavaString(env, url.spec()));
  ScopedJavaLocalRef<jstring> jmessage(ConvertUTF16ToJavaString(env, message));

  // Special case for beforeunload dialogs, as that isn't encoded in the
  // |type| of the dialog.
  if (is_before_unload_dialog) {
    return Java_ContentViewClient_onJsBeforeUnload(
        env, obj.obj(),
        jurl.obj(),
        jmessage.obj());
  }

  switch (type) {
    case JAVASCRIPT_MESSAGE_TYPE_ALERT:
      return Java_ContentViewClient_onJsAlert(env, obj.obj(),
                                              jurl.obj(),
                                              jmessage.obj());

    case JAVASCRIPT_MESSAGE_TYPE_CONFIRM:
      return Java_ContentViewClient_onJsConfirm(env, obj.obj(),
                                                jurl.obj(),
                                                jmessage.obj());

    case JAVASCRIPT_MESSAGE_TYPE_PROMPT: {
      ScopedJavaLocalRef<jstring> jdefault_value(
          ConvertUTF16ToJavaString(env, default_value));
      return Java_ContentViewClient_onJsPrompt(env, obj.obj(),
                                               jurl.obj(),
                                               jmessage.obj(),
                                               jdefault_value.obj());
    }

    default:
      NOTREACHED();
      return false;
  }
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
    case net::ERR_CERT_NON_UNIQUE_NAME:
      return CONTENT_VIEW_CLIENT_ERROR_OK;

    default:
      VLOG(1) << "ContentViewClient::ToContentViewClientError: Unknown "
              << "chromium error: "
              << net_error;
      return CONTENT_VIEW_CLIENT_ERROR_UNKNOWN;
    }
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
