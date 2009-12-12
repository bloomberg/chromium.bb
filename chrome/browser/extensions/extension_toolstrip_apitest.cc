// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#if defined(OS_LINUX) && defined(TOOLKIT_VIEWS)
// See http://crbug.com/30151.
#define Toolstrip DISABLED_Toolstrip
#endif

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Toolstrip) {
  ASSERT_TRUE(RunExtensionTest("toolstrip")) << message_;
}
