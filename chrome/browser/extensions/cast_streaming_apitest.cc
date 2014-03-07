// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/common/content_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

class CastStreamingApiTest : public ExtensionApiTest {
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        switches::kWhitelistedExtensionID,
        "ddchlicdkolnonkihahngkmmmjnjlkkf");
  }
};

// Test running the test extension for Cast Mirroring API.
IN_PROC_BROWSER_TEST_F(CastStreamingApiTest, Basics) {
  ASSERT_TRUE(RunExtensionSubtest("cast_streaming", "basics.html"));
}

IN_PROC_BROWSER_TEST_F(CastStreamingApiTest, Stats) {
  ASSERT_TRUE(RunExtensionSubtest("cast_streaming", "stats.html"));
}

IN_PROC_BROWSER_TEST_F(CastStreamingApiTest, BadLogging) {
  ASSERT_TRUE(RunExtensionSubtest("cast_streaming", "bad_logging.html"));
}

}  // namespace extensions
