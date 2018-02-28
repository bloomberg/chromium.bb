// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BYTE_ARRAY_INT_CALLBACK_H_
#define CHROME_BROWSER_ANDROID_BYTE_ARRAY_INT_CALLBACK_H_

#include <jni.h>
#include <stddef.h>
#include <stdint.h>

#include "base/android/scoped_java_ref.h"

// Runs the Java |callback| by calling its onResult method and passing the byte
// array and |int_arg| as its arguments.
void RunByteArrayIntCallback(JNIEnv* env,
                             const base::android::JavaRef<jobject>& callback,
                             const uint8_t* byte_array_arg,
                             size_t byte_array_arg_size,
                             int int_arg);

#endif  // CHROME_BROWSER_ANDROID_BYTE_ARRAY_INT_CALLBACK_H_
