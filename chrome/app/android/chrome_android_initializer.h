// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_ANDROID_CHROME_ANDROID_INITIALIZER_H_
#define CHROME_APP_ANDROID_CHROME_ANDROID_INITIALIZER_H_

#include <jni.h>

class ChromeMainDelegateAndroid;

jint RunChrome(JavaVM* vm, ChromeMainDelegateAndroid* main_delegate);

#endif  // CHROME_APP_ANDROID_CHROME_ANDROID_INITIALIZER_H_
