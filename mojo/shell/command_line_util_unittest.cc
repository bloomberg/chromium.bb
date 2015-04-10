// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/shell/command_line_util.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace shell {
namespace {

TEST(CommandLineUtil, ParseArgsFor) {
  static const struct Expectation {
    const char* args;
    const char* value;
  } EXPECTATIONS[] = {
      {"", nullptr},
      {"hello", nullptr},
      {"args-for=mojo:app1", nullptr},
      {"--args-for", nullptr},
      {"--args-for=", ""},
      {"--args-for=mojo:app1", "mojo:app1"},
      {"--args-for=mojo:app1 hello world", "mojo:app1 hello world"},
      {"-args-for", nullptr},
      {"-args-for=", ""},
      {"-args-for=mojo:app1", "mojo:app1"},
      {"-args-for=mojo:app1 hello world", "mojo:app1 hello world"}};
  for (auto& expectation : EXPECTATIONS) {
    std::string value;
    bool result = ParseArgsFor(expectation.args, &value);
    EXPECT_EQ(bool(expectation.value), result);
    if (expectation.value && result)
      EXPECT_EQ(value, expectation.value);
  }
}

TEST(CommandLineUtil, GetAppURLAndArgs) {
  const char* NO_ARGUMENTS[] = {nullptr};
  const char* ONE_ARGUMENTS[] = {"1", nullptr};
  const char* TWO_ARGUMENTS[] = {"1", "two", nullptr};
  static const struct Expectation {
    const char* args;
    const char* url;
    const char** values;
  } EXPECTATIONS[] = {
      {"", nullptr, nullptr},
      {"foo", "file:///root/foo", NO_ARGUMENTS},
      {"/foo", "file:///foo", NO_ARGUMENTS},
      {"file:foo", "file:///root/foo", NO_ARGUMENTS},
      {"file:///foo", "file:///foo", NO_ARGUMENTS},
      {"http://example.com", "http://example.com", NO_ARGUMENTS},
      {"http://example.com 1", "http://example.com", ONE_ARGUMENTS},
      {"http://example.com 1 ", "http://example.com", ONE_ARGUMENTS},
      {"http://example.com  1 ", "http://example.com", ONE_ARGUMENTS},
      {"http://example.com 1 two", "http://example.com", TWO_ARGUMENTS},
      {"   http://example.com  1  two   ",
       "http://example.com",
       TWO_ARGUMENTS}};
  Context context;
  context.SetCommandLineCWD(base::FilePath(FILE_PATH_LITERAL("/root")));
  for (auto& expectation : EXPECTATIONS) {
    std::vector<std::string> args;
    GURL result(GetAppURLAndArgs(&context, expectation.args, &args));
    EXPECT_EQ(bool(expectation.url), result.is_valid());
    if (expectation.url && result.is_valid()) {
      EXPECT_EQ(GURL(expectation.url), result);
      std::vector<std::string> expected_args;
      if (expectation.values) {
        if (*expectation.values)
          expected_args.push_back(expectation.url);
        for (const char** value = expectation.values; *value; ++value)
          expected_args.push_back(*value);
      }
      EXPECT_EQ(expected_args, args);
    }
  }
}

}  // namespace
}  // namespace shell
}  // namespace mojo
