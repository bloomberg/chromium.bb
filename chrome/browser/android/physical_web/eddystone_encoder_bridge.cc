// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/physical_web/eddystone_encoder_bridge.h"

#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "components/physical_web/eddystone/eddystone_encoder.h"
#include "jni/PhysicalWebBroadcastService_jni.h"

static base::android::ScopedJavaLocalRef<jbyteArray> EncodeUrl(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& j_url) {
  /*
   * Exception preparation for illegal arguments.
   */
  jclass exception_class = env->FindClass("java/lang/IllegalArgumentException");

  /*
   * Input Sanitization
   */
  if (j_url.is_null()) {
    env->ThrowNew(exception_class, "URL is null.");
    return NULL;
  }

  std::string c_url;
  base::android::ConvertJavaStringToUTF8(env, j_url, &c_url);
  std::vector<uint8_t> encoded_url;

  int encode_message = physical_web::EncodeUrl(c_url, &encoded_url);

  if (encode_message < 0) {
    std::string err_message = "Error in Eddystone Encoding. Error Code: ";
    err_message += std::to_string(encode_message);
    env->ThrowNew(exception_class, err_message.c_str());
    return NULL;
  }

  return base::android::ToJavaByteArray(env, encoded_url.data(),
                                        encoded_url.size());
}

// Functions For Testing
base::android::ScopedJavaLocalRef<jbyteArray> EncodeUrlForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& j_url) {
  return EncodeUrl(env, jcaller, j_url);
}
