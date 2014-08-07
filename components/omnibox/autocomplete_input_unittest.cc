// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/autocomplete_input.h"

#include "base/basictypes.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/metrics/proto/omnibox_event.pb.h"
#include "components/metrics/proto/omnibox_input_type.pb.h"
#include "components/omnibox/test_scheme_classifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_parse.h"
#include "url/url_util.h"

using base::ASCIIToUTF16;
using metrics::OmniboxEventProto;

TEST(AutocompleteInputTest, InputType) {
  // TODO(mukai): Fix this scheme setup.
  url::AddStandardScheme("chrome");
  url::AddStandardScheme("chrome-devtools");
  url::AddStandardScheme("chrome-search");

  struct test_data {
    const base::string16 input;
    const metrics::OmniboxInputType::Type type;
  } input_cases[] = {
    { base::string16(), metrics::OmniboxInputType::INVALID },
    { ASCIIToUTF16("?"), metrics::OmniboxInputType::FORCED_QUERY },
    { ASCIIToUTF16("?foo"), metrics::OmniboxInputType::FORCED_QUERY },
    { ASCIIToUTF16("?foo bar"), metrics::OmniboxInputType::FORCED_QUERY },
    { ASCIIToUTF16("?http://foo.com/bar"),
      metrics::OmniboxInputType::FORCED_QUERY },
    { ASCIIToUTF16("foo"), metrics::OmniboxInputType::UNKNOWN },
    { ASCIIToUTF16("localhost"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("foo.c"), metrics::OmniboxInputType::UNKNOWN },
    { ASCIIToUTF16("foo.com"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("-foo.com"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("foo-.com"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("foo_.com"), metrics::OmniboxInputType::UNKNOWN },
    { ASCIIToUTF16("foo.-com"), metrics::OmniboxInputType::QUERY },
    { ASCIIToUTF16("foo/"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("foo/bar"), metrics::OmniboxInputType::UNKNOWN },
    { ASCIIToUTF16("foo/bar%00"), metrics::OmniboxInputType::QUERY },
    { ASCIIToUTF16("foo/bar/"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("foo/bar baz\\"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("foo.com/bar"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("foo;bar"), metrics::OmniboxInputType::UNKNOWN },
    { ASCIIToUTF16("foo/bar baz"), metrics::OmniboxInputType::UNKNOWN },
    { ASCIIToUTF16("foo bar.com"), metrics::OmniboxInputType::QUERY },
    { ASCIIToUTF16("foo bar"), metrics::OmniboxInputType::QUERY },
    { ASCIIToUTF16("foo+bar"), metrics::OmniboxInputType::QUERY },
    { ASCIIToUTF16("foo+bar.com"), metrics::OmniboxInputType::UNKNOWN },
    { ASCIIToUTF16("\"foo:bar\""), metrics::OmniboxInputType::QUERY },
    { ASCIIToUTF16("link:foo.com"), metrics::OmniboxInputType::UNKNOWN },
    { ASCIIToUTF16("foo:81"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("localhost:8080"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("www.foo.com:81"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("foo.com:123456"), metrics::OmniboxInputType::QUERY },
    { ASCIIToUTF16("foo.com:abc"), metrics::OmniboxInputType::QUERY },
    { ASCIIToUTF16("1.2.3.4:abc"), metrics::OmniboxInputType::QUERY },
    { ASCIIToUTF16("user@foo.com"), metrics::OmniboxInputType::UNKNOWN },
    { ASCIIToUTF16("user@foo/z"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("user@foo/z z"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("user@foo.com/z"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("user:pass@"), metrics::OmniboxInputType::UNKNOWN },
    { ASCIIToUTF16("user:pass@!foo.com"), metrics::OmniboxInputType::UNKNOWN },
    { ASCIIToUTF16("user:pass@foo"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("user:pass@foo.c"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("user:pass@foo.com"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("user:pass@foo.com:81"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("user:pass@foo:81"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("1.2"), metrics::OmniboxInputType::UNKNOWN },
    { ASCIIToUTF16("1.2/45"), metrics::OmniboxInputType::UNKNOWN },
    { ASCIIToUTF16("1.2:45"), metrics::OmniboxInputType::UNKNOWN },
    { ASCIIToUTF16("user@1.2:45"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("user@foo:45"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("user:pass@1.2:45"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("host?query"), metrics::OmniboxInputType::UNKNOWN },
    { ASCIIToUTF16("host#ref"), metrics::OmniboxInputType::UNKNOWN },
    { ASCIIToUTF16("host/path?query"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("host/path#ref"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("en.wikipedia.org/wiki/Jim Beam"),
      metrics::OmniboxInputType::URL },
    // In Chrome itself, mailto: will get handled by ShellExecute, but in
    // unittest mode, we don't have the data loaded in the external protocol
    // handler to know this.
    // { ASCIIToUTF16("mailto:abuse@foo.com"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("view-source:http://www.foo.com/"),
      metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("javascript:alert(\"Hi there\");"),
      metrics::OmniboxInputType::URL },
#if defined(OS_WIN)
    { ASCIIToUTF16("C:\\Program Files"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("\\\\Server\\Folder\\File"),
      metrics::OmniboxInputType::URL },
#endif  // defined(OS_WIN)
    { ASCIIToUTF16("http:foo"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("http://foo"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("http://foo.c"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("http://foo.com"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("http://foo_bar.com"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("http://foo/bar%00"), metrics::OmniboxInputType::QUERY },
    { ASCIIToUTF16("http://foo/bar baz"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("http://-foo.com"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("http://foo-.com"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("http://foo_.com"), metrics::OmniboxInputType::UNKNOWN },
    { ASCIIToUTF16("http://foo.-com"), metrics::OmniboxInputType::UNKNOWN },
    { ASCIIToUTF16("http://_foo_.com"), metrics::OmniboxInputType::UNKNOWN },
    { ASCIIToUTF16("http://foo.com:abc"), metrics::OmniboxInputType::QUERY },
    { ASCIIToUTF16("http://foo.com:123456"), metrics::OmniboxInputType::QUERY },
    { ASCIIToUTF16("http://1.2.3.4:abc"), metrics::OmniboxInputType::QUERY },
    { ASCIIToUTF16("http:user@foo.com"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("http://user@foo.com"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("http:user:pass@foo.com"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("http://user:pass@foo.com"),
      metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("http://1.2"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("http://1.2/45"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("http:ps/2 games"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("https://foo.com"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("127.0.0.1"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("127.0.1"), metrics::OmniboxInputType::UNKNOWN },
    { ASCIIToUTF16("127.0.1/"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("browser.tabs.closeButtons"),
      metrics::OmniboxInputType::UNKNOWN },
    { base::WideToUTF16(L"\u6d4b\u8bd5"), metrics::OmniboxInputType::UNKNOWN },
    { ASCIIToUTF16("[2001:]"), metrics::OmniboxInputType::QUERY },
    { ASCIIToUTF16("[2001:dB8::1]"), metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("192.168.0.256"), metrics::OmniboxInputType::QUERY },
    { ASCIIToUTF16("[foo.com]"), metrics::OmniboxInputType::QUERY },
    { ASCIIToUTF16("filesystem:http://a.com/t/bar"),
      metrics::OmniboxInputType::URL },
    { ASCIIToUTF16("filesystem:http://a.com/"),
      metrics::OmniboxInputType::QUERY },
    { ASCIIToUTF16("filesystem:file://"), metrics::OmniboxInputType::QUERY },
    { ASCIIToUTF16("filesystem:http"), metrics::OmniboxInputType::QUERY },
    { ASCIIToUTF16("filesystem:"), metrics::OmniboxInputType::QUERY },
    { ASCIIToUTF16("chrome-search://"), metrics::OmniboxInputType::QUERY },
    { ASCIIToUTF16("chrome-devtools:"), metrics::OmniboxInputType::QUERY },
    { ASCIIToUTF16("about://f;"), metrics::OmniboxInputType::QUERY },
    { ASCIIToUTF16("://w"), metrics::OmniboxInputType::QUERY },
    { ASCIIToUTF16(":w"), metrics::OmniboxInputType::QUERY },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(input_cases); ++i) {
    SCOPED_TRACE(input_cases[i].input);
    AutocompleteInput input(input_cases[i].input, base::string16::npos,
                            base::string16(), GURL(),
                            OmniboxEventProto::INVALID_SPEC, true, false, true,
                            true, TestSchemeClassifier());
    EXPECT_EQ(input_cases[i].type, input.type());
  }
}

TEST(AutocompleteInputTest, InputTypeWithDesiredTLD) {
  struct test_data {
    const base::string16 input;
    const metrics::OmniboxInputType::Type type;
    const std::string spec;  // Unused if not a URL.
  } input_cases[] = {
    { ASCIIToUTF16("401k"), metrics::OmniboxInputType::URL,
        std::string("http://www.401k.com/") },
    { ASCIIToUTF16("999999999999999"), metrics::OmniboxInputType::URL,
        std::string("http://www.999999999999999.com/") },
    { ASCIIToUTF16("x@y"), metrics::OmniboxInputType::URL,
        std::string("http://x@www.y.com/") },
    { ASCIIToUTF16("y/z z"), metrics::OmniboxInputType::URL,
        std::string("http://www.y.com/z%20z") },
    { ASCIIToUTF16("abc.com"), metrics::OmniboxInputType::URL,
        std::string("http://abc.com/") },
    { ASCIIToUTF16("foo bar"), metrics::OmniboxInputType::QUERY,
        std::string() },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(input_cases); ++i) {
    SCOPED_TRACE(input_cases[i].input);
    AutocompleteInput input(input_cases[i].input, base::string16::npos,
                            ASCIIToUTF16("com"), GURL(),
                            OmniboxEventProto::INVALID_SPEC, true, false, true,
                            true, TestSchemeClassifier());
    EXPECT_EQ(input_cases[i].type, input.type());
    if (input_cases[i].type == metrics::OmniboxInputType::URL)
      EXPECT_EQ(input_cases[i].spec, input.canonicalized_url().spec());
  }
}

// This tests for a regression where certain input in the omnibox caused us to
// crash. As long as the test completes without crashing, we're fine.
TEST(AutocompleteInputTest, InputCrash) {
  AutocompleteInput input(base::WideToUTF16(L"\uff65@s"), base::string16::npos,
                          base::string16(), GURL(),
                          OmniboxEventProto::INVALID_SPEC, true, false,
                          true, true, TestSchemeClassifier());
}

TEST(AutocompleteInputTest, ParseForEmphasizeComponent) {
  using url::Component;
  Component kInvalidComponent(0, -1);
  struct test_data {
    const base::string16 input;
    const Component scheme;
    const Component host;
  } input_cases[] = {
    { base::string16(), kInvalidComponent, kInvalidComponent },
    { ASCIIToUTF16("?"), kInvalidComponent, kInvalidComponent },
    { ASCIIToUTF16("?http://foo.com/bar"), kInvalidComponent,
        kInvalidComponent },
    { ASCIIToUTF16("foo/bar baz"), kInvalidComponent, Component(0, 3) },
    { ASCIIToUTF16("http://foo/bar baz"), Component(0, 4), Component(7, 3) },
    { ASCIIToUTF16("link:foo.com"), Component(0, 4), kInvalidComponent },
    { ASCIIToUTF16("www.foo.com:81"), kInvalidComponent, Component(0, 11) },
    { base::WideToUTF16(L"\u6d4b\u8bd5"), kInvalidComponent, Component(0, 2) },
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
                                                   TestSchemeClassifier(),
                                                   &scheme,
                                                   &host);
    AutocompleteInput input(input_cases[i].input, base::string16::npos,
                            base::string16(), GURL(),
                            OmniboxEventProto::INVALID_SPEC, true,
                            false, true, true, TestSchemeClassifier());
    EXPECT_EQ(input_cases[i].scheme.begin, scheme.begin);
    EXPECT_EQ(input_cases[i].scheme.len, scheme.len);
    EXPECT_EQ(input_cases[i].host.begin, host.begin);
    EXPECT_EQ(input_cases[i].host.len, host.len);
  }
}

TEST(AutocompleteInputTest, InputTypeWithCursorPosition) {
  struct test_data {
    const base::string16 input;
    size_t cursor_position;
    const base::string16 normalized_input;
    size_t normalized_cursor_position;
  } input_cases[] = {
    { ASCIIToUTF16("foo bar"), base::string16::npos,
      ASCIIToUTF16("foo bar"), base::string16::npos },

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
                            base::string16(), GURL(),
                            OmniboxEventProto::INVALID_SPEC,
                            true, false, true, true, TestSchemeClassifier());
    EXPECT_EQ(input_cases[i].normalized_input, input.text());
    EXPECT_EQ(input_cases[i].normalized_cursor_position,
              input.cursor_position());
  }
}
