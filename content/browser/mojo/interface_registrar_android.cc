// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/mojo/interface_registrar_android.h"

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "content/public/browser/android/interface_registry_android.h"
#include "jni/InterfaceRegistrar_jni.h"

namespace content {

// static
void InterfaceRegistrarAndroid::ExposeInterfacesToRenderer(
    InterfaceRegistryAndroid* registry) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_InterfaceRegistrar_exposeInterfacesToRenderer(
      env, registry->GetObj().obj(), base::android::GetApplicationContext());
}

// static
void InterfaceRegistrarAndroid::ExposeInterfacesToFrame(
  InterfaceRegistryAndroid* registry) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_InterfaceRegistrar_exposeInterfacesToFrame(
      env, registry->GetObj().obj(), base::android::GetApplicationContext());
}
}  // namespace content
