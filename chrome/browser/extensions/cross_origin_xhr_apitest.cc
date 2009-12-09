// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

#if defined(OS_MACOSX)
// http://crbug.com/29711
#define MAYBE_CrossOriginXHR DISABLED_CrossOriginXHR
#else
#define MAYBE_CrossOriginXHR CrossOriginXHR
#endif

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_CrossOriginXHR) {
  host_resolver()->AddRule("*.com", "127.0.0.1");
  StartHTTPServer();
  ASSERT_TRUE(RunExtensionTest("cross_origin_xhr")) << message_;
}
