// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_util.h"

#include <string.h>

#include <algorithm>

#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/scoped_native_library.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_byteorder.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include <iphlpapi.h>
#include <objbase.h>
#include "base/win/windows_version.h"
#elif !defined(OS_ANDROID)
#include <net/if.h>
#endif  // OS_WIN

using base::ASCIIToUTF16;
using base::WideToUTF16;

namespace net {

namespace {

static const size_t kNpos = base::string16::npos;

struct HeaderCase {
  const char* header_name;
  const char* expected;
};

struct HeaderParamCase {
  const char* header_name;
  const char* param_name;
  const char* expected;
};

const char* kLanguages[] = {
  "",      "en",    "zh-CN",    "ja",    "ko",
  "he",    "ar",    "ru",       "el",    "fr",
  "de",    "pt",    "sv",       "th",    "hi",
  "de,en", "el,en", "zh-TW,en", "ko,ja", "he,ru,en",
  "zh,ru,en"
};

struct IDNTestCase {
  const char* input;
  const wchar_t* unicode_output;
  const bool unicode_allowed[arraysize(kLanguages)];
};

// TODO(jungshik) This is just a random sample of languages and is far
// from exhaustive.  We may have to generate all the combinations
// of languages (powerset of a set of all the languages).
const IDNTestCase idn_cases[] = {
  // No IDN
  {"www.google.com", L"www.google.com",
   {true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true}},
  {"www.google.com.", L"www.google.com.",
   {true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true}},
  {".", L".",
   {true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true}},
  {"", L"",
   {true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true}},
  // IDN
  // Hanzi (Traditional Chinese)
  {"xn--1lq90ic7f1rc.cn", L"\x5317\x4eac\x5927\x5b78.cn",
   {true,  false, true,  true,  false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, true,  true,  false,
    true}},
  // Hanzi ('video' in Simplified Chinese : will pass only in zh-CN,zh)
  {"xn--cy2a840a.com", L"\x89c6\x9891.com",
   {true,  false, true,  false,  false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false,  false,
    true}},
  // Hanzi + '123'
  {"www.xn--123-p18d.com", L"www.\x4e00" L"123.com",
   {true,  false, true,  true,  false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, true,  true,  false,
    true}},
  // Hanzi + Latin : U+56FD is simplified and is regarded
  // as not supported in zh-TW.
  {"www.xn--hello-9n1hm04c.com", L"www.hello\x4e2d\x56fd.com",
   {false, false, true,  true,  false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, true,  false,
    true}},
  // Kanji + Kana (Japanese)
  {"xn--l8jvb1ey91xtjb.jp", L"\x671d\x65e5\x3042\x3055\x3072.jp",
   {true,  false, false, true,  false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, true,  false,
    false}},
  // Katakana including U+30FC
  {"xn--tckm4i2e.jp", L"\x30b3\x30de\x30fc\x30b9.jp",
   {true, false, false, true,  false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, true, false,
    }},
  {"xn--3ck7a7g.jp", L"\u30ce\u30f3\u30bd.jp",
   {true, false, false, true,  false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, true, false,
    }},
  // Katakana + Latin (Japanese)
  // TODO(jungshik): Change 'false' in the first element to 'true'
  // after upgrading to ICU 4.2.1 to use new uspoof_* APIs instead
  // of our IsIDNComponentInSingleScript().
  {"xn--e-efusa1mzf.jp", L"e\x30b3\x30de\x30fc\x30b9.jp",
   {false, false, false, true,  false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, true, false,
    }},
  {"xn--3bkxe.jp", L"\x30c8\x309a.jp",
   {false, false, false, true,  false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, true, false,
    }},
  // Hangul (Korean)
  {"www.xn--or3b17p6jjc.kr", L"www.\xc804\xc790\xc815\xbd80.kr",
   {true,  false, false, false, true,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, true,  false,
    false}},
  // b<u-umlaut>cher (German)
  {"xn--bcher-kva.de", L"b\x00fc" L"cher.de",
   {true,  false, false, false, false,
    false, false, false, false, true,
    true,  false,  false, false, false,
    true,  false, false, false, false,
    false}},
  // a with diaeresis
  {"www.xn--frgbolaget-q5a.se", L"www.f\x00e4rgbolaget.se",
   {true,  false, false, false, false,
    false, false, false, false, false,
    true,  false, true, false, false,
    true,  false, false, false, false,
    false}},
  // c-cedilla (French)
  {"www.xn--alliancefranaise-npb.fr", L"www.alliancefran\x00e7" L"aise.fr",
   {true,  false, false, false, false,
    false, false, false, false, true,
    false, true,  false, false, false,
    false, false, false, false, false,
    false}},
  // caf'e with acute accent' (French)
  {"xn--caf-dma.fr", L"caf\x00e9.fr",
   {true,  false, false, false, false,
    false, false, false, false, true,
    false, true,  true,  false, false,
    false, false, false, false, false,
    false}},
  // c-cedillla and a with tilde (Portuguese)
  {"xn--poema-9qae5a.com.br", L"p\x00e3oema\x00e7\x00e3.com.br",
   {true,  false, false, false, false,
    false, false, false, false, false,
    false, true,  false, false, false,
    false, false, false, false, false,
    false}},
  // s with caron
  {"xn--achy-f6a.com", L"\x0161" L"achy.com",
   {true,  false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false}},
  // TODO(jungshik) : Add examples with Cyrillic letters
  // only used in some languages written in Cyrillic.
  // Eutopia (Greek)
  {"xn--kxae4bafwg.gr", L"\x03bf\x03c5\x03c4\x03bf\x03c0\x03af\x03b1.gr",
   {true,  false, false, false, false,
    false, false, false, true,  false,
    false, false, false, false, false,
    false, true,  false, false, false,
    false}},
  // Eutopia + 123 (Greek)
  {"xn---123-pldm0haj2bk.gr",
   L"\x03bf\x03c5\x03c4\x03bf\x03c0\x03af\x03b1-123.gr",
   {true,  false, false, false, false,
    false, false, false, true,  false,
    false, false, false, false, false,
    false, true,  false, false, false,
    false}},
  // Cyrillic (Russian)
  {"xn--n1aeec9b.ru", L"\x0442\x043e\x0440\x0442\x044b.ru",
   {true,  false, false, false, false,
    false, false, true,  false, false,
    false, false, false, false, false,
    false, false, false, false, true,
    true}},
  // Cyrillic + 123 (Russian)
  {"xn---123-45dmmc5f.ru", L"\x0442\x043e\x0440\x0442\x044b-123.ru",
   {true,  false, false, false, false,
    false, false, true,  false, false,
    false, false, false, false, false,
    false, false, false, false, true,
    true}},
  // Arabic
  {"xn--mgba1fmg.ar", L"\x0627\x0641\x0644\x0627\x0645.ar",
   {true,  false, false, false, false,
    false, true,  false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false}},
  // Hebrew
  {"xn--4dbib.he", L"\x05d5\x05d0\x05d4.he",
   {true,  false, false, false, false,
    true,  false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, true,
    false}},
  // Thai
  {"xn--12c2cc4ag3b4ccu.th",
   L"\x0e2a\x0e32\x0e22\x0e01\x0e32\x0e23\x0e1a\x0e34\x0e19.th",
   {true,  false, false, false, false,
    false, false, false, false, false,
    false, false, false, true,  false,
    false, false, false, false, false,
    false}},
  // Devangari (Hindi)
  {"www.xn--l1b6a9e1b7c.in", L"www.\x0905\x0915\x094b\x0932\x093e.in",
   {true,  false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, true,
    false, false, false, false, false,
    false}},
  // Invalid IDN
  {"xn--hello?world.com", NULL,
   {false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false}},
  // Unsafe IDNs
  // "payp<alpha>l.com"
  {"www.xn--paypl-g9d.com", L"payp\x03b1l.com",
   {false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false}},
  // google.gr with Greek omicron and epsilon
  {"xn--ggl-6xc1ca.gr", L"g\x03bf\x03bfgl\x03b5.gr",
   {false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false}},
  // google.ru with Cyrillic o
  {"xn--ggl-tdd6ba.ru", L"g\x043e\x043egl\x0435.ru",
   {false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false}},
  // h<e with acute>llo<China in Han>.cn
  {"xn--hllo-bpa7979ih5m.cn", L"h\x00e9llo\x4e2d\x56fd.cn",
   {false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false}},
  // <Greek rho><Cyrillic a><Cyrillic u>.ru
  {"xn--2xa6t2b.ru", L"\x03c1\x0430\x0443.ru",
   {false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false, false, false, false, false,
    false}},
  // One that's really long that will force a buffer realloc
  {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
       "aaaaaaa",
   L"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
       L"aaaaaaaa",
   {true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true,  true,  true,  true,  true,
    true}},
  // Test cases for characters we blacklisted although allowed in IDN.
  // Embedded spaces will be turned to %20 in the display.
  // TODO(jungshik): We need to have more cases. This is a typical
  // data-driven trap. The following test cases need to be separated
  // and tested only for a couple of languages.
  {"xn--osd3820f24c.kr", L"\xac00\xb098\x115f.kr",
    {false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
     false}},
  {"www.xn--google-ho0coa.com", L"www.\x2039google\x203a.com",
    {false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
  }},
  {"google.xn--comabc-k8d", L"google.com\x0338" L"abc",
    {false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
  }},
  {"google.xn--com-oh4ba.evil.jp", L"google.com\x309a\x309a.evil.jp",
    {false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
  }},
  {"google.xn--comevil-v04f.jp", L"google.com\x30ce" L"evil.jp",
    {false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
     false, false, false, false, false,
  }},
#if 0
  // These two cases are special. We need a separate test.
  // U+3000 and U+3002 are normalized to ASCII space and dot.
  {"xn-- -kq6ay5z.cn", L"\x4e2d\x56fd\x3000.cn",
    {false, false, true,  false, false,
     false, false, false, false, false,
     false, false, false, false, false,
     false, false, true,  false, false,
     true}},
  {"xn--fiqs8s.cn", L"\x4e2d\x56fd\x3002" L"cn",
    {false, false, true,  false, false,
     false, false, false, false, false,
     false, false, false, false, false,
     false, false, true,  false, false,
     true}},
#endif
};

struct AdjustOffsetCase {
  size_t input_offset;
  size_t output_offset;
};

struct CompliantHostCase {
  const char* host;
  const char* desired_tld;
  bool expected_output;
};

struct UrlTestData {
  const char* description;
  const char* input;
  const char* languages;
  FormatUrlTypes format_types;
  UnescapeRule::Type escape_rules;
  const wchar_t* output;  // Use |wchar_t| to handle Unicode constants easily.
  size_t prefix_len;
};

// Fills in sockaddr for the given 32-bit address (IPv4.)
// |bytes| should be an array of length 4.
void MakeIPv4Address(const uint8* bytes, int port, SockaddrStorage* storage) {
  memset(&storage->addr_storage, 0, sizeof(storage->addr_storage));
  storage->addr_len = sizeof(struct sockaddr_in);
  struct sockaddr_in* addr4 = reinterpret_cast<sockaddr_in*>(storage->addr);
  addr4->sin_port = base::HostToNet16(port);
  addr4->sin_family = AF_INET;
  memcpy(&addr4->sin_addr, bytes, 4);
}

// Fills in sockaddr for the given 128-bit address (IPv6.)
// |bytes| should be an array of length 16.
void MakeIPv6Address(const uint8* bytes, int port, SockaddrStorage* storage) {
  memset(&storage->addr_storage, 0, sizeof(storage->addr_storage));
  storage->addr_len = sizeof(struct sockaddr_in6);
  struct sockaddr_in6* addr6 = reinterpret_cast<sockaddr_in6*>(storage->addr);
  addr6->sin6_port = base::HostToNet16(port);
  addr6->sin6_family = AF_INET6;
  memcpy(&addr6->sin6_addr, bytes, 16);
}

// A helper for IDN*{Fast,Slow}.
// Append "::<language list>" to |expected| and |actual| to make it
// easy to tell which sub-case fails without debugging.
void AppendLanguagesToOutputs(const char* languages,
                              base::string16* expected,
                              base::string16* actual) {
  base::string16 to_append = ASCIIToUTF16("::") + ASCIIToUTF16(languages);
  expected->append(to_append);
  actual->append(to_append);
}

// A pair of helpers for the FormatUrlWithOffsets() test.
void VerboseExpect(size_t expected,
                   size_t actual,
                   const std::string& original_url,
                   size_t position,
                   const base::string16& formatted_url) {
  EXPECT_EQ(expected, actual) << "Original URL: " << original_url
      << " (at char " << position << ")\nFormatted URL: " << formatted_url;
}

void CheckAdjustedOffsets(const std::string& url_string,
                          const std::string& languages,
                          FormatUrlTypes format_types,
                          UnescapeRule::Type unescape_rules,
                          const size_t* output_offsets) {
  GURL url(url_string);
  size_t url_length = url_string.length();
  std::vector<size_t> offsets;
  for (size_t i = 0; i <= url_length + 1; ++i)
    offsets.push_back(i);
  offsets.push_back(500000);  // Something larger than any input length.
  offsets.push_back(std::string::npos);
  base::string16 formatted_url = FormatUrlWithOffsets(url, languages,
      format_types, unescape_rules, NULL, NULL, &offsets);
  for (size_t i = 0; i < url_length; ++i)
    VerboseExpect(output_offsets[i], offsets[i], url_string, i, formatted_url);
  VerboseExpect(formatted_url.length(), offsets[url_length], url_string,
                url_length, formatted_url);
  VerboseExpect(base::string16::npos, offsets[url_length + 1], url_string,
                500000, formatted_url);
  VerboseExpect(base::string16::npos, offsets[url_length + 2], url_string,
                std::string::npos, formatted_url);
}

// Helper to strignize an IP number (used to define expectations).
std::string DumpIPNumber(const IPAddressNumber& v) {
  std::string out;
  for (size_t i = 0; i < v.size(); ++i) {
    if (i != 0)
      out.append(",");
    out.append(base::IntToString(static_cast<int>(v[i])));
  }
  return out;
}

}  // anonymous namespace

TEST(NetUtilTest, GetIdentityFromURL) {
  struct {
    const char* input_url;
    const char* expected_username;
    const char* expected_password;
  } tests[] = {
    {
      "http://username:password@google.com",
      "username",
      "password",
    },
    { // Test for http://crbug.com/19200
      "http://username:p@ssword@google.com",
      "username",
      "p@ssword",
    },
    { // Special URL characters should be unescaped.
      "http://username:p%3fa%26s%2fs%23@google.com",
      "username",
      "p?a&s/s#",
    },
    { // Username contains %20.
      "http://use rname:password@google.com",
      "use rname",
      "password",
    },
    { // Keep %00 as is.
      "http://use%00rname:password@google.com",
      "use%00rname",
      "password",
    },
    { // Use a '+' in the username.
      "http://use+rname:password@google.com",
      "use+rname",
      "password",
    },
    { // Use a '&' in the password.
      "http://username:p&ssword@google.com",
      "username",
      "p&ssword",
    },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s", i,
                                    tests[i].input_url));
    GURL url(tests[i].input_url);

