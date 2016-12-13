// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/payments/service_worker_payment_app_bridge.h"

#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/payments/payment_app.mojom.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/storage_partition.h"
#include "jni/ServiceWorkerPaymentAppBridge_jni.h"

using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

static ScopedJavaLocalRef<jobject> GetAllAppManifests(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller) {
  // TODO(tommyt): crbug.com/669876. Initialise the following two variables.
  // At the moment, they are empty, so this function will return an empty
  // list of manifests. We need to hook this function up to the service worker
  // payment apps.
  std::string scope_url;
  std::vector<payments::mojom::PaymentAppManifestPtr> manifests;

  ScopedJavaLocalRef<jobject> java_manifests =
      Java_ServiceWorkerPaymentAppBridge_createManifestList(env);
  for (const auto& manifest : manifests) {
    ScopedJavaLocalRef<jobject> java_manifest =
        Java_ServiceWorkerPaymentAppBridge_createAndAddManifest(
            env, java_manifests, ConvertUTF8ToJavaString(env, scope_url),
            ConvertUTF8ToJavaString(env, manifest->label),
            manifest->icon ? ConvertUTF8ToJavaString(env, *manifest->icon)
                           : nullptr);
    for (const auto& option : manifest->options) {
      ScopedJavaLocalRef<jobject> java_option =
          Java_ServiceWorkerPaymentAppBridge_createAndAddOption(
              env, java_manifest, ConvertUTF8ToJavaString(env, option->id),
              ConvertUTF8ToJavaString(env, option->label),
              option->icon ? ConvertUTF8ToJavaString(env, *option->icon)
                           : nullptr);
      for (const auto& enabled_method : option->enabled_methods) {
        Java_ServiceWorkerPaymentAppBridge_addEnabledMethod(
            env, java_option, ConvertUTF8ToJavaString(env, enabled_method));
      }
    }
  }

  return java_manifests;
}

static void InvokePaymentApp(JNIEnv* env,
                             const JavaParamRef<jclass>& jcaller,
                             const JavaParamRef<jstring>& scopeUrl,
                             const JavaParamRef<jstring>& optionId,
                             const JavaParamRef<jobject>& methodDataMap) {
  NOTIMPLEMENTED();
}

bool RegisterServiceWorkerPaymentAppBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
