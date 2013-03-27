// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENT_AUTOFILL_BROWSER_ANDROID_COMPONENT_JNI_REGISTRAR_H_
#define COMPONENT_AUTOFILL_BROWSER_ANDROID_COMPONENT_JNI_REGISTRAR_H_

#include <jni.h>

namespace components {

// Register all JNI bindings necessary for the autofill
// component.
bool RegisterAutofillAndroidJni(JNIEnv* env);

}  // namespace components

#endif  // COMPONENT_AUTOFILL_BROWSER_ANDROID_COMPONENT_JNI_REGISTRAR_H_