    base::string16 username, password;
    GetIdentityFromURL(url, &username, &password);

    EXPECT_EQ(ASCIIToUTF16(tests[i].expected_username), username);
    EXPECT_EQ(ASCIIToUTF16(tests[i].expected_password), password);
  }
}

// Try extracting a username which was encoded with UTF8.
TEST(NetUtilTest, GetIdentityFromURL_UTF8) {
  GURL url(WideToUTF16(L"http://foo:\x4f60\x597d@blah.com"));

  EXPECT_EQ("foo", url.username());
  EXPECT_EQ("%E4%BD%A0%E5%A5%BD", url.password());

  // Extract the unescaped identity.
  base::string16 username, password;
  GetIdentityFromURL(url, &username, &password);

  // Verify that it was decoded as UTF8.
  EXPECT_EQ(ASCIIToUTF16("foo"), username);
  EXPECT_EQ(WideToUTF16(L"\x4f60\x597d"), password);
}

// Just a bunch of fake headers.
const char* google_headers =
    "HTTP/1.1 200 OK\n"
    "Content-TYPE: text/html; charset=utf-8\n"
    "Content-disposition: attachment; filename=\"download.pdf\"\n"
    "Content-Length: 378557\n"
    "X-Google-Google1: 314159265\n"
    "X-Google-Google2: aaaa2:7783,bbb21:9441\n"
    "X-Google-Google4: home\n"
    "Transfer-Encoding: chunked\n"
    "Set-Cookie: HEHE_AT=6666x66beef666x6-66xx6666x66; Path=/mail\n"
    "Set-Cookie: HEHE_HELP=owned:0;Path=/\n"
    "Set-Cookie: S=gmail=Xxx-beefbeefbeef_beefb:gmail_yj=beefbeef000beefbee"
       "fbee:gmproxy=bee-fbeefbe; Domain=.google.com; Path=/\n"
    "X-Google-Google2: /one/two/three/four/five/six/seven-height/nine:9411\n"
    "Server: GFE/1.3\n"
    "Transfer-Encoding: chunked\n"
    "Date: Mon, 13 Nov 2006 21:38:09 GMT\n"
    "Expires: Tue, 14 Nov 2006 19:23:58 GMT\n"
    "X-Malformed: bla; arg=test\"\n"
    "X-Malformed2: bla; arg=\n"
    "X-Test: bla; arg1=val1; arg2=val2";

TEST(NetUtilTest, GetSpecificHeader) {
  const HeaderCase tests[] = {
    {"content-type", "text/html; charset=utf-8"},
    {"CONTENT-LENGTH", "378557"},
    {"Date", "Mon, 13 Nov 2006 21:38:09 GMT"},
    {"Bad-Header", ""},
    {"", ""},
  };

  // Test first with google_headers.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string result =
        GetSpecificHeader(google_headers, tests[i].header_name);
    EXPECT_EQ(result, tests[i].expected);
  }

  // Test again with empty headers.
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string result = GetSpecificHeader(std::string(), tests[i].header_name);
    EXPECT_EQ(result, std::string());
  }
}

TEST(NetUtilTest, IDNToUnicodeFast) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(idn_cases); i++) {
    for (size_t j = 0; j < arraysize(kLanguages); j++) {
      // ja || zh-TW,en || ko,ja -> IDNToUnicodeSlow
      if (j == 3 || j == 17 || j == 18)
        continue;
      base::string16 output(IDNToUnicode(idn_cases[i].input, kLanguages[j]));
      base::string16 expected(idn_cases[i].unicode_allowed[j] ?
          WideToUTF16(idn_cases[i].unicode_output) :
          ASCIIToUTF16(idn_cases[i].input));
      AppendLanguagesToOutputs(kLanguages[j], &expected, &output);
      EXPECT_EQ(expected, output);
    }
  }
}

