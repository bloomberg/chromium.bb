// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/android/sync_shell/chrome_main_delegate_chrome_sync_shell_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_registrar.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "sync/test/fake_server/android/fake_server_helper_android.h"

static const char kDefaultCountryCode[] = "US";

ChromeMainDelegateAndroid* ChromeMainDelegateAndroid::Create() {
  return new ChromeMainDelegateChromeSyncShellAndroid();
}

ChromeMainDelegateChromeSyncShellAndroid::
ChromeMainDelegateChromeSyncShellAndroid() {
}

ChromeMainDelegateChromeSyncShellAndroid::
~ChromeMainDelegateChromeSyncShellAndroid() {
}

bool
ChromeMainDelegateChromeSyncShellAndroid::RegisterApplicationNativeMethods(
    JNIEnv* env) {
  return ChromeMainDelegateAndroid::RegisterApplicationNativeMethods(env) &&
      FakeServerHelperAndroid::Register(env);
}

bool ChromeMainDelegateChromeSyncShellAndroid::BasicStartupComplete(
    int* exit_code) {
  TemplateURLPrepopulateData::InitCountryCode(kDefaultCountryCode);
  return ChromeMainDelegateAndroid::BasicStartupComplete(exit_code);
}

