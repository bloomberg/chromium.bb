// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/cronet/android/test/cronet_test_util.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "components/cronet/android/cronet_url_request_adapter.h"
#include "components/cronet/android/test/native_test_server.h"
#include "jni/CronetTestUtil_jni.h"
#include "net/url_request/url_request.h"

using base::android::JavaParamRef;

namespace cronet {

jint GetLoadFlags(JNIEnv* env,
                  const JavaParamRef<jclass>& jcaller,
                  const jlong urlRequest) {
  return reinterpret_cast<CronetURLRequestAdapter*>(urlRequest)
      ->GetURLRequestForTesting()
      ->load_flags();
}

bool RegisterCronetTestUtil(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

}  // namespace cronet