TEST(NetUtilTest, IDNToUnicodeSlow) {
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(idn_cases); i++) {
    for (size_t j = 0; j < arraysize(kLanguages); j++) {
      // !(ja || zh-TW,en || ko,ja) -> IDNToUnicodeFast
      if (!(j == 3 || j == 17 || j == 18))
        continue;
      base::string16 output(IDNToUnicode(idn_cases[i].input, kLanguages[j]));
      base::string16 expected(idn_cases[i].unicode_allowed[j] ?
          WideToUTF16(idn_cases[i].unicode_output) :
          ASCIIToUTF16(idn_cases[i].input));
      AppendLanguagesToOutputs(kLanguages[j], &expected, &output);
      EXPECT_EQ(expected, output);
    }
  }
}

TEST(NetUtilTest, CompliantHost) {
  const CompliantHostCase compliant_host_cases[] = {
    {"", "", false},
    {"a", "", true},
    {"-", "", false},
    {".", "", false},
    {"9", "", true},
    {"9a", "", true},
    {"a.", "", true},
    {"a.a", "", true},
    {"9.a", "", true},
    {"a.9", "", true},
    {"_9a", "", false},
    {"-9a", "", false},
    {"-9a", "a", true},
    {"a.a9", "", true},
    {"a.-a9", "", false},
    {"a+9a", "", false},
    {"-a.a9", "", true},
    {"1-.a-b", "", true},
    {"1_.a-b", "", false},
    {"1-2.a_b", "", true},
    {"a.b.c.d.e", "", true},
    {"1.2.3.4.5", "", true},
    {"1.2.3.4.5.", "", true},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(compliant_host_cases); ++i) {
    EXPECT_EQ(compliant_host_cases[i].expected_output,
        IsCanonicalizedHostCompliant(compliant_host_cases[i].host,
                                     compliant_host_cases[i].desired_tld));
  }
}

TEST(NetUtilTest, StripWWW) {
  EXPECT_EQ(base::string16(), StripWWW(base::string16()));
  EXPECT_EQ(base::string16(), StripWWW(ASCIIToUTF16("www.")));
  EXPECT_EQ(ASCIIToUTF16("blah"), StripWWW(ASCIIToUTF16("www.blah")));
  EXPECT_EQ(ASCIIToUTF16("blah"), StripWWW(ASCIIToUTF16("blah")));
}

// This is currently a windows specific function.
#if defined(OS_WIN)
namespace {

struct GetDirectoryListingEntryCase {
  const wchar_t* name;
  const char* raw_bytes;
  bool is_dir;
  int64 filesize;
  base::Time time;
  const char* expected;
};

}  // namespace
TEST(NetUtilTest, GetDirectoryListingEntry) {
  const GetDirectoryListingEntryCase test_cases[] = {
    {L"Foo",
     "",
     false,
     10000,
     base::Time(),
     "<script>addRow(\"Foo\",\"Foo\",0,\"9.8 kB\",\"\");</script>\n"},
    {L"quo\"tes",
     "",
     false,
     10000,
     base::Time(),
     "<script>addRow(\"quo\\\"tes\",\"quo%22tes\",0,\"9.8 kB\",\"\");</script>"
         "\n"},
    {L"quo\"tes",
     "quo\"tes",
     false,
     10000,
     base::Time(),
     "<script>addRow(\"quo\\\"tes\",\"quo%22tes\",0,\"9.8 kB\",\"\");</script>"
         "\n"},
    // U+D55C0 U+AE00. raw_bytes is empty (either a local file with
    // UTF-8/UTF-16 encoding or a remote file on an ftp server using UTF-8
    {L"\xD55C\xAE00.txt",
     "",
     false,
     10000,
     base::Time(),
     "<script>addRow(\"\xED\x95\x9C\xEA\xB8\x80.txt\","
         "\"%ED%95%9C%EA%B8%80.txt\",0,\"9.8 kB\",\"\");</script>\n"},
    // U+D55C0 U+AE00. raw_bytes is the corresponding EUC-KR sequence:
    // a local or remote file in EUC-KR.
    {L"\xD55C\xAE00.txt",
     "\xC7\xD1\xB1\xDB.txt",
     false,
     10000,
     base::Time(),
     "<script>addRow(\"\xED\x95\x9C\xEA\xB8\x80.txt\",\"%C7%D1%B1%DB.txt\""
         ",0,\"9.8 kB\",\"\");</script>\n"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); ++i) {
    const std::string results = GetDirectoryListingEntry(
        WideToUTF16(test_cases[i].name),
        test_cases[i].raw_bytes,
        test_cases[i].is_dir,
        test_cases[i].filesize,
        test_cases[i].time);
    EXPECT_EQ(test_cases[i].expected, results);
  }
}

#endif

TEST(NetUtilTest, ParseHostAndPort) {
  const struct {
    const char* input;
    bool success;
    const char* expected_host;
    int expected_port;
  } tests[] = {
    // Valid inputs:
    {"foo:10", true, "foo", 10},
    {"foo", true, "foo", -1},
    {
      "[1080:0:0:0:8:800:200C:4171]:11",
      true,
      "[1080:0:0:0:8:800:200C:4171]",
      11,
    },
    // Invalid inputs:
    {"foo:bar", false, "", -1},
    {"foo:", false, "", -1},
    {":", false, "", -1},
    {":80", false, "", -1},
    {"", false, "", -1},
    {"porttoolong:300000", false, "", -1},
    {"usrname@host", false, "", -1},
    {"usrname:password@host", false, "", -1},
    {":password@host", false, "", -1},
    {":password@host:80", false, "", -1},
    {":password@host", false, "", -1},
    {"@host", false, "", -1},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string host;
    int port;
    bool ok = ParseHostAndPort(tests[i].input, &host, &port);

    EXPECT_EQ(tests[i].success, ok);

    if (tests[i].success) {
      EXPECT_EQ(tests[i].expected_host, host);
      EXPECT_EQ(tests[i].expected_port, port);
    }
  }
}

TEST(NetUtilTest, GetHostAndPort) {
  const struct {
    GURL url;
    const char* expected_host_and_port;
  } tests[] = {
    { GURL("http://www.foo.com/x"), "www.foo.com:80"},
    { GURL("http://www.foo.com:21/x"), "www.foo.com:21"},

    // For IPv6 literals should always include the brackets.
    { GURL("http://[1::2]/x"), "[1::2]:80"},
    { GURL("http://[::a]:33/x"), "[::a]:33"},
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string host_and_port = GetHostAndPort(tests[i].url);
    EXPECT_EQ(std::string(tests[i].expected_host_and_port), host_and_port);
  }
}

TEST(NetUtilTest, GetHostAndOptionalPort) {
  const struct {
    GURL url;
    const char* expected_host_and_port;
  } tests[] = {
    { GURL("http://www.foo.com/x"), "www.foo.com"},
    { GURL("http://www.foo.com:21/x"), "www.foo.com:21"},

    // For IPv6 literals should always include the brackets.
    { GURL("http://[1::2]/x"), "[1::2]"},
    { GURL("http://[::a]:33/x"), "[::a]:33"},
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    std::string host_and_port = GetHostAndOptionalPort(tests[i].url);
    EXPECT_EQ(std::string(tests[i].expected_host_and_port), host_and_port);
  }
}

TEST(NetUtilTest, IPAddressToString) {
  uint8 addr1[4] = {0, 0, 0, 0};
  EXPECT_EQ("0.0.0.0", IPAddressToString(addr1, sizeof(addr1)));

  uint8 addr2[4] = {192, 168, 0, 1};
  EXPECT_EQ("192.168.0.1", IPAddressToString(addr2, sizeof(addr2)));

  uint8 addr3[16] = {0xFE, 0xDC, 0xBA, 0x98};
  EXPECT_EQ("fedc:ba98::", IPAddressToString(addr3, sizeof(addr3)));
}

TEST(NetUtilTest, IPAddressToStringWithPort) {
  uint8 addr1[4] = {0, 0, 0, 0};
  EXPECT_EQ("0.0.0.0:3", IPAddressToStringWithPort(addr1, sizeof(addr1), 3));

  uint8 addr2[4] = {192, 168, 0, 1};
  EXPECT_EQ("192.168.0.1:99",
            IPAddressToStringWithPort(addr2, sizeof(addr2), 99));

  uint8 addr3[16] = {0xFE, 0xDC, 0xBA, 0x98};
  EXPECT_EQ("[fedc:ba98::]:8080",
            IPAddressToStringWithPort(addr3, sizeof(addr3), 8080));
}

