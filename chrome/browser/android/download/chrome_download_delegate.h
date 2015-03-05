// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_DOWNLOAD_CHROME_DOWNLOAD_DELEGATE_H_
#define CHROME_BROWSER_ANDROID_DOWNLOAD_CHROME_DOWNLOAD_DELEGATE_H_

#include "base/android/jni_android.h"
#include "base/android/jni_weak_ref.h"
#include "base/android/scoped_java_ref.h"

class ChromeDownloadDelegate {
 public:
  static void EnqueueDownloadManagerRequest(jobject chrome_download_delegate,
                                            bool overwrite,
                                            jobject download_info);
};

bool RegisterChromeDownloadDelegate(JNIEnv* env);

#endif  // CHROME_BROWSER_ANDROID_DOWNLOAD_CHROME_DOWNLOAD_DELEGATE_H_
