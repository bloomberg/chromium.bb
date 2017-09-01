// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/chrome_http_user_agent_settings.h"

#include "testing/gtest/include/gtest/gtest.h"

// Test the expansion of the Language List.
TEST(ChromeHttpUserAgentSettings, ExpandLanguageList) {
  std::string output = ChromeHttpUserAgentSettings::ExpandLanguageList("");
  EXPECT_EQ("", output);

  output = ChromeHttpUserAgentSettings::ExpandLanguageList("en-US");
  EXPECT_EQ("en-US,en", output);

  output = ChromeHttpUserAgentSettings::ExpandLanguageList("fr");
  EXPECT_EQ("fr", output);

  // The base language is added after all regional codes...
  output = ChromeHttpUserAgentSettings::ExpandLanguageList("en-US,en-CA");
  EXPECT_EQ("en-US,en-CA,en", output);

  // ... but before other language families.
  output = ChromeHttpUserAgentSettings::ExpandLanguageList("en-US,en-CA,fr");
  EXPECT_EQ("en-US,en-CA,en,fr", output);

  output =
      ChromeHttpUserAgentSettings::ExpandLanguageList("en-US,en-CA,fr,en-AU");
  EXPECT_EQ("en-US,en-CA,en,fr,en-AU", output);

  output = ChromeHttpUserAgentSettings::ExpandLanguageList("en-US,en-CA,fr-CA");
  EXPECT_EQ("en-US,en-CA,en,fr-CA,fr", output);

  // Add a base language even if it's already in the list.
  output = ChromeHttpUserAgentSettings::ExpandLanguageList(
      "en-US,fr-CA,it,fr,es-AR,it-IT");
  EXPECT_EQ("en-US,en,fr-CA,fr,it,es-AR,es,it-IT", output);
}
