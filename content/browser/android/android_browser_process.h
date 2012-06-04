// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_ANDROID_ANDROID_BROWSER_PROCESS_H_
#define CONTENT_APP_ANDROID_ANDROID_BROWSER_PROCESS_H_
#pragma once

#include <jni.h>

namespace content {

bool RegisterAndroidBrowserProcess(JNIEnv* env);

}  // namespace content

#endif  // CONTENT_APP_ANDROID_ANDROID_BROWSER_PROCESS_H_
