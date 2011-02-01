// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebRequest) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("webrequest/api")) << message_;
}

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, WebRequestEvents) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
    switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("webrequest/events")) << message_;
}
