// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"

namespace extensions {

#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, IdleGetAutoLockDelay) {
  ASSERT_TRUE(RunExtensionTest("idle/get_auto_lock_delay")) << message_;
}
#else
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, UnsupportedIdleGetAutoLockDelay) {
  ASSERT_TRUE(RunExtensionTest("idle/unsupported_get_auto_lock_delay"));
}
#endif
}  // namespace extensions
