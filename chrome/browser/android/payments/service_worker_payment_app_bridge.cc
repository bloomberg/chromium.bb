// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/payments/service_worker_payment_app_bridge.h"

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/payments/payment_app.mojom.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/payment_app_provider.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "jni/ServiceWorkerPaymentAppBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

namespace {

void OnGotAllManifests(const JavaRef<jobject>& jweb_contents,
                       const JavaRef<jobject>& jcallback,
                       content::PaymentAppProvider::Manifests manifests) {
  JNIEnv* env = AttachCurrentThread();

  for (const auto& entry : manifests) {
    ScopedJavaLocalRef<jobject> java_manifest =
        Java_ServiceWorkerPaymentAppBridge_createManifest(
            env, entry.first, ConvertUTF8ToJavaString(env, entry.second->name),
            entry.second->icon
                ? ConvertUTF8ToJavaString(env, *entry.second->icon)
                : nullptr);
    for (const auto& option : entry.second->options) {
      ScopedJavaLocalRef<jobject> java_option =
          Java_ServiceWorkerPaymentAppBridge_createAndAddOption(
              env, java_manifest, ConvertUTF8ToJavaString(env, option->id),
              ConvertUTF8ToJavaString(env, option->name),
              option->icon ? ConvertUTF8ToJavaString(env, *option->icon)
                           : nullptr);
      for (const auto& enabled_method : option->enabled_methods) {
        Java_ServiceWorkerPaymentAppBridge_addEnabledMethod(
            env, java_option, ConvertUTF8ToJavaString(env, enabled_method));
      }
    }

    Java_ServiceWorkerPaymentAppBridge_onGotManifest(env, java_manifest,
                                                     jweb_contents, jcallback);
  }

  Java_ServiceWorkerPaymentAppBridge_onGotAllManifests(env, jcallback);
}

}  // namespace

static void GetAllAppManifests(JNIEnv* env,
                               const JavaParamRef<jclass>& jcaller,
                               const JavaParamRef<jobject>& jweb_contents,
                               const JavaParamRef<jobject>& jcallback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);

  content::PaymentAppProvider::GetInstance()->GetAllManifests(
      web_contents->GetBrowserContext(),
      base::Bind(&OnGotAllManifests,
                 ScopedJavaGlobalRef<jobject>(env, jweb_contents),
                 ScopedJavaGlobalRef<jobject>(env, jcallback)));
}

static void InvokePaymentApp(JNIEnv* env,
                             const JavaParamRef<jclass>& jcaller,
                             const JavaParamRef<jobject>& jweb_contents,
                             jlong registration_id,
                             const JavaParamRef<jstring>& joption_id,
                             const JavaParamRef<jobjectArray>& jmethod_data) {
  // TODO(tommyt): crbug.com/669876. Implement this
  NOTIMPLEMENTED();
}

bool RegisterServiceWorkerPaymentAppBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
