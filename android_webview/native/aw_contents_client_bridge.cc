// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_contents_client_bridge.h"

#include <memory>
#include <utility>

#include "android_webview/common/devtools_instrumentation.h"
#include "android_webview/native/aw_contents.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/client_certificate_delegate.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "grit/components_strings.h"
#include "jni/AwContentsClientBridge_jni.h"
#include "net/cert/x509_certificate.h"
#include "net/ssl/openssl_client_key_store.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_client_cert_type.h"
#include "net/ssl/ssl_platform_key_android.h"
#include "net/ssl/ssl_private_key.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::HasException;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;

namespace android_webview {

namespace {

// Must be called on the I/O thread to record a client certificate
// and its private key in the OpenSSLClientKeyStore.
void RecordClientCertificateKey(net::X509Certificate* client_cert,
                                scoped_refptr<net::SSLPrivateKey> private_key) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  net::OpenSSLClientKeyStore::GetInstance()->RecordClientCertPrivateKey(
      client_cert, std::move(private_key));
}

}  // namespace

AwContentsClientBridge::AwContentsClientBridge(JNIEnv* env,
                                               const JavaRef<jobject>& obj)
    : java_ref_(env, obj) {
  DCHECK(!obj.is_null());
  Java_AwContentsClientBridge_setNativeContentsClientBridge(
      env, obj, reinterpret_cast<intptr_t>(this));
}

AwContentsClientBridge::~AwContentsClientBridge() {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (!obj.is_null()) {
    // Clear the weak reference from the java peer to the native object since
    // it is possible that java object lifetime can exceed the AwContens.
    Java_AwContentsClientBridge_setNativeContentsClientBridge(env, obj, 0);
  }

  for (IDMap<content::ClientCertificateDelegate>::iterator iter(
           &pending_client_cert_request_delegates_);
       !iter.IsAtEnd(); iter.Advance()) {
    delete iter.GetCurrentValue();
  }
}

void AwContentsClientBridge::AllowCertificateError(
    int cert_error,
    net::X509Certificate* cert,
    const GURL& request_url,
    const base::Callback<void(content::CertificateRequestResultType)>& callback,
    bool* cancel_request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  std::string der_string;
  net::X509Certificate::GetDEREncoded(cert->os_cert_handle(), &der_string);
  ScopedJavaLocalRef<jbyteArray> jcert = base::android::ToJavaByteArray(
      env, reinterpret_cast<const uint8_t*>(der_string.data()),
      der_string.length());
  ScopedJavaLocalRef<jstring> jurl(ConvertUTF8ToJavaString(
      env, request_url.spec()));
  // We need to add the callback before making the call to java side,
  // as it may do a synchronous callback prior to returning.
  int request_id = pending_cert_error_callbacks_.Add(
      base::MakeUnique<CertErrorCallback>(callback));
  *cancel_request = !Java_AwContentsClientBridge_allowCertificateError(
      env, obj, cert_error, jcert, jurl, request_id);
  // if the request is cancelled, then cancel the stored callback
  if (*cancel_request) {
    pending_cert_error_callbacks_.Remove(request_id);
  }
}

void AwContentsClientBridge::ProceedSslError(JNIEnv* env,
                                             const JavaRef<jobject>& obj,
                                             jboolean proceed,
                                             jint id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CertErrorCallback* callback = pending_cert_error_callbacks_.Lookup(id);
  if (!callback || callback->is_null()) {
    LOG(WARNING) << "Ignoring unexpected ssl error proceed callback";
    return;
  }
  callback->Run(proceed ? content::CERTIFICATE_REQUEST_RESULT_TYPE_CONTINUE
                        : content::CERTIFICATE_REQUEST_RESULT_TYPE_CANCEL);
  pending_cert_error_callbacks_.Remove(id);
}

