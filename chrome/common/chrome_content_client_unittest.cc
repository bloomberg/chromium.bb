// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/chrome_content_client.h"

#include <string.h>

#include <memory>

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "content/public/common/content_switches.h"
#include "extensions/common/constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace {

void CheckUserAgentStringOrdering(bool mobile_device) {
  std::vector<std::string> pieces;

  // Check if the pieces of the user agent string come in the correct order.
  ChromeContentClient content_client;
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
  const char* const kArguments[] = {"chrome"};
  base::CommandLine::Reset();
  base::CommandLine::Init(1, kArguments);
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();

  // Do it for regular devices.
  ASSERT_FALSE(command_line->HasSwitch(switches::kUseMobileUserAgent));
  CheckUserAgentStringOrdering(false);

  // Do it for mobile devices.
  command_line->AppendSwitch(switches::kUseMobileUserAgent);
  ASSERT_TRUE(command_line->HasSwitch(switches::kUseMobileUserAgent));
  CheckUserAgentStringOrdering(true);
#endif
}

#if defined(ENABLE_PLUGINS)
TEST(ChromeContentClientTest, FindMostRecent) {
  std::vector<content::PepperPluginInfo*> version_vector;
  // Test an empty vector.
  EXPECT_EQ(nullptr, ChromeContentClient::FindMostRecentPlugin(version_vector));

  // Now test the vector with one element.
  content::PepperPluginInfo info1;
  info1.version = "1.0.0.0";
  version_vector.push_back(&info1);

  content::PepperPluginInfo* most_recent =
      ChromeContentClient::FindMostRecentPlugin(version_vector);
  EXPECT_EQ("1.0.0.0", most_recent->version);

  content::PepperPluginInfo info5;
  info5.version = "5.0.12.1";
  content::PepperPluginInfo info6_12;
  info6_12.version = "6.0.0.12";
  content::PepperPluginInfo info6_13;
  info6_13.version = "6.0.0.13";

  // Test highest version is picked.
  version_vector.clear();
  version_vector.push_back(&info5);
  version_vector.push_back(&info6_12);
  version_vector.push_back(&info6_13);

  most_recent = ChromeContentClient::FindMostRecentPlugin(version_vector);
  EXPECT_EQ("6.0.0.13", most_recent->version);

  // Test that order does not matter, validates tests below.
  version_vector.clear();
  version_vector.push_back(&info6_13);
  version_vector.push_back(&info6_12);
  version_vector.push_back(&info5);

  most_recent = ChromeContentClient::FindMostRecentPlugin(version_vector);
  EXPECT_EQ("6.0.0.13", most_recent->version);

  // Test real scenarios.
  content::PepperPluginInfo bundled_flash;
  bundled_flash.version = "4.3.2.1";
  bundled_flash.is_external = false;
  bundled_flash.is_debug = false;
  bundled_flash.is_on_local_drive = true;
  bundled_flash.is_bundled = true;
  bundled_flash.name = "bundled_flash";

  content::PepperPluginInfo local_component_flash;
  local_component_flash.version = "4.3.2.1";
  local_component_flash.is_external = false;
  local_component_flash.is_debug = false;
  local_component_flash.is_on_local_drive = true;
  local_component_flash.is_bundled = false;
  local_component_flash.name = "local_component_flash";

  content::PepperPluginInfo network_component_flash;
  network_component_flash.version = "4.3.2.1";
  network_component_flash.is_external = false;
  network_component_flash.is_debug = false;
  network_component_flash.is_on_local_drive = false;
  network_component_flash.is_bundled = false;
  network_component_flash.name = "network_component_flash";

  content::PepperPluginInfo system_flash;
  system_flash.version = "4.3.2.1";
  system_flash.is_external = true;
  system_flash.is_debug = false;
  system_flash.is_on_local_drive = true;
  system_flash.is_bundled = false;
  system_flash.name = "system_flash";

  content::PepperPluginInfo system_debug_flash;
  system_debug_flash.version = "4.3.2.1";
  system_debug_flash.is_external = true;
  system_debug_flash.is_debug = true;
  system_debug_flash.is_on_local_drive = false;
  system_debug_flash.is_bundled = false;
  system_debug_flash.name = "system_debug_flash";

  // The order here should be:
  // 1. Debug System Flash.
  // 2. Bundled.
  // 3. Component update on a local drive.
  // 4. System Flash.
  // 5. Component update on a network drive.

  // Debug beats bundled.
  version_vector.clear();
  version_vector.push_back(&system_debug_flash);
  version_vector.push_back(&bundled_flash);

  most_recent = ChromeContentClient::FindMostRecentPlugin(version_vector);
  EXPECT_STREQ("system_debug_flash", most_recent->name.c_str());

  // Bundled beats component updated.
  version_vector.clear();
  version_vector.push_back(&bundled_flash);
  version_vector.push_back(&local_component_flash);

  most_recent = ChromeContentClient::FindMostRecentPlugin(version_vector);
  EXPECT_STREQ("bundled_flash", most_recent->name.c_str());

  // Bundled beats System flash
  version_vector.clear();
  version_vector.push_back(&bundled_flash);
  version_vector.push_back(&system_flash);

  most_recent = ChromeContentClient::FindMostRecentPlugin(version_vector);
  EXPECT_STREQ("bundled_flash", most_recent->name.c_str());

  // Local component updated beats System Flash.
  version_vector.clear();
  version_vector.push_back(&system_flash);
  version_vector.push_back(&local_component_flash);

  most_recent = ChromeContentClient::FindMostRecentPlugin(version_vector);
  EXPECT_STREQ("local_component_flash", most_recent->name.c_str());

  // System Flash beats component update on network drive.
  version_vector.clear();
  version_vector.push_back(&network_component_flash);
  version_vector.push_back(&system_flash);

  most_recent = ChromeContentClient::FindMostRecentPlugin(version_vector);
  EXPECT_STREQ("system_flash", most_recent->name.c_str());
}
#endif  // defined(ENABLE_PLUGINS)

TEST(ChromeContentClientTest, AdditionalSchemes) {
  EXPECT_TRUE(url::IsStandard(
      extensions::kExtensionScheme,
      url::Component(0, strlen(extensions::kExtensionScheme))));

  GURL extension_url(
      "chrome-extension://abcdefghijklmnopqrstuvwxyzabcdef/foo.html");
  url::Origin origin(extension_url);
  EXPECT_EQ("chrome-extension://abcdefghijklmnopqrstuvwxyzabcdef",
            origin.Serialize());
}

}  // namespace chrome_common
