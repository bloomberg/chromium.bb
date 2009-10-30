// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"

IN_PROC_BROWSER_TEST_F(ExtensionApiTest, History) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  host_resolver()->AddRule("www.a.com", "127.0.0.1");
  host_resolver()->AddRule("www.b.com", "127.0.0.1");
  StartHTTPServer();

  ASSERT_TRUE(RunExtensionTest("history")) << message_;
}