// This method is inspired by SelectClientCertificate() in
// chrome/browser/ui/android/ssl_client_certificate_request.cc
void AwContentsClientBridge::SelectClientCertificate(
    net::SSLCertRequestInfo* cert_request_info,
    std::unique_ptr<content::ClientCertificateDelegate> delegate) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Add the callback to id map.
  int request_id =
      pending_client_cert_request_delegates_.Add(delegate.release());
  // Make sure callback is run on error.
  base::ScopedClosureRunner guard(base::Bind(
      &AwContentsClientBridge::HandleErrorInClientCertificateResponse,
      base::Unretained(this),
      request_id));

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  // Build the |key_types| JNI parameter, as a String[]
  std::vector<std::string> key_types;
  for (size_t i = 0; i < cert_request_info->cert_key_types.size(); ++i) {
    switch (cert_request_info->cert_key_types[i]) {
      case net::CLIENT_CERT_RSA_SIGN:
        key_types.push_back("RSA");
        break;
      case net::CLIENT_CERT_ECDSA_SIGN:
        key_types.push_back("ECDSA");
        break;
      default:
        // Ignore unknown types.
        break;
    }
  }

  ScopedJavaLocalRef<jobjectArray> key_types_ref =
      base::android::ToJavaArrayOfStrings(env, key_types);
  if (key_types_ref.is_null()) {
    LOG(ERROR) << "Could not create key types array (String[])";
    return;
  }

  // Build the |encoded_principals| JNI parameter, as a byte[][]
  ScopedJavaLocalRef<jobjectArray> principals_ref =
      base::android::ToJavaArrayOfByteArray(
          env, cert_request_info->cert_authorities);
  if (principals_ref.is_null()) {
    LOG(ERROR) << "Could not create principals array (byte[][])";
    return;
  }

  // Build the |host_name| and |port| JNI parameters, as a String and
  // a jint.
  ScopedJavaLocalRef<jstring> host_name_ref =
      base::android::ConvertUTF8ToJavaString(
          env, cert_request_info->host_and_port.host());

  Java_AwContentsClientBridge_selectClientCertificate(
      env, obj, request_id, key_types_ref, principals_ref, host_name_ref,
      cert_request_info->host_and_port.port());

  // Release the guard.
  ignore_result(guard.Release());
}

// This method is inspired by OnSystemRequestCompletion() in
// chrome/browser/ui/android/ssl_client_certificate_request.cc
void AwContentsClientBridge::ProvideClientCertificateResponse(
    JNIEnv* env,
    const JavaRef<jobject>& obj,
    int request_id,
    const JavaRef<jobjectArray>& encoded_chain_ref,
    const JavaRef<jobject>& private_key_ref) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  content::ClientCertificateDelegate* delegate =
      pending_client_cert_request_delegates_.Lookup(request_id);
  DCHECK(delegate);

  if (encoded_chain_ref.is_null() || private_key_ref.is_null()) {
    LOG(ERROR) << "No client certificate selected";
    pending_client_cert_request_delegates_.Remove(request_id);
    delegate->ContinueWithCertificate(nullptr);
    delete delegate;
    return;
  }

  // Make sure callback is run on error.
  base::ScopedClosureRunner guard(base::Bind(
      &AwContentsClientBridge::HandleErrorInClientCertificateResponse,
      base::Unretained(this),
      request_id));

  // Convert the encoded chain to a vector of strings.
  std::vector<std::string> encoded_chain_strings;
  if (!encoded_chain_ref.is_null()) {
    base::android::JavaArrayOfByteArrayToStringVector(
        env, encoded_chain_ref.obj(), &encoded_chain_strings);
  }

  std::vector<base::StringPiece> encoded_chain;
  for (size_t i = 0; i < encoded_chain_strings.size(); ++i)
    encoded_chain.push_back(encoded_chain_strings[i]);

  // Create the X509Certificate object from the encoded chain.
  scoped_refptr<net::X509Certificate> client_cert(
      net::X509Certificate::CreateFromDERCertChain(encoded_chain));
  if (!client_cert.get()) {
    LOG(ERROR) << "Could not decode client certificate chain";
    return;
  }

  // Create an SSLPrivateKey wrapper for the private key JNI reference.
  scoped_refptr<net::SSLPrivateKey> private_key =
      net::WrapJavaPrivateKey(client_cert.get(), private_key_ref);
  if (!private_key) {
    LOG(ERROR) << "Could not create OpenSSL wrapper for private key";
    return;
  }

  // Release the guard and |pending_client_cert_request_delegates_| references
  // to |delegate|.
  pending_client_cert_request_delegates_.Remove(request_id);
  ignore_result(guard.Release());

  // RecordClientCertificateKey() must be called on the I/O thread,
  // before the delegate is called with the selected certificate on
  // the UI thread.
  content::BrowserThread::PostTaskAndReply(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&RecordClientCertificateKey, base::RetainedRef(client_cert),
                 base::Passed(&private_key)),
      base::Bind(&content::ClientCertificateDelegate::ContinueWithCertificate,
                 base::Owned(delegate), base::RetainedRef(client_cert)));
}

