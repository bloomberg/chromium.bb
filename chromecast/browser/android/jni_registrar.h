// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_ANDROID_JNI_REGISTRAR_H_
#define CHROMECAST_BROWSER_ANDROID_JNI_REGISTRAR_H_

#include <jni.h>

namespace chromecast {
namespace shell {

// Register all JNI bindings necessary for the Android cast shell.
bool RegisterJni(JNIEnv* env);

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_ANDROID_JNI_REGISTRAR_H_
