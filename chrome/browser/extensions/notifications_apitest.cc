// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"


IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Notifications) {
#if defined(OS_LINUX) && defined(TOOLKIT_VIEWS)
  // Notifications not supported on linux/views yet.
#else
  ASSERT_TRUE(RunExtensionTest("notifications/has_permission")) << message_;
  ASSERT_TRUE(RunExtensionTest("notifications/has_not_permission")) << message_;
#endif
}
