// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autocomplete/autocomplete_input.h"

#include "base/basictypes.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "build/build_config.h"
#include "googleurl/src/url_parse.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(AutocompleteInputTest, InputType) {
  struct test_data {
    const string16 input;
    const AutocompleteInput::Type type;
  } input_cases[] = {
    { string16(), AutocompleteInput::INVALID },
    { ASCIIToUTF16("?"), AutocompleteInput::FORCED_QUERY },
    { ASCIIToUTF16("?foo"), AutocompleteInput::FORCED_QUERY },
    { ASCIIToUTF16("?foo bar"), AutocompleteInput::FORCED_QUERY },
    { ASCIIToUTF16("?http://foo.com/bar"), AutocompleteInput::FORCED_QUERY },
    { ASCIIToUTF16("foo"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("localhost"), AutocompleteInput::URL },
    { ASCIIToUTF16("foo.c"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("foo.com"), AutocompleteInput::URL },
    { ASCIIToUTF16("-foo.com"), AutocompleteInput::URL },
    { ASCIIToUTF16("foo-.com"), AutocompleteInput::URL },
    { ASCIIToUTF16("foo_.com"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("foo.-com"), AutocompleteInput::QUERY },
    { ASCIIToUTF16("foo/"), AutocompleteInput::URL },
    { ASCIIToUTF16("foo/bar"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("foo/bar/"), AutocompleteInput::URL },
    { ASCIIToUTF16("foo/bar baz\\"), AutocompleteInput::URL },
    { ASCIIToUTF16("foo.com/bar"), AutocompleteInput::URL },
    { ASCIIToUTF16("foo;bar"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("foo/bar baz"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("foo bar.com"), AutocompleteInput::QUERY },
    { ASCIIToUTF16("foo bar"), AutocompleteInput::QUERY },
    { ASCIIToUTF16("foo+bar"), AutocompleteInput::QUERY },
    { ASCIIToUTF16("foo+bar.com"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("\"foo:bar\""), AutocompleteInput::QUERY },
    { ASCIIToUTF16("link:foo.com"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("foo:81"), AutocompleteInput::URL },
    { ASCIIToUTF16("localhost:8080"), AutocompleteInput::URL },
    { ASCIIToUTF16("www.foo.com:81"), AutocompleteInput::URL },
    { ASCIIToUTF16("foo.com:123456"), AutocompleteInput::QUERY },
    { ASCIIToUTF16("foo.com:abc"), AutocompleteInput::QUERY },
    { ASCIIToUTF16("1.2.3.4:abc"), AutocompleteInput::QUERY },
    { ASCIIToUTF16("user@foo.com"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("user@foo/z"), AutocompleteInput::URL },
    { ASCIIToUTF16("user@foo/z z"), AutocompleteInput::URL },
    { ASCIIToUTF16("user@foo.com/z"), AutocompleteInput::URL },
    { ASCIIToUTF16("user:pass@"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("user:pass@!foo.com"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("user:pass@foo"), AutocompleteInput::URL },
    { ASCIIToUTF16("user:pass@foo.c"), AutocompleteInput::URL },
    { ASCIIToUTF16("user:pass@foo.com"), AutocompleteInput::URL },
    { ASCIIToUTF16("user:pass@foo.com:81"), AutocompleteInput::URL },
    { ASCIIToUTF16("user:pass@foo:81"), AutocompleteInput::URL },
    { ASCIIToUTF16("1.2"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("1.2/45"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("1.2:45"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("user@1.2:45"), AutocompleteInput::URL },
    { ASCIIToUTF16("user@foo:45"), AutocompleteInput::URL },
    { ASCIIToUTF16("user:pass@1.2:45"), AutocompleteInput::URL },
    { ASCIIToUTF16("host?query"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("host#ref"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("host/path?query"), AutocompleteInput::URL },
    { ASCIIToUTF16("host/path#ref"), AutocompleteInput::URL },
    { ASCIIToUTF16("en.wikipedia.org/wiki/Jim Beam"), AutocompleteInput::URL },
    // In Chrome itself, mailto: will get handled by ShellExecute, but in
    // unittest mode, we don't have the data loaded in the external protocol
    // handler to know this.
    // { ASCIIToUTF16("mailto:abuse@foo.com"), AutocompleteInput::URL },
    { ASCIIToUTF16("view-source:http://www.foo.com/"), AutocompleteInput::URL },
    { ASCIIToUTF16("javascript:alert(\"Hi there\");"), AutocompleteInput::URL },
#if defined(OS_WIN)
    { ASCIIToUTF16("C:\\Program Files"), AutocompleteInput::URL },
    { ASCIIToUTF16("\\\\Server\\Folder\\File"), AutocompleteInput::URL },
#endif  // defined(OS_WIN)
    { ASCIIToUTF16("http:foo"), AutocompleteInput::URL },
    { ASCIIToUTF16("http://foo"), AutocompleteInput::URL },
    { ASCIIToUTF16("http://foo.c"), AutocompleteInput::URL },
    { ASCIIToUTF16("http://foo.com"), AutocompleteInput::URL },
    { ASCIIToUTF16("http://foo_bar.com"), AutocompleteInput::URL },
    { ASCIIToUTF16("http://foo/bar baz"), AutocompleteInput::URL },
    { ASCIIToUTF16("http://-foo.com"), AutocompleteInput::URL },
    { ASCIIToUTF16("http://foo-.com"), AutocompleteInput::URL },
    { ASCIIToUTF16("http://foo_.com"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("http://foo.-com"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("http://_foo_.com"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("http://foo.com:abc"), AutocompleteInput::QUERY },
    { ASCIIToUTF16("http://foo.com:123456"), AutocompleteInput::QUERY },
    { ASCIIToUTF16("http://1.2.3.4:abc"), AutocompleteInput::QUERY },
    { ASCIIToUTF16("http:user@foo.com"), AutocompleteInput::URL },
    { ASCIIToUTF16("http://user@foo.com"), AutocompleteInput::URL },
    { ASCIIToUTF16("http:user:pass@foo.com"), AutocompleteInput::URL },
    { ASCIIToUTF16("http://user:pass@foo.com"), AutocompleteInput::URL },
    { ASCIIToUTF16("http://1.2"), AutocompleteInput::URL },
    { ASCIIToUTF16("http://1.2/45"), AutocompleteInput::URL },
    { ASCIIToUTF16("http:ps/2 games"), AutocompleteInput::URL },
    { ASCIIToUTF16("https://foo.com"), AutocompleteInput::URL },
    { ASCIIToUTF16("127.0.0.1"), AutocompleteInput::URL },
    { ASCIIToUTF16("127.0.1"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("127.0.1/"), AutocompleteInput::URL },
    { ASCIIToUTF16("browser.tabs.closeButtons"), AutocompleteInput::UNKNOWN },
    { WideToUTF16(L"\u6d4b\u8bd5"), AutocompleteInput::UNKNOWN },
    { ASCIIToUTF16("[2001:]"), AutocompleteInput::QUERY },
    { ASCIIToUTF16("[2001:dB8::1]"), AutocompleteInput::URL },
    { ASCIIToUTF16("192.168.0.256"), AutocompleteInput::QUERY },
    { ASCIIToUTF16("[foo.com]"), AutocompleteInput::QUERY },
    { ASCIIToUTF16("filesystem:http://a.com/t/bar"), AutocompleteInput::URL },
    { ASCIIToUTF16("filesystem:http:foo"), AutocompleteInput::URL },
    { ASCIIToUTF16("filesystem:file://"), AutocompleteInput::URL },
    { ASCIIToUTF16("filesystem:http"), AutocompleteInput::URL },
    { ASCIIToUTF16("filesystem:"), AutocompleteInput::URL },
    { ASCIIToUTF16("ftp:"), AutocompleteInput::URL },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(input_cases); ++i) {
    SCOPED_TRACE(input_cases[i].input);
    AutocompleteInput input(input_cases[i].input, string16::npos, string16(),
                            GURL(), true, false, true,
                            AutocompleteInput::ALL_MATCHES);
    EXPECT_EQ(input_cases[i].type, input.type());
  }
}

TEST(AutocompleteInputTest, InputTypeWithDesiredTLD) {
  struct test_data {
    const string16 input;
    const AutocompleteInput::Type type;
    const std::string spec;  // Unused if not a URL.
  } input_cases[] = {
    { ASCIIToUTF16("401k"), AutocompleteInput::URL,
        std::string("http://www.401k.com/") },
    { ASCIIToUTF16("999999999999999"), AutocompleteInput::URL,
        std::string("http://www.999999999999999.com/") },
    { ASCIIToUTF16("x@y"), AutocompleteInput::URL,
        std::string("http://x@www.y.com/") },
    { ASCIIToUTF16("y/z z"), AutocompleteInput::URL,
        std::string("http://www.y.com/z%20z") },
    { ASCIIToUTF16("abc.com"), AutocompleteInput::URL,
        std::string("http://abc.com/") },
    { ASCIIToUTF16("foo bar"), AutocompleteInput::QUERY, std::string() },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(input_cases); ++i) {
    SCOPED_TRACE(input_cases[i].input);
    AutocompleteInput input(input_cases[i].input, string16::npos,
                            ASCIIToUTF16("com"), GURL(), true, false, true,
                            AutocompleteInput::ALL_MATCHES);
    EXPECT_EQ(input_cases[i].type, input.type());
    if (input_cases[i].type == AutocompleteInput::URL)
      EXPECT_EQ(input_cases[i].spec, input.canonicalized_url().spec());
  }
}

// This tests for a regression where certain input in the omnibox caused us to
// crash. As long as the test completes without crashing, we're fine.
TEST(AutocompleteInputTest, InputCrash) {
  AutocompleteInput input(WideToUTF16(L"\uff65@s"), string16::npos, string16(),
                          GURL(), true, false, true,
                          AutocompleteInput::ALL_MATCHES);
}

TEST(AutocompleteInputTest, ParseForEmphasizeComponent) {
  using url_parse::Component;
  Component kInvalidComponent(0, -1);
  struct test_data {
    const string16 input;
    const Component scheme;
    const Component host;
  } input_cases[] = {
    { string16(), kInvalidComponent, kInvalidComponent },
    { ASCIIToUTF16("?"), kInvalidComponent, kInvalidComponent },
    { ASCIIToUTF16("?http://foo.com/bar"), kInvalidComponent,
        kInvalidComponent },
    { ASCIIToUTF16("foo/bar baz"), kInvalidComponent, Component(0, 3) },
    { ASCIIToUTF16("http://foo/bar baz"), Component(0, 4), Component(7, 3) },
    { ASCIIToUTF16("link:foo.com"), Component(0, 4), kInvalidComponent },
    { ASCIIToUTF16("www.foo.com:81"), kInvalidComponent, Component(0, 11) },
    { WideToUTF16(L"\u6d4b\u8bd5"), kInvalidComponent, Component(0, 2) },
    { ASCIIToUTF16("view-source:http://www.foo.com/"), Component(12, 4),
        Component(19, 11) },
    { ASCIIToUTF16("view-source:https://example.com/"),
      Component(12, 5), Component(20, 11) },
    { ASCIIToUTF16("view-source:www.foo.com"), kInvalidComponent,
        Component(12, 11) },
    { ASCIIToUTF16("view-source:"), Component(0, 11), kInvalidComponent },
    { ASCIIToUTF16("view-source:garbage"), kInvalidComponent,
        Component(12, 7) },
    { ASCIIToUTF16("view-source:http://http://foo"), Component(12, 4),
        Component(19, 4) },
    { ASCIIToUTF16("view-source:view-source:http://example.com/"),
        Component(12, 11), kInvalidComponent }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(input_cases); ++i) {
    SCOPED_TRACE(input_cases[i].input);
    Component scheme, host;
    AutocompleteInput::ParseForEmphasizeComponents(input_cases[i].input,
                                                   &scheme,
                                                   &host);
    AutocompleteInput input(input_cases[i].input, string16::npos, string16(),
                            GURL(), true, false, true,
                            AutocompleteInput::ALL_MATCHES);
    EXPECT_EQ(input_cases[i].scheme.begin, scheme.begin);
    EXPECT_EQ(input_cases[i].scheme.len, scheme.len);
    EXPECT_EQ(input_cases[i].host.begin, host.begin);
    EXPECT_EQ(input_cases[i].host.len, host.len);
  }
}

TEST(AutocompleteInputTest, InputTypeWithCursorPosition) {
  struct test_data {
    const string16 input;
    size_t cursor_position;
    const string16 normalized_input;
    size_t normalized_cursor_position;
  } input_cases[] = {
    { ASCIIToUTF16("foo bar"), string16::npos,
      ASCIIToUTF16("foo bar"), string16::npos },

    // regular case, no changes.
    { ASCIIToUTF16("foo bar"), 3, ASCIIToUTF16("foo bar"), 3 },

    // extra leading space.
    { ASCIIToUTF16("  foo bar"), 3, ASCIIToUTF16("foo bar"), 1 },
    { ASCIIToUTF16("      foo bar"), 3, ASCIIToUTF16("foo bar"), 0 },
    { ASCIIToUTF16("      foo bar   "), 2, ASCIIToUTF16("foo bar   "), 0 },

    // forced query.
    { ASCIIToUTF16("?foo bar"), 2, ASCIIToUTF16("foo bar"), 1 },
    { ASCIIToUTF16("  ?foo bar"), 4, ASCIIToUTF16("foo bar"), 1 },
    { ASCIIToUTF16("?  foo bar"), 4, ASCIIToUTF16("foo bar"), 1 },
    { ASCIIToUTF16("  ?  foo bar"), 6, ASCIIToUTF16("foo bar"), 1 },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(input_cases); ++i) {
    SCOPED_TRACE(input_cases[i].input);
    AutocompleteInput input(input_cases[i].input,
                            input_cases[i].cursor_position,
                            string16(), GURL(), true, false, true,
                            AutocompleteInput::ALL_MATCHES);
    EXPECT_EQ(input_cases[i].normalized_input, input.text());
    EXPECT_EQ(input_cases[i].normalized_cursor_position,
              input.cursor_position());
  }
}
