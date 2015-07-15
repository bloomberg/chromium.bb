// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_ANDROID_COMPONENT_JNI_REGISTRAR_H_
#define COMPONENTS_POLICY_CORE_BROWSER_ANDROID_COMPONENT_JNI_REGISTRAR_H_

#include <jni.h>

#include "components/policy/policy_export.h"

namespace policy {
namespace android {

// Register all JNI bindings necessary for the policy component.
bool POLICY_EXPORT RegisterPolicy(JNIEnv* env);

}  // namespace android
}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_BROWSER_ANDROID_COMPONENT_JNI_REGISTRAR_H_
