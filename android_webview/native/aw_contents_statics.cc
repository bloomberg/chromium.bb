// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_contents_statics.h"

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/net/aw_url_request_context_getter.h"
#include "android_webview/common/aw_version_info_values.h"
#include "android_webview/native/aw_contents_io_thread_client_impl.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/url_constants.h"
#include "jni/AwContentsStatics_jni.h"
#include "net/cert/cert_database.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;

namespace android_webview {

namespace {

void ClientCertificatesCleared(const JavaRef<jobject>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  Java_AwContentsStatics_clientCertificatesCleared(env, callback);
}

void NotifyClientCertificatesChanged() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::CertDatabase::GetInstance()->OnAndroidKeyStoreChanged();
}

}  // namespace

// static
void ClearClientCertPreferences(JNIEnv* env,
                                const JavaParamRef<jclass>&,
                                const JavaParamRef<jobject>& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  BrowserThread::PostTaskAndReply(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&NotifyClientCertificatesChanged),
      base::Bind(&ClientCertificatesCleared,
                 ScopedJavaGlobalRef<jobject>(env, callback)));
}

// static
ScopedJavaLocalRef<jstring> GetUnreachableWebDataUrl(
    JNIEnv* env,
    const JavaParamRef<jclass>&) {
  return base::android::ConvertUTF8ToJavaString(
      env, content::kUnreachableWebDataURL);
}

// static
void SetLegacyCacheRemovalDelayForTest(JNIEnv*,
                                       const JavaParamRef<jclass>&,
                                       jlong delay_ms) {
  AwBrowserContext::SetLegacyCacheRemovalDelayForTest(delay_ms);
}

// static
ScopedJavaLocalRef<jstring> GetProductVersion(JNIEnv* env,
                                              const JavaParamRef<jclass>&) {
  return base::android::ConvertUTF8ToJavaString(env, PRODUCT_VERSION);
}

// static
void SetServiceWorkerIoThreadClient(
    JNIEnv* env,
    const JavaParamRef<jclass>&,
    const base::android::JavaParamRef<jobject>& io_thread_client,
    const base::android::JavaParamRef<jobject>& browser_context) {
  AwContentsIoThreadClientImpl::SetServiceWorkerIoThreadClient(
      io_thread_client, browser_context);
}


bool RegisterAwContentsStatics(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android_webview
