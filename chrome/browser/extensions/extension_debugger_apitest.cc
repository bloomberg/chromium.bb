// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"

// Temporary disabled for landing DevTools protocol patch upstream.
// https://bugs.webkit.org/show_bug.cgi?id=57957
// We are migrating the protocol to JSON-RPC-2.0 spec.
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, DISABLED_Debugger) {
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableExperimentalExtensionApis);

  ASSERT_TRUE(RunExtensionTest("debugger")) << message_;
}
