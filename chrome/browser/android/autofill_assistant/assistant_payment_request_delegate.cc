// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/assistant_payment_request_delegate.h"

#include <memory>
#include <utility>

#include "base/android/jni_string.h"
#include "chrome/browser/android/autofill_assistant/ui_controller_android.h"
#include "chrome/browser/autofill/android/personal_data_manager_android.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "jni/AssistantPaymentRequestDelegate_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;

namespace autofill_assistant {

AssistantPaymentRequestDelegate::AssistantPaymentRequestDelegate(
    UiControllerAndroid* ui_controller)
    : ui_controller_(ui_controller) {
  java_assistant_payment_request_delegate_ =
      Java_AssistantPaymentRequestDelegate_create(
          AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
}

AssistantPaymentRequestDelegate::~AssistantPaymentRequestDelegate() {
  Java_AssistantPaymentRequestDelegate_clearNativePtr(
      AttachCurrentThread(), java_assistant_payment_request_delegate_);
}

void AssistantPaymentRequestDelegate::OnGetPaymentInformation(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    jboolean jsucceed,
    const JavaParamRef<jobject>& jcard,
    const JavaParamRef<jobject>& jaddress,
    const JavaParamRef<jstring>& jpayer_name,
    const JavaParamRef<jstring>& jpayer_phone,
    const JavaParamRef<jstring>& jpayer_email,
    jboolean jis_terms_and_services_accepted) {
  std::unique_ptr<PaymentInformation> payment_info =
      std::make_unique<PaymentInformation>();
  payment_info->succeed = jsucceed;
  payment_info->is_terms_and_conditions_accepted =
      jis_terms_and_services_accepted;
  if (payment_info->succeed) {
    if (jcard != nullptr) {
      payment_info->card = std::make_unique<autofill::CreditCard>();
      autofill::PersonalDataManagerAndroid::PopulateNativeCreditCardFromJava(
          jcard, env, payment_info->card.get());

      auto guid = payment_info->card->billing_address_id();
      if (!guid.empty()) {
        autofill::AutofillProfile* profile =
            autofill::PersonalDataManagerFactory::GetForProfile(
                ProfileManager::GetLastUsedProfile())
                ->GetProfileByGUID(guid);
        if (profile != nullptr)
          payment_info->billing_address =
              std::make_unique<autofill::AutofillProfile>(*profile);
      }
    }
    if (jaddress != nullptr) {
      payment_info->shipping_address =
          std::make_unique<autofill::AutofillProfile>();
      autofill::PersonalDataManagerAndroid::PopulateNativeProfileFromJava(
          jaddress, env, payment_info->shipping_address.get());
    }
    if (jpayer_name != nullptr) {
      base::android::ConvertJavaStringToUTF8(env, jpayer_name,
                                             &payment_info->payer_name);
    }
    if (jpayer_phone != nullptr) {
      base::android::ConvertJavaStringToUTF8(env, jpayer_phone,
                                             &payment_info->payer_phone);
    }
    if (jpayer_email != nullptr) {
      base::android::ConvertJavaStringToUTF8(env, jpayer_email,
                                             &payment_info->payer_email);
    }
  }
  ui_controller_->OnGetPaymentInformation(std::move(payment_info));
}

base::android::ScopedJavaGlobalRef<jobject>
AssistantPaymentRequestDelegate::GetJavaObject() {
  return java_assistant_payment_request_delegate_;
}

}  // namespace autofill_assistant
