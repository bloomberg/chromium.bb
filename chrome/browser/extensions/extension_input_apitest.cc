// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/ui_test_utils.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, Input) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("input")) << message_;
}
