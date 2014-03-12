// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_ANDROID_COMPONENT_JNI_REGISTRAR_H_
#define COMPONENTS_DOM_DISTILLER_ANDROID_COMPONENT_JNI_REGISTRAR_H_

#include <jni.h>

namespace dom_distiller {

namespace android {

// Register all JNI bindings necessary for the dom_distiller component.
bool RegisterDomDistiller(JNIEnv* env);

}  // namespace android

}  // namespace dom_distiller

#endif  // COMPONENTS_DOM_DISTILLER_ANDROID_COMPONENT_JNI_REGISTRAR_H_
