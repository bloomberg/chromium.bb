// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_UMA_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_UMA_BRIDGE_H_

#include <jni.h>

namespace chrome {
namespace android {

// Registers the native methods through jni
bool RegisterUmaBridge(JNIEnv* env);

}  // namespace android
}  // namespace chrome

#endif  // CHROME_BROWSER_ANDROID_UMA_BRIDGE_H_
