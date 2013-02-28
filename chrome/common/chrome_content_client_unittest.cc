// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_content_client.h"

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "content/public/common/content_switches.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void CheckUserAgentStringOrdering(bool mobile_device) {
  std::vector<std::string> pieces;

  // Check if the pieces of the user agent string come in the correct order.
  chrome::ChromeContentClient content_client;
  std::string buffer = content_client.GetUserAgent();

  base::SplitStringUsingSubstr(buffer, "Mozilla/5.0 (", &pieces);
  ASSERT_EQ(2u, pieces.size());
  buffer = pieces[1];
  EXPECT_EQ("", pieces[0]);

  base::SplitStringUsingSubstr(buffer, ") AppleWebKit/", &pieces);
  ASSERT_EQ(2u, pieces.size());
  buffer = pieces[1];
  std::string os_str = pieces[0];

  base::SplitStringUsingSubstr(buffer, " (KHTML, like Gecko) ", &pieces);
  ASSERT_EQ(2u, pieces.size());
  buffer = pieces[1];
  std::string webkit_version_str = pieces[0];

  base::SplitStringUsingSubstr(buffer, " Safari/", &pieces);
  ASSERT_EQ(2u, pieces.size());
  std::string product_str = pieces[0];
  std::string safari_version_str = pieces[1];

  // Not sure what can be done to better check the OS string, since it's highly
  // platform-dependent.
  EXPECT_TRUE(os_str.size() > 0);

  // Check that the version numbers match.
  EXPECT_TRUE(webkit_version_str.size() > 0);
  EXPECT_TRUE(safari_version_str.size() > 0);
  EXPECT_EQ(webkit_version_str, safari_version_str);

  EXPECT_EQ(0u, product_str.find("Chrome/"));
  if (mobile_device) {
    // "Mobile" gets tacked on to the end for mobile devices, like phones.
    const std::string kMobileStr = " Mobile";
    EXPECT_EQ(kMobileStr,
              product_str.substr(product_str.size() - kMobileStr.size()));
  }
}

}  // namespace


namespace chrome_common {

TEST(ChromeContentClientTest, Basic) {
#if !defined(OS_ANDROID)
  CheckUserAgentStringOrdering(false);
#else
  const char* kArguments[] = {"chrome"};
  CommandLine::Reset();
  CommandLine::Init(1, kArguments);
  CommandLine* command_line = CommandLine::ForCurrentProcess();

  // Do it for regular devices.
  ASSERT_FALSE(command_line->HasSwitch(switches::kUseMobileUserAgent));
  CheckUserAgentStringOrdering(false);

  // Do it for mobile devices.
  command_line->AppendSwitch(switches::kUseMobileUserAgent);
  ASSERT_TRUE(command_line->HasSwitch(switches::kUseMobileUserAgent));
  CheckUserAgentStringOrdering(true);
#endif
}

}  // namespace chrome_common