TEST(NetUtilTest, NetAddressToString_IPv4) {
  const struct {
    uint8 addr[4];
    const char* result;
  } tests[] = {
    {{0, 0, 0, 0}, "0.0.0.0"},
    {{127, 0, 0, 1}, "127.0.0.1"},
    {{192, 168, 0, 1}, "192.168.0.1"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    SockaddrStorage storage;
    MakeIPv4Address(tests[i].addr, 80, &storage);
    std::string result = NetAddressToString(storage.addr, storage.addr_len);
    EXPECT_EQ(std::string(tests[i].result), result);
  }
}

TEST(NetUtilTest, NetAddressToString_IPv6) {
  const struct {
    uint8 addr[16];
    const char* result;
  } tests[] = {
    {{0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10, 0xFE, 0xDC, 0xBA,
      0x98, 0x76, 0x54, 0x32, 0x10},
     "fedc:ba98:7654:3210:fedc:ba98:7654:3210"},
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    SockaddrStorage storage;
    MakeIPv6Address(tests[i].addr, 80, &storage);
    EXPECT_EQ(std::string(tests[i].result),
        NetAddressToString(storage.addr, storage.addr_len));
  }
}

TEST(NetUtilTest, NetAddressToStringWithPort_IPv4) {
  uint8 addr[] = {127, 0, 0, 1};
  SockaddrStorage storage;
  MakeIPv4Address(addr, 166, &storage);
  std::string result = NetAddressToStringWithPort(storage.addr,
                                                  storage.addr_len);
  EXPECT_EQ("127.0.0.1:166", result);
}

TEST(NetUtilTest, NetAddressToStringWithPort_IPv6) {
  uint8 addr[] = {
      0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10, 0xFE, 0xDC, 0xBA,
      0x98, 0x76, 0x54, 0x32, 0x10
  };
  SockaddrStorage storage;
  MakeIPv6Address(addr, 361, &storage);
  std::string result = NetAddressToStringWithPort(storage.addr,
                                                  storage.addr_len);

  // May fail on systems that don't support IPv6.
  if (!result.empty())
    EXPECT_EQ("[fedc:ba98:7654:3210:fedc:ba98:7654:3210]:361", result);
}

TEST(NetUtilTest, GetHostName) {
  // We can't check the result of GetHostName() directly, since the result
  // will differ across machines. Our goal here is to simply exercise the
  // code path, and check that things "look about right".
  std::string hostname = GetHostName();
  EXPECT_FALSE(hostname.empty());
}

TEST(NetUtilTest, FormatUrl) {
  FormatUrlTypes default_format_type = kFormatUrlOmitUsernamePassword;
  const UrlTestData tests[] = {
    {"Empty URL", "", "", default_format_type, UnescapeRule::NORMAL, L"", 0},

    {"Simple URL",
     "http://www.google.com/", "", default_format_type, UnescapeRule::NORMAL,
     L"http://www.google.com/", 7},

    {"With a port number and a reference",
     "http://www.google.com:8080/#\xE3\x82\xB0", "", default_format_type,
     UnescapeRule::NORMAL,
     L"http://www.google.com:8080/#\x30B0", 7},

    // -------- IDN tests --------
    {"Japanese IDN with ja",
     "http://xn--l8jvb1ey91xtjb.jp", "ja", default_format_type,
     UnescapeRule::NORMAL, L"http://\x671d\x65e5\x3042\x3055\x3072.jp/", 7},

    {"Japanese IDN with en",
     "http://xn--l8jvb1ey91xtjb.jp", "en", default_format_type,
     UnescapeRule::NORMAL, L"http://xn--l8jvb1ey91xtjb.jp/", 7},

    {"Japanese IDN without any languages",
     "http://xn--l8jvb1ey91xtjb.jp", "", default_format_type,
     UnescapeRule::NORMAL,
     // Single script is safe for empty languages.
     L"http://\x671d\x65e5\x3042\x3055\x3072.jp/", 7},

    {"mailto: with Japanese IDN",
     "mailto:foo@xn--l8jvb1ey91xtjb.jp", "ja", default_format_type,
     UnescapeRule::NORMAL,
     // GURL doesn't assume an email address's domain part as a host name.
     L"mailto:foo@xn--l8jvb1ey91xtjb.jp", 7},

    {"file: with Japanese IDN",
     "file://xn--l8jvb1ey91xtjb.jp/config.sys", "ja", default_format_type,
     UnescapeRule::NORMAL,
     L"file://\x671d\x65e5\x3042\x3055\x3072.jp/config.sys", 7},

    {"ftp: with Japanese IDN",
     "ftp://xn--l8jvb1ey91xtjb.jp/config.sys", "ja", default_format_type,
     UnescapeRule::NORMAL,
     L"ftp://\x671d\x65e5\x3042\x3055\x3072.jp/config.sys", 6},

    // -------- omit_username_password flag tests --------
    {"With username and password, omit_username_password=false",
     "http://user:passwd@example.com/foo", "",
     kFormatUrlOmitNothing, UnescapeRule::NORMAL,
     L"http://user:passwd@example.com/foo", 19},

    {"With username and password, omit_username_password=true",
     "http://user:passwd@example.com/foo", "", default_format_type,
     UnescapeRule::NORMAL, L"http://example.com/foo", 7},

    {"With username and no password",
     "http://user@example.com/foo", "", default_format_type,
     UnescapeRule::NORMAL, L"http://example.com/foo", 7},

    {"Just '@' without username and password",
     "http://@example.com/foo", "", default_format_type, UnescapeRule::NORMAL,
     L"http://example.com/foo", 7},

    // GURL doesn't think local-part of an email address is username for URL.
    {"mailto:, omit_username_password=true",
     "mailto:foo@example.com", "", default_format_type, UnescapeRule::NORMAL,
     L"mailto:foo@example.com", 7},

    // -------- unescape flag tests --------
    {"Do not unescape",
     "http://%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB.jp/"
     "%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB"
     "?q=%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB", "en", default_format_type,
     UnescapeRule::NONE,
     // GURL parses %-encoded hostnames into Punycode.
     L"http://xn--qcka1pmc.jp/%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB"
     L"?q=%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB", 7},

    {"Unescape normally",
     "http://%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB.jp/"
     "%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB"
     "?q=%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB", "en", default_format_type,
     UnescapeRule::NORMAL,
     L"http://xn--qcka1pmc.jp/\x30B0\x30FC\x30B0\x30EB"
     L"?q=\x30B0\x30FC\x30B0\x30EB", 7},

    {"Unescape normally with BiDi control character",
     "http://example.com/%E2%80%AEabc?q=%E2%80%8Fxy", "en", default_format_type,
     UnescapeRule::NORMAL, L"http://example.com/%E2%80%AEabc?q=%E2%80%8Fxy", 7},

    {"Unescape normally including unescape spaces",
     "http://www.google.com/search?q=Hello%20World", "en", default_format_type,
     UnescapeRule::SPACES, L"http://www.google.com/search?q=Hello World", 7},

    /*
    {"unescape=true with some special characters",
    "http://user%3A:%40passwd@example.com/foo%3Fbar?q=b%26z", "",
    kFormatUrlOmitNothing, UnescapeRule::NORMAL,
    L"http://user%3A:%40passwd@example.com/foo%3Fbar?q=b%26z", 25},
    */
    // Disabled: the resultant URL becomes "...user%253A:%2540passwd...".

    // -------- omit http: --------
    {"omit http with user name",
     "http://user@example.com/foo", "", kFormatUrlOmitAll,
     UnescapeRule::NORMAL, L"example.com/foo", 0},

    {"omit http",
     "http://www.google.com/", "en", kFormatUrlOmitHTTP,
     UnescapeRule::NORMAL, L"www.google.com/",
     0},

    {"omit http with https",
     "https://www.google.com/", "en", kFormatUrlOmitHTTP,
     UnescapeRule::NORMAL, L"https://www.google.com/",
     8},

    {"omit http starts with ftp.",
     "http://ftp.google.com/", "en", kFormatUrlOmitHTTP,
     UnescapeRule::NORMAL, L"http://ftp.google.com/",
     7},

    // -------- omit trailing slash on bare hostname --------
    {"omit slash when it's the entire path",
     "http://www.google.com/", "en",
     kFormatUrlOmitTrailingSlashOnBareHostname, UnescapeRule::NORMAL,
     L"http://www.google.com", 7},
    {"omit slash when there's a ref",
     "http://www.google.com/#ref", "en",
     kFormatUrlOmitTrailingSlashOnBareHostname, UnescapeRule::NORMAL,
     L"http://www.google.com/#ref", 7},
    {"omit slash when there's a query",
     "http://www.google.com/?", "en",
     kFormatUrlOmitTrailingSlashOnBareHostname, UnescapeRule::NORMAL,
     L"http://www.google.com/?", 7},
    {"omit slash when it's not the entire path",
     "http://www.google.com/foo", "en",
     kFormatUrlOmitTrailingSlashOnBareHostname, UnescapeRule::NORMAL,
     L"http://www.google.com/foo", 7},
    {"omit slash for nonstandard URLs",
     "data:/", "en", kFormatUrlOmitTrailingSlashOnBareHostname,
     UnescapeRule::NORMAL, L"data:/", 5},
    {"omit slash for file URLs",
     "file:///", "en", kFormatUrlOmitTrailingSlashOnBareHostname,
     UnescapeRule::NORMAL, L"file:///", 7},

    // -------- view-source: --------
    {"view-source",
     "view-source:http://xn--qcka1pmc.jp/", "ja", default_format_type,
     UnescapeRule::NORMAL, L"view-source:http://\x30B0\x30FC\x30B0\x30EB.jp/",
     19},

    {"view-source of view-source",
     "view-source:view-source:http://xn--qcka1pmc.jp/", "ja",
     default_format_type, UnescapeRule::NORMAL,
     L"view-source:view-source:http://xn--qcka1pmc.jp/", 12},

    // view-source should omit http and trailing slash where non-view-source
    // would.
    {"view-source omit http",
     "view-source:http://a.b/c", "en", kFormatUrlOmitAll,
     UnescapeRule::NORMAL, L"view-source:a.b/c",
     12},
    {"view-source omit http starts with ftp.",
     "view-source:http://ftp.b/c", "en", kFormatUrlOmitAll,
     UnescapeRule::NORMAL, L"view-source:http://ftp.b/c",
     19},
    {"view-source omit slash when it's the entire path",
     "view-source:http://a.b/", "en", kFormatUrlOmitAll,
     UnescapeRule::NORMAL, L"view-source:a.b",
     12},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    size_t prefix_len;
    base::string16 formatted = FormatUrl(
        GURL(tests[i].input), tests[i].languages, tests[i].format_types,
        tests[i].escape_rules, NULL, &prefix_len, NULL);
    EXPECT_EQ(WideToUTF16(tests[i].output), formatted) << tests[i].description;
    EXPECT_EQ(tests[i].prefix_len, prefix_len) << tests[i].description;
  }
}

TEST(NetUtilTest, FormatUrlParsed) {
  // No unescape case.
  url_parse::Parsed parsed;
  base::string16 formatted = FormatUrl(
      GURL("http://\xE3\x82\xB0:\xE3\x83\xBC@xn--qcka1pmc.jp:8080/"
           "%E3%82%B0/?q=%E3%82%B0#\xE3\x82\xB0"),
      "ja", kFormatUrlOmitNothing, UnescapeRule::NONE, &parsed, NULL,
      NULL);
  EXPECT_EQ(WideToUTF16(
      L"http://%E3%82%B0:%E3%83%BC@\x30B0\x30FC\x30B0\x30EB.jp:8080"
      L"/%E3%82%B0/?q=%E3%82%B0#\x30B0"), formatted);
  EXPECT_EQ(WideToUTF16(L"%E3%82%B0"),
      formatted.substr(parsed.username.begin, parsed.username.len));
  EXPECT_EQ(WideToUTF16(L"%E3%83%BC"),
      formatted.substr(parsed.password.begin, parsed.password.len));
  EXPECT_EQ(WideToUTF16(L"\x30B0\x30FC\x30B0\x30EB.jp"),
      formatted.substr(parsed.host.begin, parsed.host.len));
  EXPECT_EQ(WideToUTF16(L"8080"),
      formatted.substr(parsed.port.begin, parsed.port.len));
  EXPECT_EQ(WideToUTF16(L"/%E3%82%B0/"),
      formatted.substr(parsed.path.begin, parsed.path.len));
  EXPECT_EQ(WideToUTF16(L"q=%E3%82%B0"),
      formatted.substr(parsed.query.begin, parsed.query.len));
  EXPECT_EQ(WideToUTF16(L"\x30B0"),
      formatted.substr(parsed.ref.begin, parsed.ref.len));

  // Unescape case.
  formatted = FormatUrl(
      GURL("http://\xE3\x82\xB0:\xE3\x83\xBC@xn--qcka1pmc.jp:8080/"
           "%E3%82%B0/?q=%E3%82%B0#\xE3\x82\xB0"),
      "ja", kFormatUrlOmitNothing, UnescapeRule::NORMAL, &parsed, NULL,
      NULL);
  EXPECT_EQ(WideToUTF16(L"http://\x30B0:\x30FC@\x30B0\x30FC\x30B0\x30EB.jp:8080"
      L"/\x30B0/?q=\x30B0#\x30B0"), formatted);
  EXPECT_EQ(WideToUTF16(L"\x30B0"),
      formatted.substr(parsed.username.begin, parsed.username.len));
  EXPECT_EQ(WideToUTF16(L"\x30FC"),
      formatted.substr(parsed.password.begin, parsed.password.len));
  EXPECT_EQ(WideToUTF16(L"\x30B0\x30FC\x30B0\x30EB.jp"),
      formatted.substr(parsed.host.begin, parsed.host.len));
  EXPECT_EQ(WideToUTF16(L"8080"),
      formatted.substr(parsed.port.begin, parsed.port.len));
  EXPECT_EQ(WideToUTF16(L"/\x30B0/"),
      formatted.substr(parsed.path.begin, parsed.path.len));
  EXPECT_EQ(WideToUTF16(L"q=\x30B0"),
      formatted.substr(parsed.query.begin, parsed.query.len));
  EXPECT_EQ(WideToUTF16(L"\x30B0"),
      formatted.substr(parsed.ref.begin, parsed.ref.len));

  // Omit_username_password + unescape case.
  formatted = FormatUrl(
      GURL("http://\xE3\x82\xB0:\xE3\x83\xBC@xn--qcka1pmc.jp:8080/"
           "%E3%82%B0/?q=%E3%82%B0#\xE3\x82\xB0"),
      "ja", kFormatUrlOmitUsernamePassword, UnescapeRule::NORMAL, &parsed,
      NULL, NULL);
  EXPECT_EQ(WideToUTF16(L"http://\x30B0\x30FC\x30B0\x30EB.jp:8080"
      L"/\x30B0/?q=\x30B0#\x30B0"), formatted);
  EXPECT_FALSE(parsed.username.is_valid());
  EXPECT_FALSE(parsed.password.is_valid());
  EXPECT_EQ(WideToUTF16(L"\x30B0\x30FC\x30B0\x30EB.jp"),
      formatted.substr(parsed.host.begin, parsed.host.len));
  EXPECT_EQ(WideToUTF16(L"8080"),
      formatted.substr(parsed.port.begin, parsed.port.len));
  EXPECT_EQ(WideToUTF16(L"/\x30B0/"),
      formatted.substr(parsed.path.begin, parsed.path.len));
  EXPECT_EQ(WideToUTF16(L"q=\x30B0"),
      formatted.substr(parsed.query.begin, parsed.query.len));
  EXPECT_EQ(WideToUTF16(L"\x30B0"),
      formatted.substr(parsed.ref.begin, parsed.ref.len));

  // View-source case.
  formatted =
      FormatUrl(GURL("view-source:http://user:passwd@host:81/path?query#ref"),
                std::string(),
                kFormatUrlOmitUsernamePassword,
                UnescapeRule::NORMAL,
                &parsed,
                NULL,
                NULL);
  EXPECT_EQ(WideToUTF16(L"view-source:http://host:81/path?query#ref"),
      formatted);
  EXPECT_EQ(WideToUTF16(L"view-source:http"),
      formatted.substr(parsed.scheme.begin, parsed.scheme.len));
  EXPECT_FALSE(parsed.username.is_valid());
  EXPECT_FALSE(parsed.password.is_valid());
  EXPECT_EQ(WideToUTF16(L"host"),
      formatted.substr(parsed.host.begin, parsed.host.len));
  EXPECT_EQ(WideToUTF16(L"81"),
      formatted.substr(parsed.port.begin, parsed.port.len));
  EXPECT_EQ(WideToUTF16(L"/path"),
      formatted.substr(parsed.path.begin, parsed.path.len));
  EXPECT_EQ(WideToUTF16(L"query"),
      formatted.substr(parsed.query.begin, parsed.query.len));
  EXPECT_EQ(WideToUTF16(L"ref"),
      formatted.substr(parsed.ref.begin, parsed.ref.len));

  // omit http case.
  formatted = FormatUrl(GURL("http://host:8000/a?b=c#d"),
                        std::string(),
                        kFormatUrlOmitHTTP,
                        UnescapeRule::NORMAL,
                        &parsed,
                        NULL,
                        NULL);
  EXPECT_EQ(WideToUTF16(L"host:8000/a?b=c#d"), formatted);
  EXPECT_FALSE(parsed.scheme.is_valid());
  EXPECT_FALSE(parsed.username.is_valid());
  EXPECT_FALSE(parsed.password.is_valid());
  EXPECT_EQ(WideToUTF16(L"host"),
      formatted.substr(parsed.host.begin, parsed.host.len));
  EXPECT_EQ(WideToUTF16(L"8000"),
      formatted.substr(parsed.port.begin, parsed.port.len));
  EXPECT_EQ(WideToUTF16(L"/a"),
      formatted.substr(parsed.path.begin, parsed.path.len));
  EXPECT_EQ(WideToUTF16(L"b=c"),
      formatted.substr(parsed.query.begin, parsed.query.len));
  EXPECT_EQ(WideToUTF16(L"d"),
      formatted.substr(parsed.ref.begin, parsed.ref.len));

  // omit http starts with ftp case.
  formatted = FormatUrl(GURL("http://ftp.host:8000/a?b=c#d"),
                        std::string(),
                        kFormatUrlOmitHTTP,
                        UnescapeRule::NORMAL,
                        &parsed,
                        NULL,
                        NULL);
  EXPECT_EQ(WideToUTF16(L"http://ftp.host:8000/a?b=c#d"), formatted);
  EXPECT_TRUE(parsed.scheme.is_valid());
  EXPECT_FALSE(parsed.username.is_valid());
  EXPECT_FALSE(parsed.password.is_valid());
  EXPECT_EQ(WideToUTF16(L"http"),
      formatted.substr(parsed.scheme.begin, parsed.scheme.len));
  EXPECT_EQ(WideToUTF16(L"ftp.host"),
      formatted.substr(parsed.host.begin, parsed.host.len));
  EXPECT_EQ(WideToUTF16(L"8000"),
      formatted.substr(parsed.port.begin, parsed.port.len));
  EXPECT_EQ(WideToUTF16(L"/a"),
      formatted.substr(parsed.path.begin, parsed.path.len));
  EXPECT_EQ(WideToUTF16(L"b=c"),
      formatted.substr(parsed.query.begin, parsed.query.len));
  EXPECT_EQ(WideToUTF16(L"d"),
      formatted.substr(parsed.ref.begin, parsed.ref.len));

  // omit http starts with 'f' case.
  formatted = FormatUrl(GURL("http://f/"),
                        std::string(),
                        kFormatUrlOmitHTTP,
                        UnescapeRule::NORMAL,
                        &parsed,
                        NULL,
                        NULL);
  EXPECT_EQ(WideToUTF16(L"f/"), formatted);
  EXPECT_FALSE(parsed.scheme.is_valid());
  EXPECT_FALSE(parsed.username.is_valid());
  EXPECT_FALSE(parsed.password.is_valid());
  EXPECT_FALSE(parsed.port.is_valid());
  EXPECT_TRUE(parsed.path.is_valid());
  EXPECT_FALSE(parsed.query.is_valid());
  EXPECT_FALSE(parsed.ref.is_valid());
  EXPECT_EQ(WideToUTF16(L"f"),
      formatted.substr(parsed.host.begin, parsed.host.len));
  EXPECT_EQ(WideToUTF16(L"/"),
      formatted.substr(parsed.path.begin, parsed.path.len));
}

// Make sure that calling FormatUrl on a GURL and then converting back to a GURL
// results in the original GURL, for each ASCII character in the path.
TEST(NetUtilTest, FormatUrlRoundTripPathASCII) {
  for (unsigned char test_char = 32; test_char < 128; ++test_char) {
    GURL url(std::string("http://www.google.com/") +
             static_cast<char>(test_char));
    size_t prefix_len;
    base::string16 formatted = FormatUrl(url,
                                         std::string(),
                                         kFormatUrlOmitUsernamePassword,
                                         UnescapeRule::NORMAL,
                                         NULL,
                                         &prefix_len,
                                         NULL);
    EXPECT_EQ(url.spec(), GURL(formatted).spec());
  }
}

// Make sure that calling FormatUrl on a GURL and then converting back to a GURL
// results in the original GURL, for each escaped ASCII character in the path.
TEST(NetUtilTest, FormatUrlRoundTripPathEscaped) {
  for (unsigned char test_char = 32; test_char < 128; ++test_char) {
    std::string original_url("http://www.google.com/");
    original_url.push_back('%');
    original_url.append(base::HexEncode(&test_char, 1));

    GURL url(original_url);
    size_t prefix_len;
    base::string16 formatted = FormatUrl(url,
                                         std::string(),
                                         kFormatUrlOmitUsernamePassword,
                                         UnescapeRule::NORMAL,
                                         NULL,
                                         &prefix_len,
                                         NULL);
    EXPECT_EQ(url.spec(), GURL(formatted).spec());
  }
}

// Make sure that calling FormatUrl on a GURL and then converting back to a GURL
// results in the original GURL, for each ASCII character in the query.
TEST(NetUtilTest, FormatUrlRoundTripQueryASCII) {
  for (unsigned char test_char = 32; test_char < 128; ++test_char) {
    GURL url(std::string("http://www.google.com/?") +
             static_cast<char>(test_char));
    size_t prefix_len;
    base::string16 formatted = FormatUrl(url,
                                         std::string(),
                                         kFormatUrlOmitUsernamePassword,
                                         UnescapeRule::NORMAL,
                                         NULL,
                                         &prefix_len,
                                         NULL);
    EXPECT_EQ(url.spec(), GURL(formatted).spec());
  }
}

// Make sure that calling FormatUrl on a GURL and then converting back to a GURL
// only results in a different GURL for certain characters.
TEST(NetUtilTest, FormatUrlRoundTripQueryEscaped) {
  // A full list of characters which FormatURL should unescape and GURL should
  // not escape again, when they appear in a query string.
  const char* kUnescapedCharacters =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_~";
  for (unsigned char test_char = 0; test_char < 128; ++test_char) {
    std::string original_url("http://www.google.com/?");
    original_url.push_back('%');
    original_url.append(base::HexEncode(&test_char, 1));

    GURL url(original_url);
    size_t prefix_len;
    base::string16 formatted = FormatUrl(url,
                                         std::string(),
                                         kFormatUrlOmitUsernamePassword,
                                         UnescapeRule::NORMAL,
                                         NULL,
                                         &prefix_len,
                                         NULL);

    if (test_char &&
        strchr(kUnescapedCharacters, static_cast<char>(test_char))) {
      EXPECT_NE(url.spec(), GURL(formatted).spec());
    } else {
      EXPECT_EQ(url.spec(), GURL(formatted).spec());
    }
  }
}

TEST(NetUtilTest, FormatUrlWithOffsets) {
  CheckAdjustedOffsets(std::string(), "en", kFormatUrlOmitNothing,
                       UnescapeRule::NORMAL, NULL);

  const size_t basic_offsets[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25
  };
  CheckAdjustedOffsets("http://www.google.com/foo/", "en",
                       kFormatUrlOmitNothing, UnescapeRule::NORMAL,
                       basic_offsets);

  const size_t omit_auth_offsets_1[] = {
    0, 1, 2, 3, 4, 5, 6, 7, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, 7,
    8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21
  };
  CheckAdjustedOffsets("http://foo:bar@www.google.com/", "en",
                       kFormatUrlOmitUsernamePassword, UnescapeRule::NORMAL,
                       omit_auth_offsets_1);

  const size_t omit_auth_offsets_2[] = {
    0, 1, 2, 3, 4, 5, 6, 7, kNpos, kNpos, kNpos, 7, 8, 9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21
  };
  CheckAdjustedOffsets("http://foo@www.google.com/", "en",
                       kFormatUrlOmitUsernamePassword, UnescapeRule::NORMAL,
                       omit_auth_offsets_2);

  const size_t dont_omit_auth_offsets[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos,
    kNpos, kNpos, 11, 12, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos,
    kNpos, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    30, 31
  };
  // Unescape to "http://foo\x30B0:\x30B0bar@www.google.com".
  CheckAdjustedOffsets("http://foo%E3%82%B0:%E3%82%B0bar@www.google.com/", "en",
                       kFormatUrlOmitNothing, UnescapeRule::NORMAL,
                       dont_omit_auth_offsets);

  const size_t view_source_offsets[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, kNpos,
    kNpos, kNpos, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33
  };
  CheckAdjustedOffsets("view-source:http://foo@www.google.com/", "en",
                       kFormatUrlOmitUsernamePassword, UnescapeRule::NORMAL,
                       view_source_offsets);

  const size_t idn_hostname_offsets_1[] = {
    0, 1, 2, 3, 4, 5, 6, 7, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos,
    kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, 12,
    13, 14, 15, 16, 17, 18, 19
  };
  // Convert punycode to "http://\x671d\x65e5\x3042\x3055\x3072.jp/foo/".
  CheckAdjustedOffsets("http://xn--l8jvb1ey91xtjb.jp/foo/", "ja",
                       kFormatUrlOmitNothing, UnescapeRule::NORMAL,
                       idn_hostname_offsets_1);

  const size_t idn_hostname_offsets_2[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, kNpos, kNpos, kNpos, kNpos, kNpos,
    kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, 14, 15, kNpos, kNpos, kNpos,
    kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos,
    kNpos, 19, 20, 21, 22, 23, 24
  };
  // Convert punycode to
  // "http://test.\x89c6\x9891.\x5317\x4eac\x5927\x5b78.test/".
  CheckAdjustedOffsets("http://test.xn--cy2a840a.xn--1lq90ic7f1rc.test/",
                       "zh-CN", kFormatUrlOmitNothing, UnescapeRule::NORMAL,
                       idn_hostname_offsets_2);

  const size_t unescape_offsets[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, kNpos, kNpos, 26, 27, 28, 29, 30, kNpos, kNpos, kNpos,
    kNpos, kNpos, kNpos, kNpos, kNpos, 31, kNpos, kNpos, kNpos, kNpos, kNpos,
    kNpos, kNpos, kNpos, 32, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos,
    kNpos, 33, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos
  };
  // Unescape to "http://www.google.com/foo bar/\x30B0\x30FC\x30B0\x30EB".
  CheckAdjustedOffsets(
      "http://www.google.com/foo%20bar/%E3%82%B0%E3%83%BC%E3%82%B0%E3%83%AB",
      "en", kFormatUrlOmitNothing, UnescapeRule::SPACES, unescape_offsets);

  const size_t ref_offsets[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
    21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, kNpos, kNpos, 32, kNpos, kNpos,
    33
  };
  // Unescape to "http://www.google.com/foo.html#\x30B0\x30B0z".
  CheckAdjustedOffsets(
      "http://www.google.com/foo.html#\xE3\x82\xB0\xE3\x82\xB0z", "en",
      kFormatUrlOmitNothing, UnescapeRule::NORMAL, ref_offsets);

  const size_t omit_http_offsets[] = {
    0, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    10, 11, 12, 13, 14
  };
  CheckAdjustedOffsets("http://www.google.com/", "en", kFormatUrlOmitHTTP,
                       UnescapeRule::NORMAL, omit_http_offsets);

  const size_t omit_http_start_with_ftp_offsets[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21
  };
  CheckAdjustedOffsets("http://ftp.google.com/", "en", kFormatUrlOmitHTTP,
                       UnescapeRule::NORMAL, omit_http_start_with_ftp_offsets);

  const size_t omit_all_offsets[] = {
    0, kNpos, kNpos, kNpos, kNpos, kNpos, kNpos, 0, kNpos, kNpos, kNpos, kNpos,
    0, 1, 2, 3, 4, 5, 6, 7
  };
  CheckAdjustedOffsets("http://user@foo.com/", "en", kFormatUrlOmitAll,
                       UnescapeRule::NORMAL, omit_all_offsets);
}

TEST(NetUtilTest, SimplifyUrlForRequest) {
  struct {
    const char* input_url;
    const char* expected_simplified_url;
  } tests[] = {
    {
      // Reference section should be stripped.
      "http://www.google.com:78/foobar?query=1#hash",
      "http://www.google.com:78/foobar?query=1",
    },
    {
      // Reference section can itself contain #.
      "http://192.168.0.1?query=1#hash#10#11#13#14",
      "http://192.168.0.1?query=1",
    },
    { // Strip username/password.
      "http://user:pass@google.com",
      "http://google.com/",
    },
    { // Strip both the reference and the username/password.
      "http://user:pass@google.com:80/sup?yo#X#X",
      "http://google.com/sup?yo",
    },
    { // Try an HTTPS URL -- strip both the reference and the username/password.
      "https://user:pass@google.com:80/sup?yo#X#X",
      "https://google.com:80/sup?yo",
    },
    { // Try an FTP URL -- strip both the reference and the username/password.
      "ftp://user:pass@google.com:80/sup?yo#X#X",
      "ftp://google.com:80/sup?yo",
    },
    { // Try a nonstandard URL
      "foobar://user:pass@google.com:80/sup?yo#X#X",
      "foobar://user:pass@google.com:80/sup?yo",
    },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s", i,
                                    tests[i].input_url));
    GURL input_url(GURL(tests[i].input_url));
    GURL expected_url(GURL(tests[i].expected_simplified_url));
    EXPECT_EQ(expected_url, SimplifyUrlForRequest(input_url));
  }
}

