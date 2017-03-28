// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_ANDROID_COMPONENT_JNI_REGISTRAR_H_
#define COMPONENTS_PAYMENTS_CONTENT_ANDROID_COMPONENT_JNI_REGISTRAR_H_

#include <jni.h>

namespace payments {

namespace android {

// Register all JNI bindings necessary for the payments component.
bool RegisterPayments(JNIEnv* env);

}  // namespace android

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_ANDROID_COMPONENT_JNI_REGISTRAR_H_