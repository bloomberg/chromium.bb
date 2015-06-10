// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEB_CONTENTS_FACTORY_H_
#define CHROME_BROWSER_ANDROID_WEB_CONTENTS_FACTORY_H_

#include "base/android/jni_android.h"

// Register the WebContentsFactory's native methods through JNI.
bool RegisterWebContentsFactory(JNIEnv* env);

#endif  // CHROME_BROWSER_ANDROID_WEB_CONTENTS_FACTORY_H_

