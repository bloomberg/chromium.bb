// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PROFILES_PROFILE_DOWNLOADER_ANDROID_H_
#define CHROME_BROWSER_ANDROID_PROFILES_PROFILE_DOWNLOADER_ANDROID_H_

#include <jni.h>

// Registers the ProfileDownloader's native methods through JNI.
bool RegisterProfileDownloader(JNIEnv* env);

#endif  // CHROME_BROWSER_ANDROID_PROFILES_PROFILE_DOWNLOADER_ANDROID_H_
