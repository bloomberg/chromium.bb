// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/payments/service_worker_payment_app_bridge.h"

#include <utility>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/payment_app_provider.h"
#include "content/public/browser/stored_payment_instrument.h"
#include "content/public/browser/web_contents.h"
#include "jni/ServiceWorkerPaymentAppBridge_jni.h"
#include "third_party/WebKit/public/platform/modules/payments/payment_app.mojom.h"
#include "ui/gfx/android/java_bitmap.h"

namespace {

using ::base::android::AttachCurrentThread;
using ::base::android::ConvertJavaStringToUTF8;
using ::base::android::ConvertUTF8ToJavaString;
using ::base::android::JavaParamRef;
using ::base::android::JavaRef;
using ::base::android::ScopedJavaGlobalRef;
using ::base::android::ScopedJavaLocalRef;
using ::base::android::ToJavaArrayOfStrings;
using ::payments::mojom::PaymentRequestEventData;
using ::payments::mojom::PaymentRequestEventDataPtr;
using ::payments::mojom::PaymentCurrencyAmount;
using ::payments::mojom::PaymentDetailsModifier;
using ::payments::mojom::PaymentDetailsModifierPtr;
using ::payments::mojom::PaymentItem;
using ::payments::mojom::PaymentMethodData;
using ::payments::mojom::PaymentMethodDataPtr;

void OnGotAllPaymentApps(const JavaRef<jobject>& jweb_contents,
                         const JavaRef<jobject>& jcallback,
                         content::PaymentAppProvider::PaymentApps apps) {
  JNIEnv* env = AttachCurrentThread();

  for (const auto& app_info : apps) {
    ScopedJavaLocalRef<jobject> java_instruments =
        Java_ServiceWorkerPaymentAppBridge_createInstrumentList(env);
    for (const auto& instrument : app_info.second) {
      Java_ServiceWorkerPaymentAppBridge_addInstrument(
          env, java_instruments, jweb_contents, instrument->registration_id,
          ConvertUTF8ToJavaString(env, instrument->instrument_key),
          ConvertUTF8ToJavaString(env, instrument->name),
          ToJavaArrayOfStrings(env, instrument->enabled_methods),
          instrument->icon == nullptr
              ? nullptr
              : gfx::ConvertToJavaBitmap(instrument->icon.get()));
    }
    Java_ServiceWorkerPaymentAppBridge_onPaymentAppCreated(
        env, java_instruments, jweb_contents, jcallback);
  }
  Java_ServiceWorkerPaymentAppBridge_onAllPaymentAppsCreated(env, jcallback);
}

void OnPaymentAppInvoked(const JavaRef<jobject>& jweb_contents,
                         const JavaRef<jobject>& jcallback,
                         payments::mojom::PaymentAppResponsePtr app_response) {
  JNIEnv* env = AttachCurrentThread();

  Java_ServiceWorkerPaymentAppBridge_onPaymentAppInvoked(
      env, jcallback, ConvertUTF8ToJavaString(env, app_response->method_name),
      ConvertUTF8ToJavaString(env, app_response->stringified_details));
}

}  // namespace

static void GetAllPaymentApps(JNIEnv* env,
                              const JavaParamRef<jclass>& jcaller,
                              const JavaParamRef<jobject>& jweb_contents,
                              const JavaParamRef<jobject>& jcallback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);

  content::PaymentAppProvider::GetInstance()->GetAllPaymentApps(
      web_contents->GetBrowserContext(),
      base::Bind(&OnGotAllPaymentApps,
                 ScopedJavaGlobalRef<jobject>(env, jweb_contents),
                 ScopedJavaGlobalRef<jobject>(env, jcallback)));
}

static void InvokePaymentApp(
    JNIEnv* env,
    const JavaParamRef<jclass>& jcaller,
    const JavaParamRef<jobject>& jweb_contents,
    jlong registration_id,
    const JavaParamRef<jstring>& jtop_level_origin,
    const JavaParamRef<jstring>& jpayment_request_origin,
    const JavaParamRef<jstring>& jpayment_request_id,
    const JavaParamRef<jobjectArray>& jmethod_data,
    const JavaParamRef<jobject>& jtotal,
    const JavaParamRef<jobjectArray>& jmodifiers,
    const JavaParamRef<jstring>& jinstrument_key,
    const JavaParamRef<jobject>& jcallback) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);

  PaymentRequestEventDataPtr event_data = PaymentRequestEventData::New();

  event_data->top_level_origin =
      GURL(ConvertJavaStringToUTF8(env, jtop_level_origin));
  event_data->payment_request_origin =
      GURL(ConvertJavaStringToUTF8(env, jpayment_request_origin));
  event_data->payment_request_id =
      ConvertJavaStringToUTF8(env, jpayment_request_id);

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
    event_data->method_data.push_back(std::move(methodData));
  }

  event_data->total = PaymentItem::New();
  event_data->total->label = ConvertJavaStringToUTF8(
      env,
      Java_ServiceWorkerPaymentAppBridge_getLabelFromPaymentItem(env, jtotal));
  event_data->total->amount = PaymentCurrencyAmount::New();
  event_data->total->amount->currency = ConvertJavaStringToUTF8(
      env, Java_ServiceWorkerPaymentAppBridge_getCurrencyFromPaymentItem(
               env, jtotal));
  event_data->total->amount->value = ConvertJavaStringToUTF8(
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

    event_data->modifiers.push_back(std::move(modifier));
  }

  event_data->instrument_key = ConvertJavaStringToUTF8(env, jinstrument_key);

  content::PaymentAppProvider::GetInstance()->InvokePaymentApp(
      web_contents->GetBrowserContext(), registration_id, std::move(event_data),
      base::Bind(&OnPaymentAppInvoked,
                 ScopedJavaGlobalRef<jobject>(env, jweb_contents),
                 ScopedJavaGlobalRef<jobject>(env, jcallback)));
}

bool RegisterServiceWorkerPaymentAppBridge(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
