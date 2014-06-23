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
  AwContentsClientBridge(JNIEnv* env, jobject obj);
  virtual ~AwContentsClientBridge();

  // AwContentsClientBridgeBase implementation
  virtual void AllowCertificateError(int cert_error,
                                     net::X509Certificate* cert,
                                     const GURL& request_url,
                                     const base::Callback<void(bool)>& callback,
                                     bool* cancel_request) OVERRIDE;
  virtual void SelectClientCertificate(
      net::SSLCertRequestInfo* cert_request_info,
      const SelectCertificateCallback& callback) OVERRIDE;

  virtual void RunJavaScriptDialog(
      content::JavaScriptMessageType message_type,
      const GURL& origin_url,
      const base::string16& message_text,
      const base::string16& default_prompt_text,
      const content::JavaScriptDialogManager::DialogClosedCallback& callback)
      OVERRIDE;
  virtual void RunBeforeUnloadDialog(
      const GURL& origin_url,
      const base::string16& message_text,
      const content::JavaScriptDialogManager::DialogClosedCallback& callback)
      OVERRIDE;
  virtual bool ShouldOverrideUrlLoading(const base::string16& url) OVERRIDE;

  // Methods called from Java.
  void ProceedSslError(JNIEnv* env, jobject obj, jboolean proceed, jint id);
  void ProvideClientCertificateResponse(JNIEnv* env, jobject object,
      jint request_id, jobjectArray encoded_chain_ref,
      jobject private_key_ref);
  void ConfirmJsResult(JNIEnv*, jobject, int id, jstring prompt);
  void CancelJsResult(JNIEnv*, jobject, int id);

 private:
  void HandleErrorInClientCertificateResponse(int id);

  JavaObjectWeakGlobalRef java_ref_;

  typedef const base::Callback<void(bool)> CertErrorCallback;
  IDMap<CertErrorCallback, IDMapOwnPointer> pending_cert_error_callbacks_;
  IDMap<content::JavaScriptDialogManager::DialogClosedCallback, IDMapOwnPointer>
      pending_js_dialog_callbacks_;
  IDMap<SelectCertificateCallback, IDMapOwnPointer>
      pending_client_cert_request_callbacks_;
};

bool RegisterAwContentsClientBridge(JNIEnv* env);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_CLIENT_BRIDGE_H_
