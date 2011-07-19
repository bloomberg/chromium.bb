// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "net/base/mock_host_resolver.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Idle) {
  ASSERT_TRUE(RunExtensionTest("idle")) << message_;
}
