// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_CLIENT_BRIDGE_H_
#define ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_CLIENT_BRIDGE_H_

#include <jni.h>

#include "android_webview/browser/aw_contents_client_bridge_base.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "base/id_map.h"
#include "content/public/browser/javascript_dialog_manager.h"

namespace net {
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
class AwContentsClientBridge : public AwContentsClientBridgeBase {
 public:
  AwContentsClientBridge(JNIEnv* env,
                         const base::android::JavaRef<jobject>& obj);
  ~AwContentsClientBridge() override;

  // AwContentsClientBridgeBase implementation
  void AllowCertificateError(
      int cert_error,
      net::X509Certificate* cert,
      const GURL& request_url,
      const base::Callback<void(content::CertificateRequestResultType)>&
          callback,
      bool* cancel_request) override;
  void SelectClientCertificate(
      net::SSLCertRequestInfo* cert_request_info,
      std::unique_ptr<content::ClientCertificateDelegate> delegate) override;

  void RunJavaScriptDialog(
      content::JavaScriptDialogType dialog_type,
      const GURL& origin_url,
      const base::string16& message_text,
      const base::string16& default_prompt_text,
      const content::JavaScriptDialogManager::DialogClosedCallback& callback)
      override;
  void RunBeforeUnloadDialog(
      const GURL& origin_url,
      const content::JavaScriptDialogManager::DialogClosedCallback& callback)
      override;
  bool ShouldOverrideUrlLoading(const base::string16& url,
                                bool has_user_gesture,
                                bool is_redirect,
                                bool is_main_frame) override;

  void NewDownload(const GURL& url,
                   const std::string& user_agent,
                   const std::string& content_disposition,
                   const std::string& mime_type,
                   int64_t content_length) override;

  void NewLoginRequest(const std::string& realm,
                       const std::string& account,
                       const std::string& args) override;

  void OnReceivedError(const AwWebResourceRequest& request,
                       int error_code) override;

  void OnReceivedHttpError(const AwWebResourceRequest& request,
                           const scoped_refptr<const net::HttpResponseHeaders>&
                               response_headers) override;

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

 private:
  void HandleErrorInClientCertificateResponse(int id);

  JavaObjectWeakGlobalRef java_ref_;

  typedef const base::Callback<void(content::CertificateRequestResultType)>
      CertErrorCallback;
  IDMap<std::unique_ptr<CertErrorCallback>> pending_cert_error_callbacks_;
  IDMap<std::unique_ptr<content::JavaScriptDialogManager::DialogClosedCallback>>
      pending_js_dialog_callbacks_;
  // |pending_client_cert_request_delegates_| owns its pointers, but IDMap
  // doesn't provide Release, so ownership is managed manually.
  IDMap<content::ClientCertificateDelegate*>
      pending_client_cert_request_delegates_;
};

bool RegisterAwContentsClientBridge(JNIEnv* env);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_CLIENT_BRIDGE_H_
