// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/macros.h"
#include "base/numerics/safe_conversions.h"
#include "chrome/browser/payments/android/jni_headers/ServiceWorkerPaymentAppBridge_jni.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_data_service_factory.h"
#include "components/payments/content/payment_event_response_util.h"
#include "components/payments/content/payment_handler_host.h"
#include "components/payments/content/payment_manifest_web_data_service.h"
#include "components/payments/content/service_worker_payment_app_finder.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/payment_app_provider.h"
#include "content/public/browser/web_contents.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/mojom/payments/payment_app.mojom.h"
#include "ui/gfx/android/java_bitmap.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

using ::base::android::AppendJavaStringArrayToStringVector;
using ::base::android::AttachCurrentThread;
using ::base::android::ConvertJavaStringToUTF8;
using ::base::android::ConvertUTF8ToJavaString;
using ::base::android::JavaParamRef;
using ::base::android::JavaRef;
using ::base::android::ScopedJavaGlobalRef;
using ::base::android::ScopedJavaLocalRef;
using ::base::android::ToJavaArrayOfStrings;
using ::base::android::ToJavaIntArray;
using ::payments::mojom::BasicCardNetwork;
using ::payments::mojom::CanMakePaymentEventData;
using ::payments::mojom::CanMakePaymentEventDataPtr;
using ::payments::mojom::PaymentCurrencyAmount;
using ::payments::mojom::PaymentDetailsModifier;
using ::payments::mojom::PaymentDetailsModifierPtr;
using ::payments::mojom::PaymentItem;
using ::payments::mojom::PaymentMethodData;
using ::payments::mojom::PaymentMethodDataPtr;
using ::payments::mojom::PaymentOptions;
using ::payments::mojom::PaymentOptionsPtr;
using ::payments::mojom::PaymentRequestEventData;
using ::payments::mojom::PaymentRequestEventDataPtr;
using ::payments::mojom::PaymentShippingOption;
using ::payments::mojom::PaymentShippingOptionPtr;
using ::payments::mojom::PaymentShippingType;

void OnHasServiceWorkerPaymentAppsResponse(
    const JavaRef<jobject>& jcallback,
    content::PaymentAppProvider::PaymentApps apps) {
  JNIEnv* env = AttachCurrentThread();

  Java_ServiceWorkerPaymentAppBridge_onHasServiceWorkerPaymentApps(
      env, jcallback, apps.size() > 0);
}

void OnGetServiceWorkerPaymentAppsInfo(
    const JavaRef<jobject>& jcallback,
    content::PaymentAppProvider::PaymentApps apps) {
  JNIEnv* env = AttachCurrentThread();

  base::android::ScopedJavaLocalRef<jobject> jappsInfo =
      Java_ServiceWorkerPaymentAppBridge_createPaymentAppsInfo(env);

  for (const auto& app_info : apps) {
    Java_ServiceWorkerPaymentAppBridge_addPaymentAppInfo(
        env, jappsInfo,
        ConvertUTF8ToJavaString(env, app_info.second->scope.host()),
        ConvertUTF8ToJavaString(env, app_info.second->name),
        app_info.second->icon == nullptr
            ? nullptr
            : gfx::ConvertToJavaBitmap(app_info.second->icon.get()));
  }

  Java_ServiceWorkerPaymentAppBridge_onGetServiceWorkerPaymentAppsInfo(
      env, jcallback, jappsInfo);
}

void OnCanMakePayment(const JavaRef<jobject>& jcallback,
                      payments::mojom::CanMakePaymentResponsePtr response) {
  JNIEnv* env = AttachCurrentThread();
  Java_ServiceWorkerPaymentAppBridge_onCanMakePaymentEventResponse(
      env, jcallback,
      ConvertUTF8ToJavaString(
          env, payments::ConvertCanMakePaymentEventResponseTypeToErrorString(
                   response->response_type)),
      response->can_make_payment, response->ready_for_minimal_ui,
      response->account_balance
          ? ConvertUTF8ToJavaString(env, *response->account_balance)
          : nullptr);
}

