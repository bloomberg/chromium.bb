// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"

class ExtensionWebSocketProxyPrivateApiTest : public ExtensionApiTest {
  void SetUpCommandLine(CommandLine* command_line) {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kAllowWebSocketProxy, "mknjjldhaihcdajjbihghhiehamnpcak");
  }
};

IN_PROC_BROWSER_TEST_F(ExtensionWebSocketProxyPrivateApiTest, Pass) {
  ASSERT_TRUE(RunExtensionTest("web_socket_proxy_private")) << message_;
}