TEST(NetUtilTest, SetExplicitlyAllowedPortsTest) {
  std::string invalid[] = { "1,2,a", "'1','2'", "1, 2, 3", "1 0,11,12" };
  std::string valid[] = { "", "1", "1,2", "1,2,3", "10,11,12,13" };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(invalid); ++i) {
    SetExplicitlyAllowedPorts(invalid[i]);
    EXPECT_EQ(0, static_cast<int>(GetCountOfExplicitlyAllowedPorts()));
  }

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(valid); ++i) {
    SetExplicitlyAllowedPorts(valid[i]);
    EXPECT_EQ(i, GetCountOfExplicitlyAllowedPorts());
  }
}

TEST(NetUtilTest, GetHostOrSpecFromURL) {
  EXPECT_EQ("example.com",
            GetHostOrSpecFromURL(GURL("http://example.com/test")));
  EXPECT_EQ("example.com",
            GetHostOrSpecFromURL(GURL("http://example.com./test")));
  EXPECT_EQ("file:///tmp/test.html",
            GetHostOrSpecFromURL(GURL("file:///tmp/test.html")));
}

TEST(NetUtilTest, GetAddressFamily) {
  IPAddressNumber number;
  EXPECT_TRUE(ParseIPLiteralToNumber("192.168.0.1", &number));
  EXPECT_EQ(ADDRESS_FAMILY_IPV4, GetAddressFamily(number));
  EXPECT_TRUE(ParseIPLiteralToNumber("1:abcd::3:4:ff", &number));
  EXPECT_EQ(ADDRESS_FAMILY_IPV6, GetAddressFamily(number));
}

