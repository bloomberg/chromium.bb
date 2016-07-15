// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BLIMP_BLIMP_CLIENT_CONTEXT_FACTORY_ANDROID_H_
#define CHROME_BROWSER_ANDROID_BLIMP_BLIMP_CLIENT_CONTEXT_FACTORY_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/macros.h"

// Register the BlimpClientContextFactory's native methods through JNI.
bool RegisterBlimpClientContextFactoryJni(JNIEnv* env);

#endif  // CHROME_BROWSER_ANDROID_BLIMP_BLIMP_CLIENT_CONTEXT_FACTORY_ANDROID_H_
