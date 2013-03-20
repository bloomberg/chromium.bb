// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdlib.h>

#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/net/url_fixer_upper.h"
#include "chrome/common/chrome_paths.h"
#include "googleurl/src/url_parse.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
  class URLFixerUpperTest : public testing::Test {
  };
};

namespace url_parse {

std::ostream& operator<<(std::ostream& os, const Component& part) {
  return os << "(begin=" << part.begin << ", len=" << part.len << ")";
}

}  // namespace url_parse

struct segment_case {
  const std::string input;
  const std::string result;
  const url_parse::Component scheme;
  const url_parse::Component username;
  const url_parse::Component password;
  const url_parse::Component host;
  const url_parse::Component port;
  const url_parse::Component path;
  const url_parse::Component query;
  const url_parse::Component ref;
};

static const segment_case segment_cases[] = {
  { "http://www.google.com/", "http",
    url_parse::Component(0, 4), // scheme
    url_parse::Component(), // username
    url_parse::Component(), // password
    url_parse::Component(7, 14), // host
    url_parse::Component(), // port
    url_parse::Component(21, 1), // path
    url_parse::Component(), // query
    url_parse::Component(), // ref
  },
  { "aBoUt:vErSiOn", "about",
    url_parse::Component(0, 5), // scheme
    url_parse::Component(), // username
    url_parse::Component(), // password
    url_parse::Component(6, 7), // host
    url_parse::Component(), // port
    url_parse::Component(), // path
    url_parse::Component(), // query
    url_parse::Component(), // ref
  },
  { "about:host/path?query#ref", "about",
    url_parse::Component(0, 5), // scheme
    url_parse::Component(), // username
    url_parse::Component(), // password
    url_parse::Component(6, 4), // host
    url_parse::Component(), // port
    url_parse::Component(10, 5), // path
    url_parse::Component(16, 5), // query
    url_parse::Component(22, 3), // ref
  },
  { "about://host/path?query#ref", "about",
    url_parse::Component(0, 5), // scheme
    url_parse::Component(), // username
    url_parse::Component(), // password
    url_parse::Component(8, 4), // host
    url_parse::Component(), // port
    url_parse::Component(12, 5), // path
    url_parse::Component(18, 5), // query
    url_parse::Component(24, 3), // ref
  },
  { "chrome:host/path?query#ref", "chrome",
    url_parse::Component(0, 6), // scheme
    url_parse::Component(), // username
    url_parse::Component(), // password
    url_parse::Component(7, 4), // host
    url_parse::Component(), // port
    url_parse::Component(11, 5), // path
    url_parse::Component(17, 5), // query
    url_parse::Component(23, 3), // ref
  },
  { "chrome://host/path?query#ref", "chrome",
    url_parse::Component(0, 6), // scheme
    url_parse::Component(), // username
    url_parse::Component(), // password
    url_parse::Component(9, 4), // host
    url_parse::Component(), // port
    url_parse::Component(13, 5), // path
    url_parse::Component(19, 5), // query
    url_parse::Component(25, 3), // ref
  },
  { "    www.google.com:124?foo#", "http",
    url_parse::Component(), // scheme
    url_parse::Component(), // username
    url_parse::Component(), // password
    url_parse::Component(4, 14), // host
    url_parse::Component(19, 3), // port
    url_parse::Component(), // path
    url_parse::Component(23, 3), // query
    url_parse::Component(27, 0), // ref
  },
  { "user@www.google.com", "http",
    url_parse::Component(), // scheme
    url_parse::Component(0, 4), // username
    url_parse::Component(), // password
    url_parse::Component(5, 14), // host
    url_parse::Component(), // port
    url_parse::Component(), // path
    url_parse::Component(), // query
    url_parse::Component(), // ref
  },
  { "ftp:/user:P:a$$Wd@..ftp.google.com...::23///pub?foo#bar", "ftp",
    url_parse::Component(0, 3), // scheme
    url_parse::Component(5, 4), // username
    url_parse::Component(10, 7), // password
    url_parse::Component(18, 20), // host
    url_parse::Component(39, 2), // port
    url_parse::Component(41, 6), // path
    url_parse::Component(48, 3), // query
    url_parse::Component(52, 3), // ref
  },
  { "[2001:db8::1]/path", "http",
    url_parse::Component(), // scheme
    url_parse::Component(), // username
    url_parse::Component(), // password
    url_parse::Component(0, 13), // host
    url_parse::Component(), // port
    url_parse::Component(13, 5), // path
    url_parse::Component(), // query
    url_parse::Component(), // ref
  },
  { "[::1]", "http",
    url_parse::Component(), // scheme
    url_parse::Component(), // username
    url_parse::Component(), // password
    url_parse::Component(0, 5), // host
    url_parse::Component(), // port
    url_parse::Component(), // path
    url_parse::Component(), // query
    url_parse::Component(), // ref
  },
  // Incomplete IPv6 addresses (will not canonicalize).
  { "[2001:4860:", "http",
    url_parse::Component(), // scheme
    url_parse::Component(), // username
    url_parse::Component(), // password
    url_parse::Component(0, 11), // host
    url_parse::Component(), // port
    url_parse::Component(), // path
    url_parse::Component(), // query
    url_parse::Component(), // ref
  },
  { "[2001:4860:/foo", "http",
    url_parse::Component(), // scheme
    url_parse::Component(), // username
    url_parse::Component(), // password
    url_parse::Component(0, 11), // host
    url_parse::Component(), // port
    url_parse::Component(11, 4), // path
    url_parse::Component(), // query
    url_parse::Component(), // ref
  },
  { "http://:b005::68]", "http",
    url_parse::Component(0, 4), // scheme
    url_parse::Component(), // username
    url_parse::Component(), // password
    url_parse::Component(7, 10), // host
    url_parse::Component(), // port
    url_parse::Component(), // path
    url_parse::Component(), // query
    url_parse::Component(), // ref
  },
  // Can't do anything useful with this.
  { ":b005::68]", "",
    url_parse::Component(0, 0), // scheme
    url_parse::Component(), // username
    url_parse::Component(), // password
    url_parse::Component(), // host
    url_parse::Component(), // port
    url_parse::Component(), // path
    url_parse::Component(), // query
    url_parse::Component(), // ref
  },
};

