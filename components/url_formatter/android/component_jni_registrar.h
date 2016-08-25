// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_URL_FORMATTER_ANDROID_COMPONENT_JNI_REGISTRAR_H_
#define COMPONENTS_URL_FORMATTER_ANDROID_COMPONENT_JNI_REGISTRAR_H_

#include <jni.h>

namespace url_formatter {

namespace android {

// Register all JNI bindings necessary for the url_formatter component.
bool RegisterUrlFormatter(JNIEnv* env);

}  // namespace android

}  // namespace url_formatter

#endif  // COMPONENTS_URL_FORMATTER_ANDROID_COMPONENT_JNI_REGISTRAR_H_
