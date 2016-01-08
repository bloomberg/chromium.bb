// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_SESSION_BLIMP_CLIENT_SESSION_ANDROID_H_
#define BLIMP_CLIENT_SESSION_BLIMP_CLIENT_SESSION_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "blimp/client/session/blimp_client_session.h"

namespace blimp {
namespace client {

class BlimpClientSessionAndroid : public BlimpClientSession {
 public:
  static bool RegisterJni(JNIEnv* env);
  static BlimpClientSessionAndroid* FromJavaObject(JNIEnv* env, jobject jobj);

  BlimpClientSessionAndroid(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& jobj);

  // Methods called from Java via JNI.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& jobj);

 private:
  ~BlimpClientSessionAndroid() override;

  // Reference to the Java object which owns this class.
  base::android::ScopedJavaGlobalRef<jobject> java_obj_;

  DISALLOW_COPY_AND_ASSIGN(BlimpClientSessionAndroid);
};

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_SESSION_BLIMP_CLIENT_SESSION_ANDROID_H_
