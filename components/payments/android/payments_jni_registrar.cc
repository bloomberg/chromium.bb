// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/android/payments_jni_registrar.h"

#include <string>
#include <utility>

#include "components/payments/payment_details_validation.h"
#include "components/payments/payment_request.mojom.h"
#include "jni/PaymentValidator_jni.h"

namespace payments {

jboolean ValidatePaymentDetails(
    JNIEnv* env,
    const base::android::JavaParamRef<jclass>& jcaller,
    const base::android::JavaParamRef<jobject>& buffer) {
  jbyte* buf_in = static_cast<jbyte*>(env->GetDirectBufferAddress(buffer));
  jlong buf_size = env->GetDirectBufferCapacity(buffer);
  mojo::Array<uint8_t> mojo_buffer = mojo::Array<uint8_t>::New(buf_size);
  memcpy(&mojo_buffer[0], buf_in, buf_size);
  mojom::PaymentDetailsPtr details;
  if (!mojom::PaymentDetails::Deserialize(std::move(mojo_buffer), &details))
    return false;
  std::string unused_error_message;
  return payments::validatePaymentDetails(details, &unused_error_message);
}

}  // namespace payments

bool RegisterPaymentValidator(JNIEnv* env) {
  return payments::RegisterNativesImpl(env);
}
