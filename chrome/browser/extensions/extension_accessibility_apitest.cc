// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

// This extension is currently only supported on Linux and Chrome OS.
#if defined(OS_LINUX)
#define MAYBE_Accessibility Accessibility
#else
#define MAYBE_Accessibility DISABLED_Accessibility
#endif

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_Accessibility) {
  StartHTTPServer();
  ASSERT_TRUE(RunExtensionTest("accessibility")) << message_;
}
