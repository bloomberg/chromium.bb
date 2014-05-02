// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/aw_contents_statics.h"

#include "base/android/scoped_java_ref.h"
#include "base/callback.h"
#include "content/public/browser/browser_thread.h"
#include "jni/AwContentsStatics_jni.h"
#include "net/cert/cert_database.h"

using base::android::AttachCurrentThread;
using base::android::ScopedJavaGlobalRef;
using content::BrowserThread;

namespace android_webview {

namespace {

void ClientCertificatesCleared(ScopedJavaGlobalRef<jobject>* callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  JNIEnv* env = AttachCurrentThread();
  Java_AwContentsStatics_clientCertificatesCleared(env, callback->obj());
}

void NotifyClientCertificatesChanged() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  net::CertDatabase::GetInstance()->OnAndroidKeyStoreChanged();
}

}  // namespace

// static
void ClearClientCertPreferences(JNIEnv* env, jclass, jobject callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  ScopedJavaGlobalRef<jobject>* j_callback = new ScopedJavaGlobalRef<jobject>();
  j_callback->Reset(env, callback);
  BrowserThread::PostTaskAndReply(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&NotifyClientCertificatesChanged),
      base::Bind(&ClientCertificatesCleared, base::Owned(j_callback)));
}

bool RegisterAwContentsStatics(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace android_webview
