// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/notification_types.h"
#include "extensions/shell/test/shell_apitest.h"

namespace extensions {

// Test that we can open an app window and wait for it to load.
IN_PROC_BROWSER_TEST_F(ShellApiTest, Basic) {
  ASSERT_TRUE(RunAppTest("platform_app")) << message_;
}

}  // namespace extensions
