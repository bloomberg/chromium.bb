// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEB_CONTENTS_DELEGATE_ANDROID_COMPONENT_JNI_REGISTRAR_H_
#define COMPONENTS_WEB_CONTENTS_DELEGATE_ANDROID_COMPONENT_JNI_REGISTRAR_H_

#include <jni.h>

namespace components {

// Register all JNI bindings necessary for the web_contents_delegate_android
// component.
bool RegisterWebContentsDelegateAndroidJni(JNIEnv* env);

}  // namespace components

#endif  // COMPONENTS_WEB_CONTENTS_DELEGATE_ANDROID_COMPONENT_JNI_REGISTRAR_H_