void OnPaymentAppInvoked(
    const JavaRef<jobject>& jcallback,
    payments::mojom::PaymentHandlerResponsePtr handler_response) {
  JNIEnv* env = AttachCurrentThread();

  base::android::ScopedJavaLocalRef<jobject> jshipping_address =
      handler_response->shipping_address
          ? Java_ServiceWorkerPaymentAppBridge_createShippingAddress(
                env,
                ConvertUTF8ToJavaString(
                    env, handler_response->shipping_address->country),
                base::android::ToJavaArrayOfStrings(
                    env, handler_response->shipping_address->address_line),
                ConvertUTF8ToJavaString(
                    env, handler_response->shipping_address->region),
                ConvertUTF8ToJavaString(
                    env, handler_response->shipping_address->city),
                ConvertUTF8ToJavaString(
                    env,
                    handler_response->shipping_address->dependent_locality),
                ConvertUTF8ToJavaString(
                    env, handler_response->shipping_address->postal_code),
                ConvertUTF8ToJavaString(
                    env, handler_response->shipping_address->sorting_code),
                ConvertUTF8ToJavaString(
                    env, handler_response->shipping_address->organization),
                ConvertUTF8ToJavaString(
                    env, handler_response->shipping_address->recipient),
                ConvertUTF8ToJavaString(
                    env, handler_response->shipping_address->phone))
          : nullptr;

  base::android::ScopedJavaLocalRef<jobject> jpayer_data =
      Java_ServiceWorkerPaymentAppBridge_createPayerData(
          env,
          ConvertUTF8ToJavaString(env,
                                  handler_response->payer_name.value_or("")),
          ConvertUTF8ToJavaString(env,
                                  handler_response->payer_phone.value_or("")),
          ConvertUTF8ToJavaString(env,
                                  handler_response->payer_email.value_or("")),
          jshipping_address,
          ConvertUTF8ToJavaString(
              env, handler_response->shipping_option.value_or("")));

  Java_ServiceWorkerPaymentAppBridge_onPaymentAppInvoked(
      env, jcallback,
      ConvertUTF8ToJavaString(env, handler_response->method_name),
      ConvertUTF8ToJavaString(env, handler_response->stringified_details),
      jpayer_data,
      ConvertUTF8ToJavaString(
          env, payments::ConvertPaymentEventResponseTypeToErrorString(
                   handler_response->response_type)));
}

void OnPaymentAppAborted(const JavaRef<jobject>& jcallback, bool result) {
  JNIEnv* env = AttachCurrentThread();

  Java_ServiceWorkerPaymentAppBridge_onPaymentAppAborted(env, jcallback,
                                                         result);
}

template <typename T>
void ConvertIntsToEnums(const std::vector<int> ints, std::vector<T>* enums) {
  enums->resize(ints.size());
  for (size_t i = 0; i < ints.size(); ++i) {
    enums->at(i) = static_cast<T>(ints.at(i));
  }
}

std::vector<PaymentMethodDataPtr> ConvertPaymentMethodDataFromJavaToNative(
    JNIEnv* env,
    const JavaParamRef<jobjectArray>& jmethod_data) {
  std::vector<PaymentMethodDataPtr> result;
  for (auto element : jmethod_data.ReadElements<jobject>()) {
    PaymentMethodDataPtr method_data_item = PaymentMethodData::New();
    method_data_item->supported_method = ConvertJavaStringToUTF8(
        env,
        Java_ServiceWorkerPaymentAppBridge_getSupportedMethodFromMethodData(
            env, element));

    std::vector<int> supported_network_ints;
    base::android::JavaIntArrayToIntVector(
        env,
        Java_ServiceWorkerPaymentAppBridge_getSupportedNetworksFromMethodData(
            env, element),
        &supported_network_ints);
    ConvertIntsToEnums<BasicCardNetwork>(supported_network_ints,
                                         &method_data_item->supported_networks);

    method_data_item->stringified_data = ConvertJavaStringToUTF8(
        env,
        Java_ServiceWorkerPaymentAppBridge_getStringifiedDataFromMethodData(
            env, element));
    result.push_back(std::move(method_data_item));
  }
  return result;
}

