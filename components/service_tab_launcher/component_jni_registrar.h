// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICE_TAB_LAUNCHER_COMPONENT_JNI_REGISTRAR_H_
#define COMPONENTS_SERVICE_TAB_LAUNCHER_COMPONENT_JNI_REGISTRAR_H_

#include <jni.h>

namespace service_tab_launcher {

// Register all JNI bindings necessary for the service_tab_launcher component.
bool RegisterServiceTabLauncherJni(JNIEnv* env);

}  // namespace service_tab_launcher

#endif  // COMPONENTS_SERVICE_TAB_LAUNCHER_COMPONENT_JNI_REGISTRAR_H_
