// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/common/extensions/manifest_tests/chrome_manifest_test.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/launcher_page_info.h"
#include "extensions/common/switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

// The ID of test manifests requiring whitelisting.
const char kWhitelistID[] = "lmadimbbgapmngbiclpjjngmdickadpl";

}  // namespace

typedef ChromeManifestTest LauncherPageManifestTest;

TEST_F(LauncherPageManifestTest, ValidLauncherPage) {
  CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      extensions::switches::kWhitelistedExtensionID, kWhitelistID);
  scoped_refptr<extensions::Extension> extension(
      LoadAndExpectSuccess("init_valid_launcher_page.json"));
  ASSERT_TRUE(extension.get());
  extensions::LauncherPageInfo* info =
      extensions::LauncherPageHandler::GetInfo(extension.get());
  ASSERT_TRUE(info);
  EXPECT_EQ("test.html", info->page);
}

}  // namespace extensions
