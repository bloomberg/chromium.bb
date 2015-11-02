// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SESSIONS_SESSION_TAB_HELPER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_SESSIONS_SESSION_TAB_HELPER_ANDROID_H_

#include <jni.h>

// Registers the SessionTabHelper's native methods through JNI.
bool RegisterSessionTabHelper(JNIEnv* env);

#endif  // CHROME_BROWSER_ANDROID_SESSIONS_SESSION_TAB_HELPER_ANDROID_H_
