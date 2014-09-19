// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_browsertest_util.h"
#include "extensions/test/extension_test_message_listener.h"

// Supported on all platforms, but on Windows only if RLZ is enabled.
#if !defined(OS_WIN) || defined(ENABLE_RLZ)

class MusicManagerPrivateTest : public extensions::PlatformAppBrowserTest {
};

IN_PROC_BROWSER_TEST_F(MusicManagerPrivateTest, DeviceIdValueReturned) {
#if defined(OS_MACOSX) || defined(OS_LINUX)
  // Note: Some MacOS/Linux trybots seem to run under VMware, which assigns
  //       MAC addresses that are blacklisted. We still want the test
  //       to succeed in that case.
  const char* custom_arg = "device_id_may_be_undefined";
#else
  const char* custom_arg = NULL;
#endif
  ASSERT_TRUE(RunPlatformAppTestWithArg(
      "platform_apps/music_manager_private/device_id_value_returned",
      custom_arg))
          << message_;
}

#endif