PaymentRequestEventDataPtr ConvertPaymentRequestEventDataFromJavaToNative(
    JNIEnv* env,
    const JavaParamRef<jstring>& jtop_origin,
    const JavaParamRef<jstring>& jpayment_request_origin,
    const JavaParamRef<jstring>& jpayment_request_id,
    const JavaParamRef<jobjectArray>& jmethod_data,
    const JavaParamRef<jobject>& jtotal,
    const JavaParamRef<jobjectArray>& jmodifiers,
    const JavaParamRef<jobject>& jpayment_options,
    const JavaParamRef<jobjectArray>& jshipping_options,
    jlong payment_handler_host) {
  DCHECK_NE(0, payment_handler_host);
  PaymentRequestEventDataPtr event_data = PaymentRequestEventData::New();

  event_data->top_origin = GURL(ConvertJavaStringToUTF8(env, jtop_origin));
  event_data->payment_request_origin =
      GURL(ConvertJavaStringToUTF8(env, jpayment_request_origin));
  event_data->payment_request_id =
      ConvertJavaStringToUTF8(env, jpayment_request_id);
  event_data->method_data =
      ConvertPaymentMethodDataFromJavaToNative(env, jmethod_data);

  event_data->total = PaymentCurrencyAmount::New();
  event_data->total->currency = ConvertJavaStringToUTF8(
      env, Java_ServiceWorkerPaymentAppBridge_getCurrencyFromPaymentItem(
               env, jtotal));
  event_data->total->value = ConvertJavaStringToUTF8(
      env,
      Java_ServiceWorkerPaymentAppBridge_getValueFromPaymentItem(env, jtotal));

  for (auto jmodifier : jmodifiers.ReadElements<jobject>()) {
    PaymentDetailsModifierPtr modifier = PaymentDetailsModifier::New();

    ScopedJavaLocalRef<jobject> jmodifier_total =
        Java_ServiceWorkerPaymentAppBridge_getTotalFromModifier(env, jmodifier);
    modifier->total = PaymentItem::New();
    modifier->total->label = ConvertJavaStringToUTF8(
        env, Java_ServiceWorkerPaymentAppBridge_getLabelFromPaymentItem(
                 env, jmodifier_total));
    modifier->total->amount = PaymentCurrencyAmount::New();
    modifier->total->amount->currency = ConvertJavaStringToUTF8(
        env, Java_ServiceWorkerPaymentAppBridge_getCurrencyFromPaymentItem(
                 env, jmodifier_total));
    modifier->total->amount->value = ConvertJavaStringToUTF8(
        env, Java_ServiceWorkerPaymentAppBridge_getValueFromPaymentItem(
                 env, jmodifier_total));

    ScopedJavaLocalRef<jobject> jmodifier_method_data =
        Java_ServiceWorkerPaymentAppBridge_getMethodDataFromModifier(env,
                                                                     jmodifier);
    modifier->method_data = PaymentMethodData::New();
    modifier->method_data->supported_method = ConvertJavaStringToUTF8(
        env,
        Java_ServiceWorkerPaymentAppBridge_getSupportedMethodFromMethodData(
            env, jmodifier_method_data));
    modifier->method_data->stringified_data = ConvertJavaStringToUTF8(
        env,
        Java_ServiceWorkerPaymentAppBridge_getStringifiedDataFromMethodData(
            env, jmodifier_method_data));

    event_data->modifiers.push_back(std::move(modifier));
  }

  event_data->payment_options = PaymentOptions::New();
  event_data->payment_options->request_shipping =
      Java_ServiceWorkerPaymentAppBridge_getRequestShippingFromPaymentOptions(
          env, jpayment_options);
  event_data->payment_options->request_payer_name =
      Java_ServiceWorkerPaymentAppBridge_getRequestPayerNameFromPaymentOptions(
          env, jpayment_options);
  event_data->payment_options->request_payer_phone =
      Java_ServiceWorkerPaymentAppBridge_getRequestPayerPhoneFromPaymentOptions(
          env, jpayment_options);
  event_data->payment_options->request_payer_email =
      Java_ServiceWorkerPaymentAppBridge_getRequestPayerEmailFromPaymentOptions(
          env, jpayment_options);
  event_data->payment_options->shipping_type = PaymentShippingType::SHIPPING;
  int jshipping_type =
      Java_ServiceWorkerPaymentAppBridge_getShippingTypeFromPaymentOptions(
          env, jpayment_options);
  if (base::checked_cast<int>(PaymentShippingType::DELIVERY) ==
      jshipping_type) {
    event_data->payment_options->shipping_type = PaymentShippingType::DELIVERY;
  } else if (base::checked_cast<int>(PaymentShippingType::PICKUP) ==
             jshipping_type) {
    event_data->payment_options->shipping_type = PaymentShippingType::PICKUP;
  }

  if (event_data->payment_options->request_shipping) {
    event_data->shipping_options = std::vector<PaymentShippingOptionPtr>();
    for (auto jshipping_option : jshipping_options.ReadElements<jobject>()) {
      PaymentShippingOptionPtr shipping_option = PaymentShippingOption::New();
      shipping_option->id = ConvertJavaStringToUTF8(
          env,
          Java_ServiceWorkerPaymentAppBridge_getIdFromPaymentShippingOption(
              env, jshipping_option));
      shipping_option->label = ConvertJavaStringToUTF8(
          env,
          Java_ServiceWorkerPaymentAppBridge_getLabelFromPaymentShippingOption(
              env, jshipping_option));
      ScopedJavaLocalRef<jobject> jshipping_option_amount =
          Java_ServiceWorkerPaymentAppBridge_getAmountFromPaymentShippingOption(
              env, jshipping_option);
      shipping_option->amount = PaymentCurrencyAmount::New();
      shipping_option->amount->currency = ConvertJavaStringToUTF8(
          env,
          Java_ServiceWorkerPaymentAppBridge_getCurrencyFromPaymentCurrencyAmount(
              env, jshipping_option_amount));
      shipping_option->amount->value = ConvertJavaStringToUTF8(
          env,
          Java_ServiceWorkerPaymentAppBridge_getValueFromPaymentCurrencyAmount(
              env, jshipping_option_amount));
      shipping_option->selected =
          Java_ServiceWorkerPaymentAppBridge_getSelectedFromPaymentShippingOption(
              env, jshipping_option);
      event_data->shipping_options->push_back(std::move(shipping_option));
    }
  }

  event_data->payment_handler_host =
      reinterpret_cast<payments::PaymentHandlerHost*>(payment_handler_host)
          ->Bind();

  return event_data;
}

}  // namespace

