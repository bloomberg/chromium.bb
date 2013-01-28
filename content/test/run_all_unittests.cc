// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/content_test_suite.h"
#include "content/public/test/unittest_test_suite.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "content/browser/android/browser_jni_registrar.h"
#include "content/common/android/common_jni_registrar.h"
#include "net/android/net_jni_registrar.h"
#include "ui/android/ui_jni_registrar.h"
#endif

int main(int argc, char** argv) {

#if defined(OS_ANDROID)
  // Register JNI bindings for android.
  JNIEnv* env = base::android::AttachCurrentThread();
  content::android::RegisterCommonJni(env);
  content::android::RegisterBrowserJni(env);
  net::android::RegisterJni(env);
  ui::android::RegisterJni(env);
#endif

  return content::UnitTestTestSuite(
      new content::ContentTestSuite(argc, argv)).Run();
}
