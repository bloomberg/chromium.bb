// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/elide_url.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/text_elider.h"
#include "ui/gfx/text_utils.h"
#include "url/gurl.h"

using base::UTF8ToUTF16;
using gfx::GetStringWidthF;
using gfx::kEllipsis;

namespace {

struct Testcase {
  const std::string input;
  const std::string output;
};

void RunUrlTest(Testcase* testcases, size_t num_testcases) {
  static const gfx::FontList font_list;
  for (size_t i = 0; i < num_testcases; ++i) {
    const GURL url(testcases[i].input);
    // Should we test with non-empty language list?
    // That's kinda redundant with net_util_unittests.
    const float available_width =
        GetStringWidthF(UTF8ToUTF16(testcases[i].output), font_list);
    EXPECT_EQ(UTF8ToUTF16(testcases[i].output),
              ElideUrl(url, font_list, available_width, std::string()));
  }
}

// Test eliding of commonplace URLs.
TEST(TextEliderTest, TestGeneralEliding) {
  const std::string kEllipsisStr(kEllipsis);
  Testcase testcases[] = {
    {"http://www.google.com/intl/en/ads/",
     "www.google.com/intl/en/ads/"},
    {"http://www.google.com/intl/en/ads/", "www.google.com/intl/en/ads/"},
    {"http://www.google.com/intl/en/ads/",
     "google.com/intl/" + kEllipsisStr + "/ads/"},
    {"http://www.google.com/intl/en/ads/",
     "google.com/" + kEllipsisStr + "/ads/"},
    {"http://www.google.com/intl/en/ads/", "google.com/" + kEllipsisStr},
    {"http://www.google.com/intl/en/ads/", "goog" + kEllipsisStr},
    {"https://subdomain.foo.com/bar/filename.html",
     "subdomain.foo.com/bar/filename.html"},
    {"https://subdomain.foo.com/bar/filename.html",
     "subdomain.foo.com/" + kEllipsisStr + "/filename.html"},
    {"http://subdomain.foo.com/bar/filename.html",
     kEllipsisStr + "foo.com/" + kEllipsisStr + "/filename.html"},
    {"http://www.google.com/intl/en/ads/?aLongQueryWhichIsNotRequired",
     "www.google.com/intl/en/ads/?aLongQ" + kEllipsisStr},
  };

  RunUrlTest(testcases, arraysize(testcases));
}

// When there is very little space available, the elision code will shorten
// both path AND file name to an ellipsis - ".../...". To avoid this result,
// there is a hack in place that simply treats them as one string in this
// case.
TEST(TextEliderTest, TestTrailingEllipsisSlashEllipsisHack) {
  const std::string kEllipsisStr(kEllipsis);

  // Very little space, would cause double ellipsis.
  gfx::FontList font_list;
  GURL url("http://battersbox.com/directory/foo/peter_paul_and_mary.html");
  float available_width = GetStringWidthF(
      UTF8ToUTF16("battersbox.com/" + kEllipsisStr + "/" + kEllipsisStr),
      font_list);

  // Create the expected string, after elision. Depending on font size, the
  // directory might become /dir... or /di... or/d... - it never should be
  // shorter than that. (If it is, the font considers d... to be longer
  // than .../... -  that should never happen).
  ASSERT_GT(GetStringWidthF(UTF8ToUTF16(kEllipsisStr + "/" + kEllipsisStr),
                            font_list),
            GetStringWidthF(UTF8ToUTF16("d" + kEllipsisStr), font_list));
  GURL long_url("http://battersbox.com/directorynameisreallylongtoforcetrunc");
  base::string16 expected =
      ElideUrl(long_url, font_list, available_width, std::string());
  // Ensure that the expected result still contains part of the directory name.
  ASSERT_GT(expected.length(), std::string("battersbox.com/d").length());
  EXPECT_EQ(expected,
             ElideUrl(url, font_list, available_width, std::string()));

  // More space available - elide directories, partially elide filename.
  Testcase testcases[] = {
    {"http://battersbox.com/directory/foo/peter_paul_and_mary.html",
     "battersbox.com/" + kEllipsisStr + "/peter" + kEllipsisStr},
  };
  RunUrlTest(testcases, arraysize(testcases));
}

// Test eliding of empty strings, URLs with ports, passwords, queries, etc.
TEST(TextEliderTest, TestMoreEliding) {
  const std::string kEllipsisStr(kEllipsis);
  Testcase testcases[] = {
    {"http://www.google.com/foo?bar", "www.google.com/foo?bar"},
    {"http://xyz.google.com/foo?bar", "xyz.google.com/foo?" + kEllipsisStr},
    {"http://xyz.google.com/foo?bar", "xyz.google.com/foo" + kEllipsisStr},
    {"http://xyz.google.com/foo?bar", "xyz.google.com/fo" + kEllipsisStr},
    {"http://a.b.com/pathname/c?d", "a.b.com/" + kEllipsisStr + "/c?d"},
    {"", ""},
    {"http://foo.bar..example.com...hello/test/filename.html",
     "foo.bar..example.com...hello/" + kEllipsisStr + "/filename.html"},
    {"http://foo.bar../", "foo.bar.."},
    {"http://xn--1lq90i.cn/foo", "\xe5\x8c\x97\xe4\xba\xac.cn/foo"},
    {"http://me:mypass@secrethost.com:99/foo?bar#baz",
     "secrethost.com:99/foo?bar#baz"},
    {"http://me:mypass@ss%xxfdsf.com/foo", "ss%25xxfdsf.com/foo"},
    {"mailto:elgoato@elgoato.com", "mailto:elgoato@elgoato.com"},
    {"javascript:click(0)", "javascript:click(0)"},
    {"https://chess.eecs.berkeley.edu:4430/login/arbitfilename",
     "chess.eecs.berkeley.edu:4430/login/arbitfilename"},
    {"https://chess.eecs.berkeley.edu:4430/login/arbitfilename",
     kEllipsisStr + "berkeley.edu:4430/" + kEllipsisStr + "/arbitfilename"},

    // Unescaping.
    {"http://www/%E4%BD%A0%E5%A5%BD?q=%E4%BD%A0%E5%A5%BD#\xe4\xbd\xa0",
     "www/\xe4\xbd\xa0\xe5\xa5\xbd?q=\xe4\xbd\xa0\xe5\xa5\xbd#\xe4\xbd\xa0"},

    // Invalid unescaping for path. The ref will always be valid UTF-8. We don't
    // bother to do too many edge cases, since these are handled by the escaper
    // unittest.
    {"http://www/%E4%A0%E5%A5%BD?q=%E4%BD%A0%E5%A5%BD#\xe4\xbd\xa0",
     "www/%E4%A0%E5%A5%BD?q=\xe4\xbd\xa0\xe5\xa5\xbd#\xe4\xbd\xa0"},
  };

  RunUrlTest(testcases, arraysize(testcases));
}

// Test eliding of file: URLs.
TEST(TextEliderTest, TestFileURLEliding) {
  const std::string kEllipsisStr(kEllipsis);
  Testcase testcases[] = {
    {"file:///C:/path1/path2/path3/filename",
     "file:///C:/path1/path2/path3/filename"},
    {"file:///C:/path1/path2/path3/filename",
     "C:/path1/path2/path3/filename"},
// GURL parses "file:///C:path" differently on windows than it does on posix.
#if defined(OS_WIN)
    {"file:///C:path1/path2/path3/filename",
     "C:/path1/path2/" + kEllipsisStr + "/filename"},
    {"file:///C:path1/path2/path3/filename",
     "C:/path1/" + kEllipsisStr + "/filename"},
    {"file:///C:path1/path2/path3/filename",
     "C:/" + kEllipsisStr + "/filename"},
#endif
    {"file://filer/foo/bar/file", "filer/foo/bar/file"},
    {"file://filer/foo/bar/file", "filer/foo/" + kEllipsisStr + "/file"},
    {"file://filer/foo/bar/file", "filer/" + kEllipsisStr + "/file"},
  };

  RunUrlTest(testcases, arraysize(testcases));
}

TEST(TextEliderTest, TestHostEliding) {
  const std::string kEllipsisStr(kEllipsis);
  Testcase testcases[] = {
    {"http://google.com", "google.com"},
    {"http://subdomain.google.com", kEllipsisStr + ".google.com"},
    {"http://reallyreallyreallylongdomainname.com",
         "reallyreallyreallylongdomainname.com"},
    {"http://a.b.c.d.e.f.com", kEllipsisStr + "f.com"},
    {"http://foo", "foo"},
    {"http://foo.bar", "foo.bar"},
    {"http://subdomain.foo.bar", kEllipsisStr + "in.foo.bar"},
// IOS width calculations are off by a letter from other platforms for
// some strings from other platforms, probably for strings with too
// many kerned letters on the default font set.
#if !defined(OS_IOS)
    {"http://subdomain.reallylongdomainname.com",
         kEllipsisStr + "ain.reallylongdomainname.com"},
    {"http://a.b.c.d.e.f.com", kEllipsisStr + ".e.f.com"},
#endif
  };

  for (size_t i = 0; i < arraysize(testcases); ++i) {
    const float available_width =
        GetStringWidthF(UTF8ToUTF16(testcases[i].output), gfx::FontList());
    EXPECT_EQ(UTF8ToUTF16(testcases[i].output), ElideHost(
        GURL(testcases[i].input), gfx::FontList(), available_width));
  }

  // Trying to elide to a really short length will still keep the full TLD+1
  EXPECT_EQ(base::ASCIIToUTF16("google.com"),
            ElideHost(GURL("http://google.com"), gfx::FontList(), 2));
  EXPECT_EQ(base::UTF8ToUTF16(kEllipsisStr + ".google.com"),
            ElideHost(GURL("http://subdomain.google.com"), gfx::FontList(), 2));
  EXPECT_EQ(base::ASCIIToUTF16("foo.bar"),
            ElideHost(GURL("http://foo.bar"), gfx::FontList(), 2));
}

}  // namespace