static void JNI_ServiceWorkerPaymentAppBridge_HasServiceWorkerPaymentApps(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcallback) {
  // Checks whether there is a installed service worker payment app through
  // GetAllPaymentApps.
  content::PaymentAppProvider::GetInstance()->GetAllPaymentApps(
      ProfileManager::GetActiveUserProfile(),
      base::BindOnce(&OnHasServiceWorkerPaymentAppsResponse,
                     ScopedJavaGlobalRef<jobject>(env, jcallback)));
}

static void JNI_ServiceWorkerPaymentAppBridge_GetServiceWorkerPaymentAppsInfo(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcallback) {
  content::PaymentAppProvider::GetInstance()->GetAllPaymentApps(
      ProfileManager::GetActiveUserProfile(),
      base::BindOnce(&OnGetServiceWorkerPaymentAppsInfo,
                     ScopedJavaGlobalRef<jobject>(env, jcallback)));
}

static void JNI_ServiceWorkerPaymentAppBridge_FireCanMakePaymentEvent(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents,
    jlong registration_id,
    const JavaParamRef<jobject>& jservice_worker_scope,
    const JavaParamRef<jstring>& jpayment_request_id,
    const JavaParamRef<jstring>& jtop_origin,
    const JavaParamRef<jstring>& jpayment_request_origin,
    const JavaParamRef<jobjectArray>& jmethod_data,
    const JavaParamRef<jobjectArray>& jmodifiers,
    const JavaParamRef<jstring>& jcurrency,
    const JavaParamRef<jobject>& jcallback) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);

  CanMakePaymentEventDataPtr event_data = CanMakePaymentEventData::New();

  event_data->top_origin = GURL(ConvertJavaStringToUTF8(env, jtop_origin));
  event_data->payment_request_origin =
      GURL(ConvertJavaStringToUTF8(env, jpayment_request_origin));
  event_data->method_data =
      ConvertPaymentMethodDataFromJavaToNative(env, jmethod_data);
  if (!jcurrency.is_null())
    event_data->currency = ConvertJavaStringToUTF8(env, jcurrency);

  for (auto jmodifier : jmodifiers.ReadElements<jobject>()) {
    PaymentDetailsModifierPtr modifier = PaymentDetailsModifier::New();

    ScopedJavaLocalRef<jobject> jmodifier_total =
        Java_ServiceWorkerPaymentAppBridge_getTotalFromModifier(env, jmodifier);
    modifier->total = PaymentItem::New();
    modifier->total->label = ConvertJavaStringToUTF8(
        env, Java_ServiceWorkerPaymentAppBridge_getLabelFromPaymentItem(
                 env, jmodifier_total));
    modifier->total->amount = PaymentCurrencyAmount::New();
    modifier->total->amount->currency = ConvertJavaStringToUTF8(
        env, Java_ServiceWorkerPaymentAppBridge_getCurrencyFromPaymentItem(
                 env, jmodifier_total));
    modifier->total->amount->value = ConvertJavaStringToUTF8(
        env, Java_ServiceWorkerPaymentAppBridge_getValueFromPaymentItem(
                 env, jmodifier_total));

    ScopedJavaLocalRef<jobject> jmodifier_method_data =
        Java_ServiceWorkerPaymentAppBridge_getMethodDataFromModifier(env,
                                                                     jmodifier);
    modifier->method_data = PaymentMethodData::New();
    modifier->method_data->supported_method = ConvertJavaStringToUTF8(
        env,
        Java_ServiceWorkerPaymentAppBridge_getSupportedMethodFromMethodData(
            env, jmodifier_method_data));
    modifier->method_data->stringified_data = ConvertJavaStringToUTF8(
        env,
        Java_ServiceWorkerPaymentAppBridge_getStringifiedDataFromMethodData(
            env, jmodifier_method_data));

    event_data->modifiers.push_back(std::move(modifier));
  }

  content::PaymentAppProvider::GetInstance()->CanMakePayment(
      web_contents->GetBrowserContext(), registration_id,
      url::Origin::Create(
          *url::GURLAndroid::ToNativeGURL(env, jservice_worker_scope)),
      ConvertJavaStringToUTF8(env, jpayment_request_id), std::move(event_data),
      base::BindOnce(&OnCanMakePayment,
                     ScopedJavaGlobalRef<jobject>(env, jcallback)));
}

