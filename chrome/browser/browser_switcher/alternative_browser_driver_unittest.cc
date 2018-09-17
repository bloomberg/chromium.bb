// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browser_switcher/alternative_browser_driver.h"

#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#include <algorithm>

namespace browser_switcher {

using StringType = base::FilePath::StringType;

namespace {

StringType UTF8ToNative(base::StringPiece src) {
#if defined(OS_WIN)
  return base::UTF8ToWide(src);
#elif defined(OS_LINUX)
  return src.as_string();
#endif
}

std::vector<StringType> UTF8VectorToNative(
    const std::vector<base::StringPiece>& src) {
  std::vector<StringType> out;
  out.reserve(src.size());
  std::transform(src.begin(), src.end(), std::back_inserter(out),
                 &UTF8ToNative);
  return out;
}

}  // namespace

class AlternativeBrowserDriverTest : public testing::Test {};

TEST_F(AlternativeBrowserDriverTest, AppendCommandLineArguments) {
  std::vector<StringType> argv = {UTF8ToNative("wget")};
  AlternativeBrowserDriverImpl::AppendCommandLineArguments(
      &argv, UTF8VectorToNative({"-O", "-", "--"}),
      GURL("https://example.com/"));
  ASSERT_EQ(5u, argv.size());
  EXPECT_EQ(UTF8ToNative("wget"), argv[0]);
  EXPECT_EQ(UTF8ToNative("-O"), argv[1]);
  EXPECT_EQ(UTF8ToNative("-"), argv[2]);
  EXPECT_EQ(UTF8ToNative("--"), argv[3]);
  EXPECT_EQ(UTF8ToNative("https://example.com/"), argv[4]);
}

TEST_F(AlternativeBrowserDriverTest, AppendCommandLineArgumentsSubstitutesUrl) {
  std::vector<StringType> argv = {UTF8ToNative("google-chrome")};
  AlternativeBrowserDriverImpl::AppendCommandLineArguments(
      &argv, UTF8VectorToNative({"--app=${url}#fragment"}),
      GURL("https://example.com/"));
  ASSERT_EQ(2u, argv.size());
  EXPECT_EQ(UTF8ToNative("google-chrome"), argv[0]);
  EXPECT_EQ(UTF8ToNative("--app=https://example.com/#fragment"), argv[1]);
}

}  // namespace browser_switcher
