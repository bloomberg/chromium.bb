// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/android/chrome_jni_onload.h"

#include "chrome/app/android/chrome_android_initializer.h"
#include "content/public/app/content_jni_onload.h"

namespace android {

bool OnJNIOnLoadInit() {
  if (!content::android::OnJNIOnLoadInit())
    return false;

  return RunChrome();
}

}  // namespace android
