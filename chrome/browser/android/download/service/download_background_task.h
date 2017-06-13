// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DOWNLOAD_SERVICE_DOWNLOAD_BACKGROUND_TASK_H_
#define CHROME_BROWSER_ANDROID_DOWNLOAD_SERVICE_DOWNLOAD_BACKGROUND_TASK_H_

#include <jni.h>
#include <memory>

#include "base/android/jni_android.h"
#include "base/macros.h"

namespace download {
namespace android {

bool RegisterDownloadBackgroundTask(JNIEnv* env);

}  // namespace android
}  // namespace download

#endif  // CHROME_BROWSER_ANDROID_DOWNLOAD_SERVICE_DOWNLOAD_BACKGROUND_TASK_H_
