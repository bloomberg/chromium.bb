// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#if defined(TOOLKIT_VIEWS)
#define MAYBE_Infobars Infobars
#else
// Need to finish port to Linux. See http://crbug.com/39916 for details.
// Also disabled on mac.  See http://crbug.com/60990.
#define MAYBE_Infobars DISABLED_Infobars
#endif

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_Infobars) {
  ASSERT_TRUE(RunExtensionTest("infobars")) << message_;
}
