// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

// This test failed at times on the Vista dbg builder and has been marked as
// flaky for now. Bug http://code.google.com/p/chromium/issues/detail?id=28630
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, FLAKY_ExecuteScript) {
  host_resolver()->AddRule("a.com", "127.0.0.1");
  host_resolver()->AddRule("b.com", "127.0.0.1");
  StartHTTPServer();

  ASSERT_TRUE(RunExtensionTest("executescript")) << message_;
  ASSERT_TRUE(RunExtensionTest("executescript_in_frame")) << message_;
}