// Test that invalid IP literals fail to parse.
TEST(NetUtilTest, ParseIPLiteralToNumber_FailParse) {
  IPAddressNumber number;

  EXPECT_FALSE(ParseIPLiteralToNumber("bad value", &number));
  EXPECT_FALSE(ParseIPLiteralToNumber("bad:value", &number));
  EXPECT_FALSE(ParseIPLiteralToNumber(std::string(), &number));
  EXPECT_FALSE(ParseIPLiteralToNumber("192.168.0.1:30", &number));
  EXPECT_FALSE(ParseIPLiteralToNumber("  192.168.0.1  ", &number));
  EXPECT_FALSE(ParseIPLiteralToNumber("[::1]", &number));
}

// Test parsing an IPv4 literal.
TEST(NetUtilTest, ParseIPLiteralToNumber_IPv4) {
  IPAddressNumber number;
  EXPECT_TRUE(ParseIPLiteralToNumber("192.168.0.1", &number));
  EXPECT_EQ("192,168,0,1", DumpIPNumber(number));
  EXPECT_EQ("192.168.0.1", IPAddressToString(number));
}

// Test parsing an IPv6 literal.
TEST(NetUtilTest, ParseIPLiteralToNumber_IPv6) {
  IPAddressNumber number;
  EXPECT_TRUE(ParseIPLiteralToNumber("1:abcd::3:4:ff", &number));
  EXPECT_EQ("0,1,171,205,0,0,0,0,0,0,0,3,0,4,0,255", DumpIPNumber(number));
  EXPECT_EQ("1:abcd::3:4:ff", IPAddressToString(number));
}