void AwContentsClientBridge::RunJavaScriptDialog(
    content::JavaScriptMessageType message_type,
    const GURL& origin_url,
    const base::string16& message_text,
    const base::string16& default_prompt_text,
    const content::JavaScriptDialogManager::DialogClosedCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    callback.Run(false, base::string16());
    return;
  }

  int callback_id = pending_js_dialog_callbacks_.Add(
      base::MakeUnique<content::JavaScriptDialogManager::DialogClosedCallback>(
          callback));
  ScopedJavaLocalRef<jstring> jurl(
      ConvertUTF8ToJavaString(env, origin_url.spec()));
  ScopedJavaLocalRef<jstring> jmessage(
      ConvertUTF16ToJavaString(env, message_text));

  switch (message_type) {
    case content::JAVASCRIPT_MESSAGE_TYPE_ALERT: {
      devtools_instrumentation::ScopedEmbedderCallbackTask("onJsAlert");
      Java_AwContentsClientBridge_handleJsAlert(env, obj, jurl, jmessage,
                                                callback_id);
      break;
    }
    case content::JAVASCRIPT_MESSAGE_TYPE_CONFIRM: {
      devtools_instrumentation::ScopedEmbedderCallbackTask("onJsConfirm");
      Java_AwContentsClientBridge_handleJsConfirm(env, obj, jurl, jmessage,
                                                  callback_id);
      break;
    }
    case content::JAVASCRIPT_MESSAGE_TYPE_PROMPT: {
      ScopedJavaLocalRef<jstring> jdefault_value(
          ConvertUTF16ToJavaString(env, default_prompt_text));
      devtools_instrumentation::ScopedEmbedderCallbackTask("onJsPrompt");
      Java_AwContentsClientBridge_handleJsPrompt(env, obj, jurl, jmessage,
                                                 jdefault_value, callback_id);
      break;
    }
    default:
       NOTREACHED();
  }
}

void AwContentsClientBridge::RunBeforeUnloadDialog(
    const GURL& origin_url,
    const content::JavaScriptDialogManager::DialogClosedCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null()) {
    callback.Run(false, base::string16());
    return;
  }

  const base::string16 message_text =
      l10n_util::GetStringUTF16(IDS_BEFOREUNLOAD_MESSAGEBOX_MESSAGE);

  int callback_id = pending_js_dialog_callbacks_.Add(
      base::MakeUnique<content::JavaScriptDialogManager::DialogClosedCallback>(
          callback));
  ScopedJavaLocalRef<jstring> jurl(
      ConvertUTF8ToJavaString(env, origin_url.spec()));
  ScopedJavaLocalRef<jstring> jmessage(
      ConvertUTF16ToJavaString(env, message_text));

  devtools_instrumentation::ScopedEmbedderCallbackTask("onJsBeforeUnload");
  Java_AwContentsClientBridge_handleJsBeforeUnload(env, obj, jurl, jmessage,
                                                   callback_id);
}

