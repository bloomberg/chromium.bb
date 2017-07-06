// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_AW_CONTENTS_CLIENT_BRIDGE_H_
#define ANDROID_WEBVIEW_BROWSER_AW_CONTENTS_CLIENT_BRIDGE_H_

#include <jni.h>
#include <memory>

#include "android_webview/browser/aw_safe_browsing_resource_throttle.h"
#include "android_webview/browser/net/aw_web_resource_request.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/id_map.h"
#include "base/supports_user_data.h"
#include "components/security_interstitials/content/unsafe_resource.h"
#include "content/public/browser/certificate_request_result_type.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/resource_request_info.h"
#include "net/http/http_response_headers.h"

class GURL;

namespace content {
class ClientCertificateDelegate;
class WebContents;
}

namespace net {
class SSLCertRequestInfo;
class X509Certificate;
}

namespace android_webview {

// A class that handles the Java<->Native communication for the
// AwContentsClient. AwContentsClientBridge is created and owned by
// native AwContents class and it only has a weak reference to the
// its Java peer. Since the Java AwContentsClientBridge can have
// indirect refs from the Application (via callbacks) and so can outlive
// webview, this class notifies it before being destroyed and to nullify
// any references.
class AwContentsClientBridge {
 public:
  // Adds the handler to the UserData registry.
  static void Associate(content::WebContents* web_contents,
                        AwContentsClientBridge* handler);
  static AwContentsClientBridge* FromWebContents(
      content::WebContents* web_contents);
  static AwContentsClientBridge* FromWebContentsGetter(
      const content::ResourceRequestInfo::WebContentsGetter&
          web_contents_getter);
  static AwContentsClientBridge* FromID(int render_process_id,
                                        int render_frame_id);
  AwContentsClientBridge(JNIEnv* env,
                         const base::android::JavaRef<jobject>& obj);
  ~AwContentsClientBridge();

  // AwContentsClientBridge implementation
  void AllowCertificateError(
      int cert_error,
      net::X509Certificate* cert,
      const GURL& request_url,
      const base::Callback<void(content::CertificateRequestResultType)>&
          callback,
      bool* cancel_request);
  void SelectClientCertificate(
      net::SSLCertRequestInfo* cert_request_info,
      std::unique_ptr<content::ClientCertificateDelegate> delegate);
  void RunJavaScriptDialog(
      content::JavaScriptDialogType dialog_type,
      const GURL& origin_url,
      const base::string16& message_text,
      const base::string16& default_prompt_text,
      const content::JavaScriptDialogManager::DialogClosedCallback& callback);
  void RunBeforeUnloadDialog(
      const GURL& origin_url,
      const content::JavaScriptDialogManager::DialogClosedCallback& callback);
  bool ShouldOverrideUrlLoading(const base::string16& url,
                                bool has_user_gesture,
                                bool is_redirect,
                                bool is_main_frame);
  void NewDownload(const GURL& url,
                   const std::string& user_agent,
                   const std::string& content_disposition,
                   const std::string& mime_type,
                   int64_t content_length);

  // Called when a new login request is detected. See the documentation for
  // WebViewClient.onReceivedLoginRequest for arguments. Note that |account|
  // may be empty.
  void NewLoginRequest(const std::string& realm,
                       const std::string& account,
                       const std::string& args);

  // Called when a resource loading error has occured (e.g. an I/O error,
  // host name lookup failure etc.)
  void OnReceivedError(const AwWebResourceRequest& request,
                       int error_code,
                       bool safebrowsing_hit);

  void OnSafeBrowsingHit(const AwWebResourceRequest& request,
                         const safe_browsing::SBThreatType& threat_type,
                         const base::Callback<void(
                             AwSafeBrowsingResourceThrottle::SafeBrowsingAction,
                             bool)>& callback);

  // Called when a response from the server is received with status code >= 400.
  void OnReceivedHttpError(
      const AwWebResourceRequest& request,
      const scoped_refptr<const net::HttpResponseHeaders>& response_headers);

  // Methods called from Java.
  void ProceedSslError(JNIEnv* env,
                       const base::android::JavaRef<jobject>& obj,
                       jboolean proceed,
                       jint id);
  void ProvideClientCertificateResponse(
      JNIEnv* env,
      const base::android::JavaRef<jobject>& object,
      jint request_id,
      const base::android::JavaRef<jobjectArray>& encoded_chain_ref,
      const base::android::JavaRef<jobject>& private_key_ref);
  void ConfirmJsResult(JNIEnv*,
                       const base::android::JavaRef<jobject>&,
                       int id,
                       const base::android::JavaRef<jstring>& prompt);
  void CancelJsResult(JNIEnv*, const base::android::JavaRef<jobject>&, int id);

  void TakeSafeBrowsingAction(JNIEnv*,
                              const base::android::JavaRef<jobject>&,
                              int action,
                              bool reporting,
                              int request_id);

 private:
  JavaObjectWeakGlobalRef java_ref_;

  typedef const base::Callback<void(content::CertificateRequestResultType)>
      CertErrorCallback;
  typedef const base::Callback<
      void(AwSafeBrowsingResourceThrottle::SafeBrowsingAction, bool)>
      SafeBrowsingActionCallback;
  IDMap<std::unique_ptr<CertErrorCallback>> pending_cert_error_callbacks_;
  IDMap<std::unique_ptr<SafeBrowsingActionCallback>> safe_browsing_callbacks_;
  IDMap<std::unique_ptr<content::JavaScriptDialogManager::DialogClosedCallback>>
      pending_js_dialog_callbacks_;
  IDMap<std::unique_ptr<content::ClientCertificateDelegate>>
      pending_client_cert_request_delegates_;
};

bool RegisterAwContentsClientBridge(JNIEnv* env);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_AW_CONTENTS_CLIENT_BRIDGE_H_