static void JNI_ServiceWorkerPaymentAppBridge_InvokePaymentApp(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents,
    jlong registration_id,
    const JavaParamRef<jobject>& jservice_worker_scope,
    const JavaParamRef<jstring>& jtop_origin,
    const JavaParamRef<jstring>& jpayment_request_origin,
    const JavaParamRef<jstring>& jpayment_request_id,
    const JavaParamRef<jobjectArray>& jmethod_data,
    const JavaParamRef<jobject>& jtotal,
    const JavaParamRef<jobjectArray>& jmodifiers,
    const JavaParamRef<jobject>& jpayment_options,
    const JavaParamRef<jobjectArray>& jshipping_options,
    jlong payment_handler_host,
    jboolean can_show_own_ui,
    const JavaParamRef<jobject>& jcallback) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);

  auto event_data = ConvertPaymentRequestEventDataFromJavaToNative(
      env, jtop_origin, jpayment_request_origin, jpayment_request_id,
      jmethod_data, jtotal, jmodifiers, jpayment_options, jshipping_options,
      payment_handler_host);

  url::Origin sw_scope_origin = url::Origin::Create(
      *url::GURLAndroid::ToNativeGURL(env, jservice_worker_scope));
  int64_t reg_id = base::checked_cast<int64_t>(registration_id);

  auto* host =
      reinterpret_cast<payments::PaymentHandlerHost*>(payment_handler_host);
  host->set_sw_origin_for_logs(sw_scope_origin);
  host->set_payment_request_id_for_logs(event_data->payment_request_id);
  host->set_registration_id_for_logs(reg_id);

  // TODO(https://crbug.com/1000432): Pass |can_show_own_ui| to the service
  // worker.
  content::PaymentAppProvider::GetInstance()->InvokePaymentApp(
      web_contents->GetBrowserContext(), reg_id, sw_scope_origin,
      std::move(event_data),
      base::BindOnce(&OnPaymentAppInvoked,
                     ScopedJavaGlobalRef<jobject>(env, jcallback)));
}