bool AwContentsClientBridge::ShouldOverrideUrlLoading(const base::string16& url,
                                                      bool has_user_gesture,
                                                      bool is_redirect,
                                                      bool is_main_frame) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return false;
  ScopedJavaLocalRef<jstring> jurl = ConvertUTF16ToJavaString(env, url);
  devtools_instrumentation::ScopedEmbedderCallbackTask(
      "shouldOverrideUrlLoading");
  bool did_override = Java_AwContentsClientBridge_shouldOverrideUrlLoading(
      env, obj, jurl, has_user_gesture, is_redirect, is_main_frame);
  if (HasException(env)) {
    // Tell the chromium message loop to not perform any tasks after the current
    // one - we want to make sure we return to Java cleanly without first making
    // any new JNI calls.
    base::MessageLoopForUI::current()->Abort();
    // If we crashed we don't want to continue the navigation.
    return true;
  }
  return did_override;
}

void AwContentsClientBridge::NewDownload(const GURL& url,
                                         const std::string& user_agent,
                                         const std::string& content_disposition,
                                         const std::string& mime_type,
                                         int64_t content_length) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  ScopedJavaLocalRef<jstring> jstring_url =
      ConvertUTF8ToJavaString(env, url.spec());
  ScopedJavaLocalRef<jstring> jstring_user_agent =
      ConvertUTF8ToJavaString(env, user_agent);
  ScopedJavaLocalRef<jstring> jstring_content_disposition =
      ConvertUTF8ToJavaString(env, content_disposition);
  ScopedJavaLocalRef<jstring> jstring_mime_type =
      ConvertUTF8ToJavaString(env, mime_type);

  Java_AwContentsClientBridge_newDownload(
      env, obj, jstring_url, jstring_user_agent, jstring_content_disposition,
      jstring_mime_type, content_length);
}

void AwContentsClientBridge::NewLoginRequest(const std::string& realm,
                                             const std::string& account,
                                             const std::string& args) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  ScopedJavaLocalRef<jstring> jrealm = ConvertUTF8ToJavaString(env, realm);
  ScopedJavaLocalRef<jstring> jargs = ConvertUTF8ToJavaString(env, args);

  ScopedJavaLocalRef<jstring> jaccount;
  if (!account.empty())
    jaccount = ConvertUTF8ToJavaString(env, account);

  Java_AwContentsClientBridge_newLoginRequest(env, obj, jrealm, jaccount,
                                              jargs);
}

void AwContentsClientBridge::ConfirmJsResult(JNIEnv* env,
                                             const JavaRef<jobject>&,
                                             int id,
                                             const JavaRef<jstring>& prompt) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::JavaScriptDialogManager::DialogClosedCallback* callback =
      pending_js_dialog_callbacks_.Lookup(id);
  if (!callback) {
    LOG(WARNING) << "Unexpected JS dialog confirm. " << id;
    return;
  }
  base::string16 prompt_text;
  if (!prompt.is_null()) {
    prompt_text = ConvertJavaStringToUTF16(env, prompt);
  }
  callback->Run(true, prompt_text);
  pending_js_dialog_callbacks_.Remove(id);
}

void AwContentsClientBridge::CancelJsResult(JNIEnv*,
                                            const JavaRef<jobject>&,
                                            int id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::JavaScriptDialogManager::DialogClosedCallback* callback =
      pending_js_dialog_callbacks_.Lookup(id);
  if (!callback) {
    LOG(WARNING) << "Unexpected JS dialog cancel. " << id;
    return;
  }
  callback->Run(false, base::string16());
  pending_js_dialog_callbacks_.Remove(id);
}

// Use to cleanup if there is an error in client certificate response.
void AwContentsClientBridge::HandleErrorInClientCertificateResponse(
    int request_id) {
  content::ClientCertificateDelegate* delegate =
      pending_client_cert_request_delegates_.Lookup(request_id);
  pending_client_cert_request_delegates_.Remove(request_id);

  delete delegate;
}

bool RegisterAwContentsClientBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android_webview
