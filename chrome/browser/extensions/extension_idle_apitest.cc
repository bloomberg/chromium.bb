// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "net/base/mock_host_resolver.h"

// Sometimes this test fails on Linux: crbug.com/130138
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_Idle) {
#else
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Idle) {
#endif
  ASSERT_TRUE(RunExtensionTest("idle")) << message_;
}
