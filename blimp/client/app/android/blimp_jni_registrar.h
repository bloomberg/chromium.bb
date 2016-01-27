// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_APP_ANDROID_BLIMP_JNI_REGISTRAR_H_
#define BLIMP_CLIENT_APP_ANDROID_BLIMP_JNI_REGISTRAR_H_

#include <jni.h>

namespace blimp {
namespace client {

// Registers native method hooks with the Java runtime specified by |env|.
// Returns false if registration fails, in which case native methods are
// unavailable to Java.
bool RegisterBlimpJni(JNIEnv* env);

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_APP_ANDROID_BLIMP_JNI_REGISTRAR_H_
