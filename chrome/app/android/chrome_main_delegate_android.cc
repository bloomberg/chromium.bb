// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/android/chrome_main_delegate_android.h"

#include "base/android/jni_android.h"
#include "chrome/browser/android/chrome_jni_registrar.h"
#include "chrome/browser/android/chrome_startup_flags.h"
#include "content/public/browser/browser_main_runner.h"

// ChromeMainDelegateAndroid is created when the library is loaded. It is always
// done in the process's main Java thread. But for non browser process, e.g.
// renderer process, it is not the native Chrome's main thread.
ChromeMainDelegateAndroid::ChromeMainDelegateAndroid() {
}

ChromeMainDelegateAndroid::~ChromeMainDelegateAndroid() {
}

void ChromeMainDelegateAndroid::SandboxInitialized(
    const std::string& process_type) {
  ChromeMainDelegate::SandboxInitialized(process_type);
}

int ChromeMainDelegateAndroid::RunProcess(
    const std::string& process_type,
    const content::MainFunctionParams& main_function_params) {
  if (process_type.empty()) {
    JNIEnv* env = base::android::AttachCurrentThread();
    RegisterApplicationNativeMethods(env);

    browser_runner_.reset(content::BrowserMainRunner::Create());
    return browser_runner_->Initialize(main_function_params);
  }

  return ChromeMainDelegate::RunProcess(process_type, main_function_params);
}

bool ChromeMainDelegateAndroid::BasicStartupComplete(int* exit_code) {
  SetChromeSpecificCommandLineFlags();
  return ChromeMainDelegate::BasicStartupComplete(exit_code);
}

bool ChromeMainDelegateAndroid::RegisterApplicationNativeMethods(JNIEnv* env) {
  return chrome::android::RegisterJni(env);
}
