// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/preferences/preferences_launcher.h"

#include "jni/PreferencesLauncher_jni.h"

namespace chrome {
namespace android {

void PreferencesLauncher::ShowAutofillSettings() {
  Java_PreferencesLauncher_showAutofillSettings(
      base::android::AttachCurrentThread());
}

}  // android
}  // chrome