TEST(URLFixerUpperTest, SegmentURL) {
  std::string result;
  url_parse::Parsed parts;

  for (size_t i = 0; i < arraysize(segment_cases); ++i) {
    segment_case value = segment_cases[i];
    result = URLFixerUpper::SegmentURL(value.input, &parts);
    EXPECT_EQ(value.result, result);
    EXPECT_EQ(value.scheme, parts.scheme);
    EXPECT_EQ(value.username, parts.username);
    EXPECT_EQ(value.password, parts.password);
    EXPECT_EQ(value.host, parts.host);
    EXPECT_EQ(value.port, parts.port);
    EXPECT_EQ(value.path, parts.path);
    EXPECT_EQ(value.query, parts.query);
    EXPECT_EQ(value.ref, parts.ref);
  }
}

// Creates a file and returns its full name as well as the decomposed
// version. Example:
//    full_path = "c:\foo\bar.txt"
//    dir = "c:\foo"
//    file_name = "bar.txt"
static bool MakeTempFile(const base::FilePath& dir,
                         const base::FilePath& file_name,
                         base::FilePath* full_path) {
  *full_path = dir.Append(file_name);
  return file_util::WriteFile(*full_path, "", 0) == 0;
}

// Returns true if the given URL is a file: URL that matches the given file
static bool IsMatchingFileURL(const std::string& url,
                              const base::FilePath& full_file_path) {
  if (url.length() <= 8)
    return false;
  if (std::string("file:///") != url.substr(0, 8))
    return false; // no file:/// prefix
  if (url.find('\\') != std::string::npos)
    return false; // contains backslashes

  base::FilePath derived_path;
  net::FileURLToFilePath(GURL(url), &derived_path);

  return base::FilePath::CompareEqualIgnoreCase(derived_path.value(),
                                          full_file_path.value());
}

