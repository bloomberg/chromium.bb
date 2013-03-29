// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_contents_client_bridge.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/callback.h"
#include "content/public/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "jni/AwContentsClientBridge_jni.h"
#include "net/cert/x509_certificate.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;

namespace android_webview {

AwContentsClientBridge::AwContentsClientBridge(JNIEnv* env, jobject obj)
    : java_ref_(env, obj) {
  DCHECK(obj);
  Java_AwContentsClientBridge_setNativeContentsClientBridge(env, obj,
      reinterpret_cast<jint>(this));
}

AwContentsClientBridge::~AwContentsClientBridge() {
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;
  // Clear the weak reference from the java peer to the native object since
  // it is possible that java object lifetime can exceed the AwContens.
  Java_AwContentsClientBridge_setNativeContentsClientBridge(env, obj.obj(), 0);
}

void AwContentsClientBridge::AllowCertificateError(
    int cert_error,
    net::X509Certificate* cert,
    const GURL& request_url,
    const base::Callback<void(bool)>& callback,
    bool* cancel_request) {

  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  JNIEnv* env = AttachCurrentThread();

  ScopedJavaLocalRef<jobject> obj = java_ref_.get(env);
  if (obj.is_null())
    return;

  std::string der_string;
  net::X509Certificate::GetDEREncoded(cert->os_cert_handle(), &der_string);
  ScopedJavaLocalRef<jbyteArray> jcert = base::android::ToJavaByteArray(env,
      reinterpret_cast<const uint8*>(der_string.data()), der_string.length());
  ScopedJavaLocalRef<jstring> jurl(ConvertUTF8ToJavaString(
      env, request_url.spec()));
  // We need to add the callback before making the call to java side,
  // as it may do a synchronous callback prior to returning.
  int request_id = pending_cert_error_callbacks_.Add(
      new CertErrorCallback(callback));
  *cancel_request = !Java_AwContentsClientBridge_allowCertificateError(env,
      obj.obj(), cert_error, jcert.obj(), jurl.obj(), request_id);
  // if the request is cancelled, then cancel the stored callback
  if (*cancel_request) {
    pending_cert_error_callbacks_.Remove(request_id);
 }
}

void AwContentsClientBridge::ProceedSslError(JNIEnv* env, jobject obj,
                                             jboolean proceed, jint id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  CertErrorCallback* callback = pending_cert_error_callbacks_.Lookup(id);
  if (!callback || callback->is_null()) {
    LOG(WARNING) << "Ignoring unexpected ssl error proceed callback";
    return;
  }
  callback->Run(proceed);
  pending_cert_error_callbacks_.Remove(id);
}

bool RegisterAwContentsClientBridge(JNIEnv* env) {
  return RegisterNativesImpl(env) >= 0;
}

}  // namespace android_webview
