// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/common/origin_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::IsOriginSecure;

namespace chrome {

TEST(SecureOriginWhiteList, UnsafelyTreatInsecureOriginAsSecure) {
  EXPECT_FALSE(content::IsOriginSecure(GURL("http://example.com/a.html")));
  EXPECT_FALSE(
      content::IsOriginSecure(GURL("http://127.example.com/a.html")));

  // Add http://example.com and http://127.example.com to whitelist by
  // command-line and see if they are now considered secure origins.
  // (The command line is applied via
  // ChromeContentClient::AddSecureSchemesAndOrigins)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(
      switches::kUnsafelyTreatInsecureOriginAsSecure,
      "http://example.com,http://127.example.com");
  command_line->AppendSwitch(switches::kUserDataDir);
  content::ResetSchemesAndOriginsWhitelistForTesting();

  // They should be now white-listed.
  EXPECT_TRUE(content::IsOriginSecure(GURL("http://example.com/a.html")));
  EXPECT_TRUE(content::IsOriginSecure(GURL("http://127.example.com/a.html")));

  // Check that similarly named sites are not considered secure.
  EXPECT_FALSE(content::IsOriginSecure(GURL("http://128.example.com/a.html")));
  EXPECT_FALSE(content::IsOriginSecure(
      GURL("http://foobar.127.example.com/a.html")));
}

}  // namespace chrome
