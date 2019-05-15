// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_DETAILS_DESERIALIZER_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_DETAILS_DESERIALIZER_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"

namespace payments {
namespace android {

// Deserializes |buffer| into mojom::PatymentDetailsPtr and returns the result.
// Sets |success| to "true" on success. The parameters should not be null.
mojom::PaymentDetailsPtr DeserializePaymentDetails(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& buffer,
    bool* success);

}  // namespace android
}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_PAYMENT_DETAILS_DESERIALIZER_H_
