// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/android/payment_details_deserializer.h"

#include <stdint.h>
#include <cstring>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"

namespace payments {
namespace android {

mojom::PaymentDetailsPtr DeserializePaymentDetails(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& buffer,
    bool* success) {
  DCHECK(success);
  jbyte* buf_in = static_cast<jbyte*>(env->GetDirectBufferAddress(buffer));
  jlong buf_size = env->GetDirectBufferCapacity(buffer);
  std::vector<uint8_t> mojo_buffer(buf_size);
  memcpy(&mojo_buffer[0], buf_in, buf_size);
  mojom::PaymentDetailsPtr details;
  *success =
      mojom::PaymentDetails::Deserialize(std::move(mojo_buffer), &details);
  return details;
}

}  // namespace android
}  // namespace payments