// Test mapping an IPv4 address to an IPv6 address.
TEST(NetUtilTest, ConvertIPv4NumberToIPv6Number) {
  IPAddressNumber ipv4_number;
  EXPECT_TRUE(ParseIPLiteralToNumber("192.168.0.1", &ipv4_number));

  IPAddressNumber ipv6_number =
      ConvertIPv4NumberToIPv6Number(ipv4_number);

  // ::ffff:192.168.0.1
  EXPECT_EQ("0,0,0,0,0,0,0,0,0,0,255,255,192,168,0,1",
            DumpIPNumber(ipv6_number));
  EXPECT_EQ("::ffff:c0a8:1", IPAddressToString(ipv6_number));
}

TEST(NetUtilTest, IsIPv4Mapped) {
  IPAddressNumber ipv4_number;
  EXPECT_TRUE(ParseIPLiteralToNumber("192.168.0.1", &ipv4_number));
  EXPECT_FALSE(IsIPv4Mapped(ipv4_number));

  IPAddressNumber ipv6_number;
  EXPECT_TRUE(ParseIPLiteralToNumber("::1", &ipv4_number));
  EXPECT_FALSE(IsIPv4Mapped(ipv6_number));

  IPAddressNumber ipv4mapped_number;
  EXPECT_TRUE(ParseIPLiteralToNumber("::ffff:0101:1", &ipv4mapped_number));
  EXPECT_TRUE(IsIPv4Mapped(ipv4mapped_number));
}

TEST(NetUtilTest, ConvertIPv4MappedToIPv4) {
  IPAddressNumber ipv4mapped_number;
  EXPECT_TRUE(ParseIPLiteralToNumber("::ffff:0101:1", &ipv4mapped_number));
  IPAddressNumber expected;
  EXPECT_TRUE(ParseIPLiteralToNumber("1.1.0.1", &expected));
  IPAddressNumber result = ConvertIPv4MappedToIPv4(ipv4mapped_number);
  EXPECT_EQ(expected, result);
}

// Test parsing invalid CIDR notation literals.
TEST(NetUtilTest, ParseCIDRBlock_Invalid) {
  const char* bad_literals[] = {
      "foobar",
      "",
      "192.168.0.1",
      "::1",
      "/",
      "/1",
      "1",
      "192.168.1.1/-1",
      "192.168.1.1/33",
      "::1/-3",
      "a::3/129",
      "::1/x",
      "192.168.0.1//11"
  };

  for (size_t i = 0; i < arraysize(bad_literals); ++i) {
    IPAddressNumber ip_number;
    size_t prefix_length_in_bits;

    EXPECT_FALSE(ParseCIDRBlock(bad_literals[i],
                                     &ip_number,
                                     &prefix_length_in_bits));
  }
}

// Test parsing a valid CIDR notation literal.
TEST(NetUtilTest, ParseCIDRBlock_Valid) {
  IPAddressNumber ip_number;
  size_t prefix_length_in_bits;

  EXPECT_TRUE(ParseCIDRBlock("192.168.0.1/11",
                                  &ip_number,
                                  &prefix_length_in_bits));

  EXPECT_EQ("192,168,0,1", DumpIPNumber(ip_number));
  EXPECT_EQ(11u, prefix_length_in_bits);
}

