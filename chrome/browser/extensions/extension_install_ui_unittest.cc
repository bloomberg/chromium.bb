// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_install_ui.h"

#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/url_pattern.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void CompareLists(const std::vector<std::string>& expected,
                  const std::vector<std::string>& actual) {
  ASSERT_EQ(expected.size(), actual.size());

  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(expected[i], actual[i]);
  }
}

}

TEST(ExtensionInstallUITest, GetDistinctHostsForDisplay) {
  std::vector<std::string> expected;
  expected.push_back("www.foo.com");
  expected.push_back("www.bar.com");
  expected.push_back("www.baz.com");

  // Simple list with no dupes.
  URLPatternList actual;
  actual.push_back(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.com/path"));
  actual.push_back(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.bar.com/path"));
  actual.push_back(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.baz.com/path"));
  CompareLists(expected,
               Extension::GetDistinctHosts(actual));

  // Add some dupes.
  actual.push_back(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.foo.com/path"));
  actual.push_back(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.baz.com/path"));
  CompareLists(expected,
               Extension::GetDistinctHosts(actual));


  // Add a pattern that differs only by scheme. This should be filtered out.
  actual.push_back(
      URLPattern(URLPattern::SCHEME_HTTPS, "https://www.bar.com/path"));
  CompareLists(expected,
               Extension::GetDistinctHosts(actual));

  // Add some dupes by path.
  actual.push_back(
      URLPattern(URLPattern::SCHEME_HTTP, "http://www.bar.com/pathypath"));
  CompareLists(expected,
               Extension::GetDistinctHosts(actual));

  // We don't do anything special for subdomains.
  actual.push_back(
      URLPattern(URLPattern::SCHEME_HTTP, "http://monkey.www.bar.com/path"));
  actual.push_back(
      URLPattern(URLPattern::SCHEME_HTTP, "http://bar.com/path"));

  expected.push_back("monkey.www.bar.com");
  expected.push_back("bar.com");

  CompareLists(expected,
               Extension::GetDistinctHosts(actual));
}
