// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/native/token_binding_manager_bridge.h"

#include "android_webview/browser/net/token_binding_manager.h"
#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/ec_private_key.h"
#include "jni/AwTokenBindingManager_jni.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ScopedJavaGlobalRef;
using content::BrowserThread;

namespace android_webview {

namespace {

// Provides the key to the Webview client.
void OnKeyReady(const ScopedJavaGlobalRef<jobject>& callback,
                int status,
                crypto::ECPrivateKey* key) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // TODO(sgurun) implement conversion and plumbing the keypair to java.

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AwTokenBindingManager_onKeyReady(env, callback.obj());
}

// Indicates webview client that key deletion is complete.
void OnDeletionComplete(const ScopedJavaGlobalRef<jobject>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (callback.is_null())
    return;
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_AwTokenBindingManager_onDeletionComplete(env, callback.obj());
}

}  // namespace

static void EnableTokenBinding(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  // This needs to be called before starting chromium engine, so no thread
  // checks for UI.
  TokenBindingManager::GetInstance()->enable_token_binding();
}

static void GetTokenBindingKey(JNIEnv* env,
                               const JavaParamRef<jobject>& obj,
                               const JavaParamRef<jstring>& host,
                               const JavaParamRef<jobject>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Store the Java callback in a scoped object and give the ownership to
  // base::Callback
  ScopedJavaGlobalRef<jobject> j_callback;
  j_callback.Reset(env, callback);

  TokenBindingManager::KeyReadyCallback key_callback =
      base::Bind(&OnKeyReady, j_callback);
  TokenBindingManager::GetInstance()->GetKey(ConvertJavaStringToUTF8(env, host),
                                             key_callback);
}

static void DeleteTokenBindingKey(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj,
                                  const JavaParamRef<jstring>& host,
                                  const JavaParamRef<jobject>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Store the Java callback in a scoped object and give the ownership to
  // base::Callback
  ScopedJavaGlobalRef<jobject> j_callback;
  j_callback.Reset(env, callback);
  TokenBindingManager::DeletionCompleteCallback complete_callback =
      base::Bind(&OnDeletionComplete, j_callback);
  TokenBindingManager::GetInstance()->DeleteKey(
      ConvertJavaStringToUTF8(env, host), complete_callback);
}

static void DeleteAllTokenBindingKeys(JNIEnv* env,
                                      const JavaParamRef<jobject>& obj,
                                      const JavaParamRef<jobject>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Store the Java callback in a scoped object and give the ownership to
  // base::Callback
  ScopedJavaGlobalRef<jobject> j_callback;
  j_callback.Reset(env, callback);
  TokenBindingManager::DeletionCompleteCallback complete_callback =
      base::Bind(&OnDeletionComplete, j_callback);
  TokenBindingManager::GetInstance()->DeleteAllKeys(complete_callback);
}

bool RegisterTokenBindingManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // android_webview namespace