static void JNI_ServiceWorkerPaymentAppBridge_InstallAndInvokePaymentApp(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents,
    const JavaParamRef<jstring>& jtop_origin,
    const JavaParamRef<jstring>& jpayment_request_origin,
    const JavaParamRef<jstring>& jpayment_request_id,
    const JavaParamRef<jobjectArray>& jmethod_data,
    const JavaParamRef<jobject>& jtotal,
    const JavaParamRef<jobjectArray>& jmodifiers,
    const JavaParamRef<jobject>& jpayment_options,
    const JavaParamRef<jobjectArray>& jshipping_options,
    jlong payment_handler_host,
    const JavaParamRef<jobject>& jcallback,
    const JavaParamRef<jstring>& japp_name,
    const JavaParamRef<jobject>& jicon,
    const JavaParamRef<jobject>& jsw_js_url,
    const JavaParamRef<jobject>& jsw_scope,
    jboolean juse_cache,
    const JavaParamRef<jstring>& jmethod,
    // Flatten supported_delegations to avoid performance penalty.
    jboolean jsupported_delegations_shipping_address,
    jboolean jsupported_delegations_payer_name,
    jboolean jsupported_delegations_payer_email,
    jboolean jsupported_delegations_payer_phone) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);

  SkBitmap icon_bitmap;
  if (jicon) {
    icon_bitmap = gfx::CreateSkBitmapFromJavaBitmap(gfx::JavaBitmap(jicon));
  }

  auto event_data = ConvertPaymentRequestEventDataFromJavaToNative(
      env, jtop_origin, jpayment_request_origin, jpayment_request_id,
      jmethod_data, jtotal, jmodifiers, jpayment_options, jshipping_options,
      payment_handler_host);

  std::unique_ptr<GURL> sw_scope =
      url::GURLAndroid::ToNativeGURL(env, jsw_scope);

  auto* host =
      reinterpret_cast<payments::PaymentHandlerHost*>(payment_handler_host);
  host->set_sw_origin_for_logs(url::Origin::Create(*sw_scope));
  host->set_payment_request_id_for_logs(event_data->payment_request_id);

  content::SupportedDelegations supported_delegations;
  supported_delegations.shipping_address =
      jsupported_delegations_shipping_address;
  supported_delegations.payer_name = jsupported_delegations_payer_name;
  supported_delegations.payer_email = jsupported_delegations_payer_email;
  supported_delegations.payer_phone = jsupported_delegations_payer_phone;

  content::PaymentAppProvider::GetInstance()->InstallAndInvokePaymentApp(
      web_contents, std::move(event_data),
      ConvertJavaStringToUTF8(env, japp_name), icon_bitmap,
      *url::GURLAndroid::ToNativeGURL(env, jsw_js_url), *sw_scope, juse_cache,
      ConvertJavaStringToUTF8(env, jmethod), supported_delegations,
      base::BindOnce(
          &payments::PaymentHandlerHost::set_registration_id_for_logs,
          host->AsWeakPtr()),
      base::BindOnce(&OnPaymentAppInvoked,
                     ScopedJavaGlobalRef<jobject>(env, jcallback)));
}

static void JNI_ServiceWorkerPaymentAppBridge_AbortPaymentApp(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents,
    jlong registration_id,
    const JavaParamRef<jobject>& jservice_worker_scope,
    const JavaParamRef<jstring>& jpayment_request_id,
    const JavaParamRef<jobject>& jcallback) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);

  content::PaymentAppProvider::GetInstance()->AbortPayment(
      web_contents->GetBrowserContext(), registration_id,
      url::Origin::Create(
          *url::GURLAndroid::ToNativeGURL(env, jservice_worker_scope)),
      ConvertJavaStringToUTF8(env, jpayment_request_id),
      base::BindOnce(&OnPaymentAppAborted,
                     ScopedJavaGlobalRef<jobject>(env, jcallback)));
}

static void JNI_ServiceWorkerPaymentAppBridge_OnClosingPaymentAppWindow(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents,
    jint reason) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);

  content::PaymentAppProvider::GetInstance()->OnClosingOpenedWindow(
      web_contents->GetBrowserContext(),
      static_cast<payments::mojom::PaymentEventResponseType>(reason));
}

static jlong
JNI_ServiceWorkerPaymentAppBridge_GetSourceIdForPaymentAppFromScope(
    JNIEnv* env,
    const JavaParamRef<jobject>& jscope) {
  // At this point we know that the payment handler window is open for the
  // payment app associated with this scope. Since this getter is called inside
  // PaymentApp::getUkmSourceId() function which in turn gets called for the
  // invoked app inside PaymentRequestImpl::openPaymentHandlerWindowInternal.
  return content::PaymentAppProvider::GetInstance()
      ->GetSourceIdForPaymentAppFromScope(
          url::GURLAndroid::ToNativeGURL(env, jscope).get()->GetOrigin());
}