struct fixup_case {
  const std::string input;
  const std::string desired_tld;
  const std::string output;
} fixup_cases[] = {
  {"www.google.com", "", "http://www.google.com/"},
  {" www.google.com     ", "", "http://www.google.com/"},
  {" foo.com/asdf  bar", "", "http://foo.com/asdf%20%20bar"},
  {"..www.google.com..", "", "http://www.google.com./"},
  {"http://......", "", "http://....../"},
  {"http://host.com:ninety-two/", "", "http://host.com:ninety-two/"},
  {"http://host.com:ninety-two?foo", "", "http://host.com:ninety-two/?foo"},
  {"google.com:123", "", "http://google.com:123/"},
  {"about:", "", "chrome://version/"},
  {"about:foo", "", "chrome://foo/"},
  {"about:version", "", "chrome://version/"},
  {"about:usr:pwd@hst/pth?qry#ref", "", "chrome://usr:pwd@hst/pth?qry#ref"},
  {"about://usr:pwd@hst/pth?qry#ref", "", "chrome://usr:pwd@hst/pth?qry#ref"},
  {"chrome:usr:pwd@hst/pth?qry#ref", "", "chrome://usr:pwd@hst/pth?qry#ref"},
  {"chrome://usr:pwd@hst/pth?qry#ref", "", "chrome://usr:pwd@hst/pth?qry#ref"},
  {"www:123", "", "http://www:123/"},
  {"   www:123", "", "http://www:123/"},
  {"www.google.com?foo", "", "http://www.google.com/?foo"},
  {"www.google.com#foo", "", "http://www.google.com/#foo"},
  {"www.google.com?", "", "http://www.google.com/?"},
  {"www.google.com#", "", "http://www.google.com/#"},
  {"www.google.com:123?foo#bar", "", "http://www.google.com:123/?foo#bar"},
  {"user@www.google.com", "", "http://user@www.google.com/"},
  {"\xE6\xB0\xB4.com" , "", "http://xn--1rw.com/"},
  // It would be better if this next case got treated as http, but I don't see
  // a clean way to guess this isn't the new-and-exciting "user" scheme.
  {"user:passwd@www.google.com:8080/", "", "user:passwd@www.google.com:8080/"},
  // {"file:///c:/foo/bar%20baz.txt", "", "file:///C:/foo/bar%20baz.txt"},
  {"ftp.google.com", "", "ftp://ftp.google.com/"},
  {"    ftp.google.com", "", "ftp://ftp.google.com/"},
  {"FTP.GooGle.com", "", "ftp://ftp.google.com/"},
  {"ftpblah.google.com", "", "http://ftpblah.google.com/"},
  {"ftp", "", "http://ftp/"},
  {"google.ftp.com", "", "http://google.ftp.com/"},
  // URLs which end with 0x85 (NEL in ISO-8859).
  { "http://google.com/search?q=\xd0\x85", "",
    "http://google.com/search?q=%D0%85"
  },
  { "http://google.com/search?q=\xec\x97\x85", "",
    "http://google.com/search?q=%EC%97%85"
  },
  { "http://google.com/search?q=\xf0\x90\x80\x85", "",
    "http://google.com/search?q=%F0%90%80%85"
  },
  // URLs which end with 0xA0 (non-break space in ISO-8859).
  { "http://google.com/search?q=\xd0\xa0", "",
    "http://google.com/search?q=%D0%A0"
  },
  { "http://google.com/search?q=\xec\x97\xa0", "",
    "http://google.com/search?q=%EC%97%A0"
  },
  { "http://google.com/search?q=\xf0\x90\x80\xa0", "",
    "http://google.com/search?q=%F0%90%80%A0"
  },
  // URLs containing IPv6 literals.
  {"[2001:db8::2]", "", "http://[2001:db8::2]/"},
  {"[::]:80", "", "http://[::]/"},
  {"[::]:80/path", "", "http://[::]/path"},
  {"[::]:180/path", "", "http://[::]:180/path"},
  // TODO(pmarks): Maybe we should parse bare IPv6 literals someday.
  {"::1", "", "::1"},
  // Semicolon as scheme separator for standard schemes.
  {"http;//www.google.com/", "", "http://www.google.com/"},
  {"about;chrome", "", "chrome://chrome/"},
  // Semicolon left as-is for non-standard schemes.
  {"whatsup;//fool", "", "whatsup://fool"},
  // Semicolon left as-is in URL itself.
  {"http://host/port?query;moar", "", "http://host/port?query;moar"},
  // Fewer slashes than expected.
  {"http;www.google.com/", "", "http://www.google.com/"},
  {"http;/www.google.com/", "", "http://www.google.com/"},
  // Semicolon at start.
  {";http://www.google.com/", "", "http://%3Bhttp//www.google.com/"},
};

