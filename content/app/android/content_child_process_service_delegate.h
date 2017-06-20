// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_ANDROID_CONTENT_CHILD_PROCESS_SERVICE_DELEGATE_H_
#define CONTENT_APP_ANDROID_CONTENT_CHILD_PROCESS_SERVICE_DELEGATE_H_

#include <jni.h>

namespace content {
bool RegisterContentChildProcessServiceDelegate(JNIEnv* env);
}  // namespace content

#endif  // CONTENT_APP_ANDROID_CONTENT_CHILD_PROCESS_SERVICE_DELEGATE_H_
