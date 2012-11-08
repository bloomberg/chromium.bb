// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_NAVIGATION_INTERCEPTION_COMPONENT_JNI_REGISTRAR_H_
#define CHROME_BROWSER_COMPONENT_NAVIGATION_INTERCEPTION_COMPONENT_JNI_REGISTRAR_H_

#include <jni.h>

namespace navigation_interception {

// Register all JNI bindings necessary for the navigation_interception
// component.
bool RegisterJni(JNIEnv* env);

} // namespace navigation_interception

#endif  // CHROME_BROWSER_COMPONENT_NAVIGATION_INTERCEPTION_COMPONENT_JNI_REGISTRAR_H_
