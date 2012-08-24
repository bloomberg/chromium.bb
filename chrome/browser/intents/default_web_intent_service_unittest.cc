// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/utf_string_conversions.h"
#include "chrome/browser/intents/default_web_intent_service.h"
#include "chrome/common/extensions/url_pattern.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kEmptyString[] = "";
const char kShareAction[] = "http://webintents.org/share";
const char kPngType[] = "image/png";
const char kMailToScheme[] = "mailto";
const char kQuackService[] = "http://ducknet.com/quack";
const URLPattern all_pattern(
    URLPattern::SCHEME_ALL, URLPattern::kAllUrlsPattern);

TEST(DefaultWebIntentServiceTest, Defaults) {
  DefaultWebIntentService service;

  EXPECT_EQ(string16(), service.action);
  EXPECT_EQ(string16(), service.type);
  EXPECT_EQ(string16(), service.scheme);
  EXPECT_EQ(all_pattern, service.url_pattern);
  EXPECT_EQ(-1, service.user_date);
  EXPECT_EQ(0, service.suppression);
  EXPECT_EQ("", service.service_url);
}

TEST(DefaultWebIntentServiceTest, ActionServicesEqual) {
  DefaultWebIntentService actual(
      ASCIIToUTF16(kShareAction),
      ASCIIToUTF16(kPngType),
      kQuackService);

  DefaultWebIntentService expected;
  expected.action = ASCIIToUTF16(kShareAction);
  expected.type = ASCIIToUTF16(kPngType);
  expected.service_url = kQuackService;

  EXPECT_EQ(expected, actual);
}

TEST(DefaultWebIntentServiceTest, SchemeServicesEqual) {
  DefaultWebIntentService actual(
      ASCIIToUTF16(kMailToScheme),
      kQuackService);

  DefaultWebIntentService expected;
  expected.scheme = ASCIIToUTF16(kMailToScheme);
  expected.service_url = kQuackService;

  EXPECT_EQ(expected, actual);
}

} // namespace
