// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_ANDROID_BLIMP_CLIENT_CONTEXT_IMPL_ANDROID_H_
#define BLIMP_CLIENT_CORE_ANDROID_BLIMP_CLIENT_CONTEXT_IMPL_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "blimp/client/core/blimp_client_context_impl.h"

namespace blimp {
namespace client {

// JNI bridge between BlimpClientContextImpl in Java and C++.
class BlimpClientContextImplAndroid : public BlimpClientContextImpl {
 public:
  static bool RegisterJni(JNIEnv* env);
  static BlimpClientContextImplAndroid* FromJavaObject(JNIEnv* env,
                                                       jobject jobj);

  // The |io_thread_task_runner| must be the task runner to use for IO
  // operations.
  // The |file_thread_task_runner| must be the task runner to use for file
  // operations.
  explicit BlimpClientContextImplAndroid(
      scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner);
  ~BlimpClientContextImplAndroid() override;

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  base::android::ScopedJavaLocalRef<jobject> CreateBlimpContentsJava(
      JNIEnv* env,
      jobject jobj);

  // Start authentication flow from Java.
  void ConnectFromJava(JNIEnv* env, jobject jobj);

 protected:
  // BlimpClientContextImpl implementation.
  GURL GetAssignerURL() override;

 private:
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  DISALLOW_COPY_AND_ASSIGN(BlimpClientContextImplAndroid);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_ANDROID_BLIMP_CLIENT_CONTEXT_IMPL_ANDROID_H_
