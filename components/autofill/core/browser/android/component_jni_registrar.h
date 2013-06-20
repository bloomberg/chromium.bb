// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ANDROID_COMPONENT_JNI_REGISTRAR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ANDROID_COMPONENT_JNI_REGISTRAR_H_

#include <jni.h>

namespace autofill {

// Register all JNI bindings necessary for the autofill
// component.
bool RegisterAutofillAndroidJni(JNIEnv* env);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ANDROID_COMPONENT_JNI_REGISTRAR_H_
