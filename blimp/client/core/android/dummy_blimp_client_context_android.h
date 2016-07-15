// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_ANDROID_DUMMY_BLIMP_CLIENT_CONTEXT_ANDROID_H_
#define BLIMP_CLIENT_CORE_ANDROID_DUMMY_BLIMP_CLIENT_CONTEXT_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "base/supports_user_data.h"
#include "blimp/client/core/dummy_blimp_client_context.h"

namespace blimp {
namespace client {

// JNI bridge between DummyBlimpClientContext in Java and C++.
class DummyBlimpClientContextAndroid : public base::SupportsUserData::Data {
 public:
  static bool RegisterJni(JNIEnv* env);
  static DummyBlimpClientContextAndroid* FromJavaObject(JNIEnv* env,
                                                        jobject jobj);
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  DummyBlimpClientContextAndroid();
  ~DummyBlimpClientContextAndroid() override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  DISALLOW_COPY_AND_ASSIGN(DummyBlimpClientContextAndroid);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_ANDROID_DUMMY_BLIMP_CLIENT_CONTEXT_ANDROID_H_
