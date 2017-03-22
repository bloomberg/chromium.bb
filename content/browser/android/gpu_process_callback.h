// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_GPU_PROCESS_CALLBACK_H_
#define CONTENT_BROWSER_ANDROID_GPU_PROCESS_CALLBACK_H_

#include <jni.h>

namespace content {

bool RegisterGpuProcessCallback(JNIEnv* env);

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_GPU_PROCESS_CALLBACK_H_
