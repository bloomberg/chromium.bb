// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PHYSICAL_WEB_EDDYSTONE_ENCODER_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_PHYSICAL_WEB_EDDYSTONE_ENCODER_BRIDGE_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"

using base::android::JavaParamRef;

bool RegisterEddystoneEncoderBridge(JNIEnv* env);

base::android::ScopedJavaLocalRef<jbyteArray> EncodeUrlForTesting(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller,
    const JavaParamRef<jstring>& j_url);

#endif  // CHROME_BROWSER_ANDROID_PHYSICAL_WEB_EDDYSTONE_ENCODER_BRIDGE_H_
