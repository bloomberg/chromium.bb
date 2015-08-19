// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/android/sync_shell/chrome_main_delegate_chrome_sync_shell_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "sync/test/fake_server/android/fake_server_helper_android.h"

ChromeMainDelegateAndroid* ChromeMainDelegateAndroid::Create() {
  return new ChromeMainDelegateChromeSyncShellAndroid();
}

ChromeMainDelegateChromeSyncShellAndroid::
ChromeMainDelegateChromeSyncShellAndroid() {
}

ChromeMainDelegateChromeSyncShellAndroid::
~ChromeMainDelegateChromeSyncShellAndroid() {
}

bool ChromeMainDelegateChromeSyncShellAndroid::BasicStartupComplete(
    int* exit_code) {
  return ChromeMainDelegateAndroid::BasicStartupComplete(exit_code);
}

int ChromeMainDelegateChromeSyncShellAndroid::RunProcess(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params) {
  if (process_type.empty()) {
    // This JNI registration can not be moved to JNI_OnLoad because
    // FakeServerHelper seems not known by class loader when shared library
    // is loaded.
    FakeServerHelperAndroid::Register(base::android::AttachCurrentThread());
  }
  return ChromeMainDelegateAndroid::RunProcess(process_type,
                                               main_function_params);
}
