// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file encapsulates the JNI headers generated for ByteArrayIntCallback, so
// that the methods defined in the generated headers only end up in one object
// file. This is similar to //base/android/callback_android.*.

#include "chrome/browser/android/byte_array_int_callback.h"

#include "base/android/jni_array.h"
#include "jni/ByteArrayIntCallback_jni.h"

void RunByteArrayIntCallback(JNIEnv* env,
                             const base::android::JavaRef<jobject>& callback,
                             const uint8_t* byte_array_arg,
                             size_t byte_array_arg_size,
                             int int_arg) {
  Java_ByteArrayIntCallback_onResult(
      env, callback,
      base::android::ToJavaByteArray(env, byte_array_arg, byte_array_arg_size),
      int_arg);
}
