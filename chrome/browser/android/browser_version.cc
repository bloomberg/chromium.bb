// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/browser_version.h"

#include "chrome/common/chrome_version_info.h"
#include "jni/BrowserVersion_jni.h"

static jboolean IsOfficialBuild(JNIEnv* env, jclass /* clazz */) {
  return chrome::VersionInfo().IsOfficialBuild();
}

bool RegisterBrowserVersion(JNIEnv* env) {
  return RegisterNativesImpl(env);
}
