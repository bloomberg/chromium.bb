// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"

#if defined(OS_ANDROID)
#include "base/android/jni_android.h"
#include "base/test/test_file_util.h"
#endif

#if defined(OS_WIN)
#include "base/debug/close_handle_hook_win.h"
#endif

int main(int argc, char** argv) {
#if defined(OS_ANDROID)
  JNIEnv* env = base::android::AttachCurrentThread();
  base::RegisterContentUriTestUtils(env);
#endif
  base::TestSuite test_suite(argc, argv);
#if defined(OS_WIN)
  CHECK(base::debug::InstallHandleHooks());
#endif
  int ret = base::LaunchUnitTests(
      argc, argv,
      base::Bind(&base::TestSuite::Run, base::Unretained(&test_suite)));
#if defined(OS_WIN)
  base::debug::RemoveHandleHooks();
#endif
  return ret;
}
