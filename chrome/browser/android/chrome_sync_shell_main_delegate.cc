// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/chrome_sync_shell_main_delegate.h"

#include "base/android/jni_android.h"
#include "components/sync/test/fake_server/android/fake_server_helper_android.h"

ChromeSyncShellMainDelegate::ChromeSyncShellMainDelegate() {
}

ChromeSyncShellMainDelegate::~ChromeSyncShellMainDelegate() {
}

int ChromeSyncShellMainDelegate::RunProcess(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params) {
  if (process_type.empty()) {
    FakeServerHelperAndroid::Register(base::android::AttachCurrentThread());
  }
  return ChromeMainDelegateAndroid::RunProcess(
      process_type, main_function_params);
}
