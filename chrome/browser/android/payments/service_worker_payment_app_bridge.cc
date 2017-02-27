// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/payments/service_worker_payment_app_bridge.h"

#include <utility>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/payments/content/payment_app.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/payment_app_provider.h"
#include "content/public/browser/web_contents.h"
#include "jni/ServiceWorkerPaymentAppBridge_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using payments::mojom::PaymentAppRequest;
using payments::mojom::PaymentAppRequestPtr;
using payments::mojom::PaymentCurrencyAmount;
using payments::mojom::PaymentDetailsModifier;
using payments::mojom::PaymentDetailsModifierPtr;
using payments::mojom::PaymentItem;
using payments::mojom::PaymentMethodData;
using payments::mojom::PaymentMethodDataPtr;

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
                             const JavaParamRef<jstring>& jorigin,
                             const JavaParamRef<jobjectArray>& jmethod_data,
                             const JavaParamRef<jobject>& jtotal,
                             const JavaParamRef<jobjectArray>& jmodifiers) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);

  PaymentAppRequestPtr app_request = PaymentAppRequest::New();

  app_request->optionId = ConvertJavaStringToUTF8(env, joption_id);
  app_request->origin = GURL(ConvertJavaStringToUTF8(env, jorigin));

  for (jsize i = 0; i < env->GetArrayLength(jmethod_data); i++) {
    ScopedJavaLocalRef<jobject> element(
        env, env->GetObjectArrayElement(jmethod_data, i));
    PaymentMethodDataPtr methodData = PaymentMethodData::New();
    base::android::AppendJavaStringArrayToStringVector(
        env,
        Java_ServiceWorkerPaymentAppBridge_getSupportedMethodsFromMethodData(
            env, element)
            .obj(),
        &methodData->supported_methods);
    methodData->stringified_data = ConvertJavaStringToUTF8(
        env,
        Java_ServiceWorkerPaymentAppBridge_getStringifiedDataFromMethodData(
            env, element));
    app_request->methodData.push_back(std::move(methodData));
  }

  app_request->total = PaymentItem::New();
  app_request->total->label = ConvertJavaStringToUTF8(
      env,
      Java_ServiceWorkerPaymentAppBridge_getLabelFromPaymentItem(env, jtotal));
  app_request->total->amount = PaymentCurrencyAmount::New();
  app_request->total->amount->currency = ConvertJavaStringToUTF8(
      env, Java_ServiceWorkerPaymentAppBridge_getCurrencyFromPaymentItem(
               env, jtotal));
  app_request->total->amount->value = ConvertJavaStringToUTF8(
      env,
      Java_ServiceWorkerPaymentAppBridge_getValueFromPaymentItem(env, jtotal));

  for (jsize i = 0; i < env->GetArrayLength(jmodifiers); i++) {
    ScopedJavaLocalRef<jobject> jmodifier(
        env, env->GetObjectArrayElement(jmodifiers, i));
    PaymentDetailsModifierPtr modifier = PaymentDetailsModifier::New();

    ScopedJavaLocalRef<jobject> jtotal =
        Java_ServiceWorkerPaymentAppBridge_getTotalFromModifier(env, jmodifier);
    modifier->total = PaymentItem::New();
    modifier->total->label = ConvertJavaStringToUTF8(
        env, Java_ServiceWorkerPaymentAppBridge_getLabelFromPaymentItem(
                 env, jtotal));
    modifier->total->amount = PaymentCurrencyAmount::New();
    modifier->total->amount->currency = ConvertJavaStringToUTF8(
        env, Java_ServiceWorkerPaymentAppBridge_getCurrencyFromPaymentItem(
                 env, jtotal));
    modifier->total->amount->value = ConvertJavaStringToUTF8(
        env, Java_ServiceWorkerPaymentAppBridge_getValueFromPaymentItem(
                 env, jtotal));

    ScopedJavaLocalRef<jobject> jmodifier_method_data =
        Java_ServiceWorkerPaymentAppBridge_getMethodDataFromModifier(env,
                                                                     jmodifier);
    modifier->method_data = PaymentMethodData::New();
    base::android::AppendJavaStringArrayToStringVector(
        env,
        Java_ServiceWorkerPaymentAppBridge_getSupportedMethodsFromMethodData(
            env, jmodifier_method_data)
            .obj(),
        &modifier->method_data->supported_methods);
    modifier->method_data->stringified_data = ConvertJavaStringToUTF8(
        env,
        Java_ServiceWorkerPaymentAppBridge_getStringifiedDataFromMethodData(
            env, jmodifier_method_data));

    app_request->modifiers.push_back(std::move(modifier));
  }

  content::PaymentAppProvider::GetInstance()->InvokePaymentApp(
      web_contents->GetBrowserContext(), registration_id,
      std::move(app_request));
}

bool RegisterServiceWorkerPaymentAppBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
