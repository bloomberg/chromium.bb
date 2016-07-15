// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_ANDROID_BLIMP_CLIENT_CONTEXT_IMPL_ANDROID_H_
#define BLIMP_CLIENT_CORE_ANDROID_BLIMP_CLIENT_CONTEXT_IMPL_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "base/supports_user_data.h"
#include "blimp/client/core/blimp_client_context_impl.h"

namespace blimp {
namespace client {

// JNI bridge between BlimpClientContextImpl in Java and C++.
class BlimpClientContextImplAndroid : public base::SupportsUserData::Data {
 public:
  static bool RegisterJni(JNIEnv* env);
  static BlimpClientContextImplAndroid* FromJavaObject(JNIEnv* env,
                                                       jobject jobj);
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  explicit BlimpClientContextImplAndroid(
      BlimpClientContextImpl* blimp_client_context_impl);
  ~BlimpClientContextImplAndroid() override;

  base::android::ScopedJavaLocalRef<jobject> CreateBlimpContents(JNIEnv* env,
                                                                 jobject jobj);

 private:
  BlimpClientContextImpl* blimp_client_context_impl_;

  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  DISALLOW_COPY_AND_ASSIGN(BlimpClientContextImplAndroid);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_ANDROID_BLIMP_CLIENT_CONTEXT_IMPL_ANDROID_H_