TEST(URLFixerUpperTest, FixupURL) {
  for (size_t i = 0; i < arraysize(fixup_cases); ++i) {
    fixup_case value = fixup_cases[i];
    EXPECT_EQ(value.output, URLFixerUpper::FixupURL(value.input,
        value.desired_tld).possibly_invalid_spec())
        << "input: " << value.input;
  }

  // Check the TLD-appending functionality
  fixup_case tld_cases[] = {
    {"google", "com", "http://www.google.com/"},
    {"google.", "com", "http://www.google.com/"},
    {"google..", "com", "http://www.google.com/"},
    {".google", "com", "http://www.google.com/"},
    {"www.google", "com", "http://www.google.com/"},
    {"google.com", "com", "http://google.com/"},
    {"http://google", "com", "http://www.google.com/"},
    {"..google..", "com", "http://www.google.com/"},
    {"http://www.google", "com", "http://www.google.com/"},
    {"9999999999999999", "com", "http://www.9999999999999999.com/"},
    {"google/foo", "com", "http://www.google.com/foo"},
    {"google.com/foo", "com", "http://google.com/foo"},
    {"google/?foo=.com", "com", "http://www.google.com/?foo=.com"},
    {"www.google/?foo=www.", "com", "http://www.google.com/?foo=www."},
    {"google.com/?foo=.com", "com", "http://google.com/?foo=.com"},
    {"http://www.google.com", "com", "http://www.google.com/"},
    {"google:123", "com", "http://www.google.com:123/"},
    {"http://google:123", "com", "http://www.google.com:123/"},
  };
  for (size_t i = 0; i < arraysize(tld_cases); ++i) {
    fixup_case value = tld_cases[i];
    EXPECT_EQ(value.output, URLFixerUpper::FixupURL(value.input,
        value.desired_tld).possibly_invalid_spec());
  }
}