TEST(NetUtilTest, IPNumberMatchesPrefix) {
  struct {
    const char* cidr_literal;
    const char* ip_literal;
    bool expected_to_match;
  } tests[] = {
    // IPv4 prefix with IPv4 inputs.
    {
      "10.10.1.32/27",
      "10.10.1.44",
      true
    },
    {
      "10.10.1.32/27",
      "10.10.1.90",
      false
    },
    {
      "10.10.1.32/27",
      "10.10.1.90",
      false
    },

    // IPv6 prefix with IPv6 inputs.
    {
      "2001:db8::/32",
      "2001:DB8:3:4::5",
      true
    },
    {
      "2001:db8::/32",
      "2001:c8::",
      false
    },

    // IPv6 prefix with IPv4 inputs.
    {
      "2001:db8::/33",
      "192.168.0.1",
      false
    },
    {
      "::ffff:192.168.0.1/112",
      "192.168.33.77",
      true
    },

    // IPv4 prefix with IPv6 inputs.
    {
      "10.11.33.44/16",
      "::ffff:0a0b:89",
      true
    },
    {
      "10.11.33.44/16",
      "::ffff:10.12.33.44",
      false
    },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s, %s", i,
                                    tests[i].cidr_literal,
                                    tests[i].ip_literal));

    IPAddressNumber ip_number;
    EXPECT_TRUE(ParseIPLiteralToNumber(tests[i].ip_literal, &ip_number));

    IPAddressNumber ip_prefix;
    size_t prefix_length_in_bits;

    EXPECT_TRUE(ParseCIDRBlock(tests[i].cidr_literal,
                               &ip_prefix,
                               &prefix_length_in_bits));

    EXPECT_EQ(tests[i].expected_to_match,
              IPNumberMatchesPrefix(ip_number,
                                    ip_prefix,
                                    prefix_length_in_bits));
  }
}

TEST(NetUtilTest, IsLocalhost) {
  EXPECT_TRUE(net::IsLocalhost("localhost"));
  EXPECT_TRUE(net::IsLocalhost("localhost.localdomain"));
  EXPECT_TRUE(net::IsLocalhost("localhost6"));
  EXPECT_TRUE(net::IsLocalhost("localhost6.localdomain6"));
  EXPECT_TRUE(net::IsLocalhost("127.0.0.1"));
  EXPECT_TRUE(net::IsLocalhost("127.0.1.0"));
  EXPECT_TRUE(net::IsLocalhost("127.1.0.0"));
  EXPECT_TRUE(net::IsLocalhost("127.0.0.255"));
  EXPECT_TRUE(net::IsLocalhost("127.0.255.0"));
  EXPECT_TRUE(net::IsLocalhost("127.255.0.0"));
  EXPECT_TRUE(net::IsLocalhost("::1"));
  EXPECT_TRUE(net::IsLocalhost("0:0:0:0:0:0:0:1"));

  EXPECT_FALSE(net::IsLocalhost("localhostx"));
  EXPECT_FALSE(net::IsLocalhost("foo.localdomain"));
  EXPECT_FALSE(net::IsLocalhost("localhost6x"));
  EXPECT_FALSE(net::IsLocalhost("localhost.localdomain6"));
  EXPECT_FALSE(net::IsLocalhost("localhost6.localdomain"));
  EXPECT_FALSE(net::IsLocalhost("127.0.0.1.1"));
  EXPECT_FALSE(net::IsLocalhost(".127.0.0.255"));
  EXPECT_FALSE(net::IsLocalhost("::2"));
  EXPECT_FALSE(net::IsLocalhost("::1:1"));
  EXPECT_FALSE(net::IsLocalhost("0:0:0:0:1:0:0:1"));
  EXPECT_FALSE(net::IsLocalhost("::1:1"));
  EXPECT_FALSE(net::IsLocalhost("0:0:0:0:0:0:0:0:1"));
}

// Verify GetNetworkList().
TEST(NetUtilTest, GetNetworkList) {
  NetworkInterfaceList list;
  ASSERT_TRUE(GetNetworkList(&list, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES));
  for (NetworkInterfaceList::iterator it = list.begin();
       it != list.end(); ++it) {
    // Verify that the names are not empty.
    EXPECT_FALSE(it->name.empty());
    EXPECT_FALSE(it->friendly_name.empty());

    // Verify that the address is correct.
    EXPECT_TRUE(it->address.size() == kIPv4AddressSize ||
                it->address.size() == kIPv6AddressSize)
        << "Invalid address of size " << it->address.size();
    bool all_zeroes = true;
    for (size_t i = 0; i < it->address.size(); ++i) {
      if (it->address[i] != 0) {
        all_zeroes = false;
        break;
      }
    }
    EXPECT_FALSE(all_zeroes);
    EXPECT_GT(it->network_prefix, 1u);
    EXPECT_LE(it->network_prefix, it->address.size() * 8);

#if defined(OS_WIN)
    // On Windows |name| is NET_LUID.
    base::ScopedNativeLibrary phlpapi_lib(
        base::FilePath(FILE_PATH_LITERAL("iphlpapi.dll")));
    ASSERT_TRUE(phlpapi_lib.is_valid());
    typedef NETIO_STATUS (WINAPI* ConvertInterfaceIndexToLuid)(NET_IFINDEX,
                                                               PNET_LUID);
    ConvertInterfaceIndexToLuid interface_to_luid =
        reinterpret_cast<ConvertInterfaceIndexToLuid>(
            phlpapi_lib.GetFunctionPointer("ConvertInterfaceIndexToLuid"));

    typedef NETIO_STATUS (WINAPI* ConvertInterfaceLuidToGuid)(NET_LUID*,
                                                              GUID*);
    ConvertInterfaceLuidToGuid luid_to_guid =
        reinterpret_cast<ConvertInterfaceLuidToGuid>(
            phlpapi_lib.GetFunctionPointer("ConvertInterfaceLuidToGuid"));

    if (interface_to_luid && luid_to_guid) {
      NET_LUID luid;
      EXPECT_EQ(interface_to_luid(it->interface_index, &luid), NO_ERROR);
      GUID guid;
      EXPECT_EQ(luid_to_guid(&luid, &guid), NO_ERROR);
      LPOLESTR name;
      StringFromCLSID(guid, &name);
      EXPECT_STREQ(base::UTF8ToWide(it->name).c_str(), name);
      CoTaskMemFree(name);
      continue;
    } else {
      EXPECT_LT(base::win::GetVersion(), base::win::VERSION_VISTA);
      EXPECT_LT(it->interface_index, 1u << 24u);  // Must fit 0.x.x.x.
      EXPECT_NE(it->interface_index, 0u);  // 0 means to use default.
    }
#elif !defined(OS_ANDROID)
    char name[IF_NAMESIZE];
    EXPECT_TRUE(if_indextoname(it->interface_index, name));
    EXPECT_STREQ(it->name.c_str(), name);
#endif
  }
}

struct NonUniqueNameTestData {
  bool is_unique;
  const char* hostname;
};

// Google Test pretty-printer.
void PrintTo(const NonUniqueNameTestData& data, std::ostream* os) {
  ASSERT_TRUE(data.hostname);
  *os << " hostname: " << testing::PrintToString(data.hostname)
      << "; is_unique: " << testing::PrintToString(data.is_unique);
}

const NonUniqueNameTestData kNonUniqueNameTestData[] = {
    // Domains under ICANN-assigned domains.
    { true, "google.com" },
    { true, "google.co.uk" },
    // Domains under private registries.
    { true, "appspot.com" },
    { true, "test.appspot.com" },
    // Unreserved IPv4 addresses (in various forms).
    { true, "8.8.8.8" },
    { true, "99.64.0.0" },
    { true, "212.15.0.0" },
    { true, "212.15" },
    { true, "212.15.0" },
    { true, "3557752832" },
    // Reserved IPv4 addresses (in various forms).
    { false, "192.168.0.0" },
    { false, "192.168.0.6" },
    { false, "10.0.0.5" },
    { false, "10.0" },
    { false, "10.0.0" },
    { false, "3232235526" },
    // Unreserved IPv6 addresses.
    { true, "FFC0:ba98:7654:3210:FEDC:BA98:7654:3210" },
    { true, "2000:ba98:7654:2301:EFCD:BA98:7654:3210" },
    // Reserved IPv6 addresses.
    { false, "::192.9.5.5" },
    { false, "FEED::BEEF" },
    { false, "FEC0:ba98:7654:3210:FEDC:BA98:7654:3210" },
    // 'internal'/non-IANA assigned domains.
    { false, "intranet" },
    { false, "intranet." },
    { false, "intranet.example" },
    { false, "host.intranet.example" },
    // gTLDs under discussion, but not yet assigned.
    { false, "intranet.corp" },
    { false, "example.tech" },
    { false, "intranet.internal" },
    // Invalid host names are treated as unique - but expected to be
    // filtered out before then.
    { true, "junk)(£)$*!@~#" },
    { true, "w$w.example.com" },
    { true, "nocolonsallowed:example" },
    { true, "[::4.5.6.9]" },
};

class NetUtilNonUniqueNameTest
    : public testing::TestWithParam<NonUniqueNameTestData> {
 public:
  virtual ~NetUtilNonUniqueNameTest() {}

 protected:
  bool IsUnique(const std::string& hostname) {
    return !IsHostnameNonUnique(hostname);
  }
};

// Test that internal/non-unique names are properly identified as such, but
// that IP addresses and hosts beneath registry-controlled domains are flagged
// as unique names.
TEST_P(NetUtilNonUniqueNameTest, IsHostnameNonUnique) {
  const NonUniqueNameTestData& test_data = GetParam();

  EXPECT_EQ(test_data.is_unique, IsUnique(test_data.hostname));
}

INSTANTIATE_TEST_CASE_P(, NetUtilNonUniqueNameTest,
                        testing::ValuesIn(kNonUniqueNameTestData));

}  // namespace net
