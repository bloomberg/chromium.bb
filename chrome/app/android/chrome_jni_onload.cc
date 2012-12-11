// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "base/android/jni_android.h"
#include "chrome/app/android/chrome_android_initializer.h"
#include "chrome/app/android/chrome_main_delegate_android.h"

// This is called by the VM when the shared library is first loaded.
// Note that this file must be included in the final shared_library build step
// and not in a static library or the JNI_OnLoad symbol won't be included
// because the Android build is compiled with exclude-libs=ALL to reduce symbol
// size.
JNI_EXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
  return RunChrome(vm, ChromeMainDelegateAndroid::Create());
}
