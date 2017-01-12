// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_DB_ANDROID_JNI_REGISTRAR_H
#define COMPONENTS_SAFE_BROWSING_DB_ANDROID_JNI_REGISTRAR_H

#include <jni.h>

namespace safe_browsing {
namespace android {

// Register all JNI bindings necessary for chrome browser process.
bool RegisterBrowserJNI(JNIEnv* env);

}  // namespace android
}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_DB_ANDROID_JNI_REGISTRAR_H
