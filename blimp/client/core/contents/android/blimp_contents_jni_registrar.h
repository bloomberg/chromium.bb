// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_CLIENT_CORE_CONTENTS_ANDROID_BLIMP_CONTENTS_JNI_REGISTRAR_H_
#define BLIMP_CLIENT_CORE_CONTENTS_ANDROID_BLIMP_CONTENTS_JNI_REGISTRAR_H_

#include <jni.h>

namespace blimp {
namespace client {

// Register all JNI bindings necessary for the //blimp/client/core/contents.
bool RegisterBlimpContentsJni(JNIEnv* env);

}  // namespace client
}  // namespace blimp

#endif  // BLIMP_CLIENT_CORE_CONTENTS_ANDROID_BLIMP_CONTENTS_JNI_REGISTRAR_H_
