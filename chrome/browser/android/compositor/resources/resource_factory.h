// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_COMPOSITOR_RESOURCES_RESOURCE_FACTORY_H_
#define CHROME_BROWSER_ANDROID_COMPOSITOR_RESOURCES_RESOURCE_FACTORY_H_

#include "base/android/jni_android.h"

namespace android {

bool RegisterResourceFactory(JNIEnv* env);

}  // namespace android

#endif  // CHROME_BROWSER_ANDROID_COMPOSITOR_RESOURCES_RESOURCE_FACTORY_H_
