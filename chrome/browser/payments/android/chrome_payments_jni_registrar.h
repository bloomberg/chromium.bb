// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAYMENTS_ANDROID_CHROME_PAYMENTS_JNI_REGISTRAR_H_
#define CHROME_BROWSER_PAYMENTS_ANDROID_CHROME_PAYMENTS_JNI_REGISTRAR_H_

#include <jni.h>

namespace payments {
namespace android {

// Register all JNI bindings necessary for the chrome payments classes.
bool RegisterChromePayments(JNIEnv* env);

}  // namespace android
}  // namespace payments

#endif  // CHROME_BROWSER_PAYMENTS_ANDROID_CHROME_PAYMENTS_JNI_REGISTRAR_H_
