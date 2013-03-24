// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/test/base/ui_test_utils.h"


IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       ExtensionPointerLockAccessFail) {
  // Test that pointer lock cannot be accessed from an extension without
  // permission.
  ASSERT_TRUE(RunPlatformAppTest("pointer_lock/no_permission")) << message_;
}

// http://crbug.com/223447
#if defined(OS_LINUX)
#define MAYBE_ExtensionPointerLockAccessPass \
    DISABLED_ExtensionPointerLockAccessPass
#else
#define MAYBE_ExtensionPointerLockAccessPass ExtensionPointerLockAccessPass
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTest,
                       MAYBE_ExtensionPointerLockAccessPass) {
  // Test that pointer lock can be accessed from an extension with permission.
  ASSERT_TRUE(RunPlatformAppTest("pointer_lock/has_permission")) << message_;
}