// Test different types of file inputs to URIFixerUpper::FixupURL. This
// doesn't go into the nice array of fixups above since the file input
// has to exist.
TEST(URLFixerUpperTest, FixupFile) {
  // this "original" filename is the one we tweak to get all the variations
  base::FilePath dir;
  base::FilePath original;
  ASSERT_TRUE(PathService::Get(chrome::DIR_APP, &dir));
  ASSERT_TRUE(MakeTempFile(
      dir,
      base::FilePath(FILE_PATH_LITERAL("url fixer upper existing file.txt")),
      &original));

  // reference path
  GURL golden(net::FilePathToFileURL(original));

  // c:\foo\bar.txt -> file:///c:/foo/bar.txt (basic)
#if defined(OS_WIN)
  GURL fixedup(URLFixerUpper::FixupURL(WideToUTF8(original.value()),
                                       std::string()));
#elif defined(OS_POSIX)
  GURL fixedup(URLFixerUpper::FixupURL(original.value(), std::string()));
#endif
  EXPECT_EQ(golden, fixedup);

  // TODO(port): Make some equivalent tests for posix.
#if defined(OS_WIN)
  // c|/foo\bar.txt -> file:///c:/foo/bar.txt (pipe allowed instead of colon)
  std::string cur(WideToUTF8(original.value()));
  EXPECT_EQ(':', cur[1]);
  cur[1] = '|';
  EXPECT_EQ(golden, URLFixerUpper::FixupURL(cur, std::string()));

  fixup_case file_cases[] = {
    {"c:\\This%20is a non-existent file.txt", "",
     "file:///C:/This%2520is%20a%20non-existent%20file.txt"},

    // \\foo\bar.txt -> file://foo/bar.txt
    // UNC paths, this file won't exist, but since there are no escapes, it
    // should be returned just converted to a file: URL.
    {"\\\\SomeNonexistentHost\\foo\\bar.txt", "",
     "file://somenonexistenthost/foo/bar.txt"},
    // We do this strictly, like IE8, which only accepts this form using
    // backslashes and not forward ones.  Turning "//foo" into "http" matches
    // Firefox and IE, silly though it may seem (it falls out of adding "http"
    // as the default protocol if you haven't entered one).
    {"//SomeNonexistentHost\\foo/bar.txt", "",
     "http://somenonexistenthost/foo/bar.txt"},
    {"file:///C:/foo/bar", "", "file:///C:/foo/bar"},

    // Much of the work here comes from GURL's canonicalization stage.
    {"file://C:/foo/bar", "", "file:///C:/foo/bar"},
    {"file:c:", "", "file:///C:/"},
    {"file:c:WINDOWS", "", "file:///C:/WINDOWS"},
    {"file:c|Program Files", "", "file:///C:/Program%20Files"},
    {"file:/file", "", "file://file/"},
    {"file:////////c:\\foo", "", "file:///C:/foo"},
    {"file://server/folder/file", "", "file://server/folder/file"},

    // These are fixups we don't do, but could consider:
    //
    //   {"file:///foo:/bar", "", "file://foo/bar"},
    //   {"file:/\\/server\\folder/file", "", "file://server/folder/file"},
  };
#elif defined(OS_POSIX)

#if defined(OS_MACOSX)
#define HOME "/Users/"
#else
#define HOME "/home/"
#endif
  URLFixerUpper::home_directory_override = "/foo";
  fixup_case file_cases[] = {
    // File URLs go through GURL, which tries to escape intelligently.
    {"/This%20is a non-existent file.txt", "",
     "file:///This%2520is%20a%20non-existent%20file.txt"},
    // A plain "/" refers to the root.
    {"/", "",
     "file:///"},

    // These rely on the above home_directory_override.
    {"~", "",
     "file:///foo"},
    {"~/bar", "",
     "file:///foo/bar"},

    // References to other users' homedirs.
    {"~foo", "",
     "file://" HOME "foo"},
    {"~x/blah", "",
     "file://" HOME "x/blah"},
  };
#endif
  for (size_t i = 0; i < arraysize(file_cases); i++) {
    EXPECT_EQ(file_cases[i].output, URLFixerUpper::FixupURL(file_cases[i].input,
        file_cases[i].desired_tld).possibly_invalid_spec());
  }

  EXPECT_TRUE(file_util::Delete(original, false));
}

