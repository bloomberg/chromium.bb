// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_CHROME_JNI_REGISTRAR_H_
#define CHROME_BROWSER_ANDROID_CHROME_JNI_REGISTRAR_H_

#include <jni.h>

namespace chrome {
namespace android {

// Register all JNI bindings necessary for chrome.
bool RegisterJni(JNIEnv* env);

} // namespace android
} // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_CHROME_JNI_REGISTRAR_H_
