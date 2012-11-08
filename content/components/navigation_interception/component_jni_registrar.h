// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMPONENTS_NAVIGATION_INTERCEPTION_COMPONENT_JNI_REGISTRAR_H_
#define CONTENT_COMPONENTS_NAVIGATION_INTERCEPTION_COMPONENT_JNI_REGISTRAR_H_

#include <jni.h>

namespace content {

// Register all JNI bindings necessary for the navigation_interception
// component.
bool RegisterNavigationInterceptionJni(JNIEnv* env);

}  // namespace content

#endif  // CONTENT_COMPONENTS_NAVIGATION_INTERCEPTION_COMPONENT_JNI_REGISTRAR_H_