TEST(URLFixerUpperTest, FixupRelativeFile) {
  base::FilePath full_path, dir;
  base::FilePath file_part(
      FILE_PATH_LITERAL("url_fixer_upper_existing_file.txt"));
  ASSERT_TRUE(PathService::Get(chrome::DIR_APP, &dir));
  ASSERT_TRUE(MakeTempFile(dir, file_part, &full_path));
  ASSERT_TRUE(file_util::AbsolutePath(&full_path));

  // make sure we pass through good URLs
  for (size_t i = 0; i < arraysize(fixup_cases); ++i) {
    fixup_case value = fixup_cases[i];
#if defined(OS_WIN)
    base::FilePath input(UTF8ToWide(value.input));
#elif defined(OS_POSIX)
    base::FilePath input(value.input);
#endif
    EXPECT_EQ(value.output,
        URLFixerUpper::FixupRelativeFile(dir, input).possibly_invalid_spec());
  }

  // make sure the existing file got fixed-up to a file URL, and that there
  // are no backslashes
  EXPECT_TRUE(IsMatchingFileURL(URLFixerUpper::FixupRelativeFile(dir,
      file_part).possibly_invalid_spec(), full_path));
  EXPECT_TRUE(file_util::Delete(full_path, false));

  // create a filename we know doesn't exist and make sure it doesn't get
  // fixed up to a file URL
  base::FilePath nonexistent_file(
      FILE_PATH_LITERAL("url_fixer_upper_nonexistent_file.txt"));
  std::string fixedup(URLFixerUpper::FixupRelativeFile(dir,
      nonexistent_file).possibly_invalid_spec());
  EXPECT_NE(std::string("file:///"), fixedup.substr(0, 8));
  EXPECT_FALSE(IsMatchingFileURL(fixedup, nonexistent_file));

  // make a subdir to make sure relative paths with directories work, also
  // test spaces:
  // "app_dir\url fixer-upper dir\url fixer-upper existing file.txt"
  base::FilePath sub_dir(FILE_PATH_LITERAL("url fixer-upper dir"));
  base::FilePath sub_file(
      FILE_PATH_LITERAL("url fixer-upper existing file.txt"));
  base::FilePath new_dir = dir.Append(sub_dir);
  file_util::CreateDirectory(new_dir);
  ASSERT_TRUE(MakeTempFile(new_dir, sub_file, &full_path));
  ASSERT_TRUE(file_util::AbsolutePath(&full_path));

  // test file in the subdir
  base::FilePath relative_file = sub_dir.Append(sub_file);
  EXPECT_TRUE(IsMatchingFileURL(URLFixerUpper::FixupRelativeFile(dir,
      relative_file).possibly_invalid_spec(), full_path));

  // test file in the subdir with different slashes and escaping.
  base::FilePath::StringType relative_file_str = sub_dir.value() +
      FILE_PATH_LITERAL("/") + sub_file.value();
  ReplaceSubstringsAfterOffset(&relative_file_str, 0,
      FILE_PATH_LITERAL(" "), FILE_PATH_LITERAL("%20"));
  EXPECT_TRUE(IsMatchingFileURL(URLFixerUpper::FixupRelativeFile(dir,
      base::FilePath(relative_file_str)).possibly_invalid_spec(), full_path));

  // test relative directories and duplicate slashes
  // (should resolve to the same file as above)
  relative_file_str = sub_dir.value() + FILE_PATH_LITERAL("/../") +
      sub_dir.value() + FILE_PATH_LITERAL("///./") + sub_file.value();
  EXPECT_TRUE(IsMatchingFileURL(URLFixerUpper::FixupRelativeFile(dir,
      base::FilePath(relative_file_str)).possibly_invalid_spec(), full_path));

  // done with the subdir
  EXPECT_TRUE(file_util::Delete(full_path, false));
  EXPECT_TRUE(file_util::Delete(new_dir, true));

  // Test that an obvious HTTP URL isn't accidentally treated as an absolute
  // file path (on account of system-specific craziness).
  base::FilePath empty_path;
  base::FilePath http_url_path(FILE_PATH_LITERAL("http://../"));
  EXPECT_TRUE(URLFixerUpper::FixupRelativeFile(
      empty_path, http_url_path).SchemeIs("http"));
}
