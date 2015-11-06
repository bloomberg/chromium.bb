// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/net_util.h"

#include <string.h>

#include <ostream>

#include "base/files/file_path.h"
#include "base/format_macros.h"
#include "base/scoped_native_library.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/sys_byteorder.h"
#include "base/time/time.h"
#include "net/base/address_list.h"
#include "net/base/ip_endpoint.h"

#if !defined(OS_NACL) && !defined(OS_WIN)
#include <net/if.h>
#include <netinet/in.h>
#if defined(OS_MACOSX)
#include <ifaddrs.h>
#if !defined(OS_IOS)
#include <netinet/in_var.h>
#endif  // !OS_IOS
#endif  // OS_MACOSX
#endif  // !OS_NACL && !OS_WIN
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include <iphlpapi.h>
#include <objbase.h>
#include "base/win/windows_version.h"
#endif  // OS_WIN

#if !defined(OS_MACOSX) && !defined(OS_NACL) && !defined(OS_WIN)
#include "net/base/address_tracker_linux.h"
#endif  // !OS_MACOSX && !OS_NACL && !OS_WIN

using base::ASCIIToUTF16;
using base::WideToUTF16;

namespace net {

namespace {

struct HeaderCase {
  const char* const header_name;
  const char* const expected;
};

const unsigned char kLocalhostIPv4[] = {127, 0, 0, 1};
const unsigned char kLocalhostIPv6[] =
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};
const uint16_t kLocalhostLookupPort = 80;

// Fills in sockaddr for the given 32-bit address (IPv4.)
// |bytes| should be an array of length 4.
void MakeIPv4Address(const uint8_t* bytes, int port, SockaddrStorage* storage) {
  memset(&storage->addr_storage, 0, sizeof(storage->addr_storage));
  storage->addr_len = sizeof(struct sockaddr_in);
  struct sockaddr_in* addr4 = reinterpret_cast<sockaddr_in*>(storage->addr);
  addr4->sin_port = base::HostToNet16(port);
  addr4->sin_family = AF_INET;
  memcpy(&addr4->sin_addr, bytes, 4);
}

// Fills in sockaddr for the given 128-bit address (IPv6.)
// |bytes| should be an array of length 16.
void MakeIPv6Address(const uint8_t* bytes, int port, SockaddrStorage* storage) {
  memset(&storage->addr_storage, 0, sizeof(storage->addr_storage));
  storage->addr_len = sizeof(struct sockaddr_in6);
  struct sockaddr_in6* addr6 = reinterpret_cast<sockaddr_in6*>(storage->addr);
  addr6->sin6_port = base::HostToNet16(port);
  addr6->sin6_family = AF_INET6;
  memcpy(&addr6->sin6_addr, bytes, 16);
}

bool HasEndpoint(const IPEndPoint& endpoint, const AddressList& addresses) {
  for (const auto& address : addresses) {
    if (endpoint == address)
      return true;
  }
  return false;
}

void TestBothLoopbackIPs(const std::string& host) {
  IPEndPoint localhost_ipv4(
      IPAddressNumber(kLocalhostIPv4,
                      kLocalhostIPv4 + arraysize(kLocalhostIPv4)),
      kLocalhostLookupPort);
  IPEndPoint localhost_ipv6(
      IPAddressNumber(kLocalhostIPv6,
                      kLocalhostIPv6 + arraysize(kLocalhostIPv6)),
      kLocalhostLookupPort);

  AddressList addresses;
  EXPECT_TRUE(ResolveLocalHostname(host, kLocalhostLookupPort, &addresses));
  EXPECT_EQ(2u, addresses.size());
  EXPECT_TRUE(HasEndpoint(localhost_ipv4, addresses));
  EXPECT_TRUE(HasEndpoint(localhost_ipv6, addresses));
}

void TestIPv6LoopbackOnly(const std::string& host) {
  IPEndPoint localhost_ipv6(
      IPAddressNumber(kLocalhostIPv6,
                      kLocalhostIPv6 + arraysize(kLocalhostIPv6)),
      kLocalhostLookupPort);

  AddressList addresses;
  EXPECT_TRUE(ResolveLocalHostname(host, kLocalhostLookupPort, &addresses));
  EXPECT_EQ(1u, addresses.size());
  EXPECT_TRUE(HasEndpoint(localhost_ipv6, addresses));
}

}  // anonymous namespace

TEST(NetUtilTest, GetIdentityFromURL) {
  struct {
    const char* const input_url;
    const char* const expected_username;
    const char* const expected_password;
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
  for (size_t i = 0; i < arraysize(tests); ++i) {
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

TEST(NetUtilTest, CompliantHost) {
  struct CompliantHostCase {
    const char* const host;
    bool expected_output;
  };

  const CompliantHostCase compliant_host_cases[] = {
      {"", false},
      {"a", true},
      {"-", false},
      {"_", false},
      {".", false},
      {"9", true},
      {"9a", true},
      {"9_", true},
      {"a.", true},
      {"a.a", true},
      {"9.a", true},
      {"a.9", true},
      {"_9a", false},
      {"-9a", false},
      {"a.a9", true},
      {"_.9a", true},
      {"a.-a9", false},
      {"a+9a", false},
      {"-a.a9", true},
      {"a_.a9", true},
      {"1-.a-b", true},
      {"1_.a-b", true},
      {"1-2.a_b", true},
      {"a.b.c.d.e", true},
      {"1.2.3.4.5", true},
      {"1.2.3.4.5.", true},
  };

  for (size_t i = 0; i < arraysize(compliant_host_cases); ++i) {
    EXPECT_EQ(compliant_host_cases[i].expected_output,
              IsCanonicalizedHostCompliant(compliant_host_cases[i].host));
  }
}

TEST(NetUtilTest, ParseHostAndPort) {
  const struct {
    const char* const input;
    bool success;
    const char* const expected_host;
    int expected_port;
  } tests[] = {
    // Valid inputs:
    {"foo:10", true, "foo", 10},
    {"foo", true, "foo", -1},
    {
      "[1080:0:0:0:8:800:200C:4171]:11",
      true,
      "1080:0:0:0:8:800:200C:4171",
      11
    },
    {
      "[1080:0:0:0:8:800:200C:4171]",
      true,
      "1080:0:0:0:8:800:200C:4171",
      -1
    },

    // Because no validation is done on the host, the following are accepted,
    // even though they are invalid names.
    {"]", true, "]", -1},
    {"::1", true, ":", 1},
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
    {"[", false, "", -1},
    {"[]", false, "", -1},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
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
    const char* const expected_host_and_port;
  } tests[] = {
    { GURL("http://www.foo.com/x"), "www.foo.com:80"},
    { GURL("http://www.foo.com:21/x"), "www.foo.com:21"},

    // For IPv6 literals should always include the brackets.
    { GURL("http://[1::2]/x"), "[1::2]:80"},
    { GURL("http://[::a]:33/x"), "[::a]:33"},
  };
  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::string host_and_port = GetHostAndPort(tests[i].url);
    EXPECT_EQ(std::string(tests[i].expected_host_and_port), host_and_port);
  }
}

TEST(NetUtilTest, GetHostAndOptionalPort) {
  const struct {
    GURL url;
    const char* const expected_host_and_port;
  } tests[] = {
    { GURL("http://www.foo.com/x"), "www.foo.com"},
    { GURL("http://www.foo.com:21/x"), "www.foo.com:21"},

    // For IPv6 literals should always include the brackets.
    { GURL("http://[1::2]/x"), "[1::2]"},
    { GURL("http://[::a]:33/x"), "[::a]:33"},
  };
  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::string host_and_port = GetHostAndOptionalPort(tests[i].url);
    EXPECT_EQ(std::string(tests[i].expected_host_and_port), host_and_port);
  }
}

TEST(NetUtilTest, NetAddressToString_IPv4) {
  const struct {
    uint8_t addr[4];
    const char* const result;
  } tests[] = {
    {{0, 0, 0, 0}, "0.0.0.0"},
    {{127, 0, 0, 1}, "127.0.0.1"},
    {{192, 168, 0, 1}, "192.168.0.1"},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    SockaddrStorage storage;
    MakeIPv4Address(tests[i].addr, 80, &storage);
    std::string result = NetAddressToString(storage.addr, storage.addr_len);
    EXPECT_EQ(std::string(tests[i].result), result);
  }
}

TEST(NetUtilTest, NetAddressToString_IPv6) {
  const struct {
    uint8_t addr[16];
    const char* const result;
  } tests[] = {
    {{0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10, 0xFE, 0xDC, 0xBA,
      0x98, 0x76, 0x54, 0x32, 0x10},
     "fedc:ba98:7654:3210:fedc:ba98:7654:3210"},
  };

  for (size_t i = 0; i < arraysize(tests); ++i) {
    SockaddrStorage storage;
    MakeIPv6Address(tests[i].addr, 80, &storage);
    EXPECT_EQ(std::string(tests[i].result),
        NetAddressToString(storage.addr, storage.addr_len));
  }
}

TEST(NetUtilTest, NetAddressToStringWithPort_IPv4) {
  uint8_t addr[] = {127, 0, 0, 1};
  SockaddrStorage storage;
  MakeIPv4Address(addr, 166, &storage);
  std::string result = NetAddressToStringWithPort(storage.addr,
                                                  storage.addr_len);
  EXPECT_EQ("127.0.0.1:166", result);
}

TEST(NetUtilTest, NetAddressToStringWithPort_IPv6) {
  uint8_t addr[] = {
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

TEST(NetUtilTest, SimplifyUrlForRequest) {
  struct {
    const char* const input_url;
    const char* const expected_simplified_url;
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
  for (size_t i = 0; i < arraysize(tests); ++i) {
    SCOPED_TRACE(base::StringPrintf("Test[%" PRIuS "]: %s", i,
                                    tests[i].input_url));
    GURL input_url(GURL(tests[i].input_url));
    GURL expected_url(GURL(tests[i].expected_simplified_url));
    EXPECT_EQ(expected_url, SimplifyUrlForRequest(input_url));
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

TEST(NetUtilTest, IsLocalhost) {
  EXPECT_TRUE(IsLocalhost("localhost"));
  EXPECT_TRUE(IsLocalhost("localHosT"));
  EXPECT_TRUE(IsLocalhost("localhost."));
  EXPECT_TRUE(IsLocalhost("localHost."));
  EXPECT_TRUE(IsLocalhost("localhost.localdomain"));
  EXPECT_TRUE(IsLocalhost("localhost.localDOMain"));
  EXPECT_TRUE(IsLocalhost("localhost.localdomain."));
  EXPECT_TRUE(IsLocalhost("localhost6"));
  EXPECT_TRUE(IsLocalhost("localhost6."));
  EXPECT_TRUE(IsLocalhost("localhost6.localdomain6"));
  EXPECT_TRUE(IsLocalhost("localhost6.localdomain6."));
  EXPECT_TRUE(IsLocalhost("127.0.0.1"));
  EXPECT_TRUE(IsLocalhost("127.0.1.0"));
  EXPECT_TRUE(IsLocalhost("127.1.0.0"));
  EXPECT_TRUE(IsLocalhost("127.0.0.255"));
  EXPECT_TRUE(IsLocalhost("127.0.255.0"));
  EXPECT_TRUE(IsLocalhost("127.255.0.0"));
  EXPECT_TRUE(IsLocalhost("::1"));
  EXPECT_TRUE(IsLocalhost("0:0:0:0:0:0:0:1"));
  EXPECT_TRUE(IsLocalhost("foo.localhost"));
  EXPECT_TRUE(IsLocalhost("foo.localhost."));
  EXPECT_TRUE(IsLocalhost("foo.localhoST"));
  EXPECT_TRUE(IsLocalhost("foo.localhoST."));

  EXPECT_FALSE(IsLocalhost("localhostx"));
  EXPECT_FALSE(IsLocalhost("localhost.x"));
  EXPECT_FALSE(IsLocalhost("foo.localdomain"));
  EXPECT_FALSE(IsLocalhost("foo.localdomain.x"));
  EXPECT_FALSE(IsLocalhost("localhost6x"));
  EXPECT_FALSE(IsLocalhost("localhost.localdomain6"));
  EXPECT_FALSE(IsLocalhost("localhost6.localdomain"));
  EXPECT_FALSE(IsLocalhost("127.0.0.1.1"));
  EXPECT_FALSE(IsLocalhost(".127.0.0.255"));
  EXPECT_FALSE(IsLocalhost("::2"));
  EXPECT_FALSE(IsLocalhost("::1:1"));
  EXPECT_FALSE(IsLocalhost("0:0:0:0:1:0:0:1"));
  EXPECT_FALSE(IsLocalhost("::1:1"));
  EXPECT_FALSE(IsLocalhost("0:0:0:0:0:0:0:0:1"));
  EXPECT_FALSE(IsLocalhost("foo.localhost.com"));
  EXPECT_FALSE(IsLocalhost("foo.localhoste"));
  EXPECT_FALSE(IsLocalhost("foo.localhos"));
}

TEST(NetUtilTest, ResolveLocalHostname) {
  AddressList addresses;

  TestBothLoopbackIPs("localhost");
  TestBothLoopbackIPs("localhoST");
  TestBothLoopbackIPs("localhost.");
  TestBothLoopbackIPs("localhoST.");
  TestBothLoopbackIPs("localhost.localdomain");
  TestBothLoopbackIPs("localhost.localdomAIn");
  TestBothLoopbackIPs("localhost.localdomain.");
  TestBothLoopbackIPs("localhost.localdomAIn.");
  TestBothLoopbackIPs("foo.localhost");
  TestBothLoopbackIPs("foo.localhOSt");
  TestBothLoopbackIPs("foo.localhost.");
  TestBothLoopbackIPs("foo.localhOSt.");

  TestIPv6LoopbackOnly("localhost6");
  TestIPv6LoopbackOnly("localhoST6");
  TestIPv6LoopbackOnly("localhost6.");
  TestIPv6LoopbackOnly("localhost6.localdomain6");
  TestIPv6LoopbackOnly("localhost6.localdomain6.");

  EXPECT_FALSE(
      ResolveLocalHostname("127.0.0.1", kLocalhostLookupPort, &addresses));
  EXPECT_FALSE(ResolveLocalHostname("::1", kLocalhostLookupPort, &addresses));
  EXPECT_FALSE(ResolveLocalHostname("0:0:0:0:0:0:0:1", kLocalhostLookupPort,
                                    &addresses));
  EXPECT_FALSE(
      ResolveLocalHostname("localhostx", kLocalhostLookupPort, &addresses));
  EXPECT_FALSE(
      ResolveLocalHostname("localhost.x", kLocalhostLookupPort, &addresses));
  EXPECT_FALSE(ResolveLocalHostname("foo.localdomain", kLocalhostLookupPort,
                                    &addresses));
  EXPECT_FALSE(ResolveLocalHostname("foo.localdomain.x", kLocalhostLookupPort,
                                    &addresses));
  EXPECT_FALSE(
      ResolveLocalHostname("localhost6x", kLocalhostLookupPort, &addresses));
  EXPECT_FALSE(ResolveLocalHostname("localhost.localdomain6",
                                    kLocalhostLookupPort, &addresses));
  EXPECT_FALSE(ResolveLocalHostname("localhost6.localdomain",
                                    kLocalhostLookupPort, &addresses));
  EXPECT_FALSE(
      ResolveLocalHostname("127.0.0.1.1", kLocalhostLookupPort, &addresses));
  EXPECT_FALSE(
      ResolveLocalHostname(".127.0.0.255", kLocalhostLookupPort, &addresses));
  EXPECT_FALSE(ResolveLocalHostname("::2", kLocalhostLookupPort, &addresses));
  EXPECT_FALSE(ResolveLocalHostname("::1:1", kLocalhostLookupPort, &addresses));
  EXPECT_FALSE(ResolveLocalHostname("0:0:0:0:1:0:0:1", kLocalhostLookupPort,
                                    &addresses));
  EXPECT_FALSE(ResolveLocalHostname("::1:1", kLocalhostLookupPort, &addresses));
  EXPECT_FALSE(ResolveLocalHostname("0:0:0:0:0:0:0:0:1", kLocalhostLookupPort,
                                    &addresses));
  EXPECT_FALSE(ResolveLocalHostname("foo.localhost.com", kLocalhostLookupPort,
                                    &addresses));
  EXPECT_FALSE(
      ResolveLocalHostname("foo.localhoste", kLocalhostLookupPort, &addresses));
}

TEST(NetUtilTest, GoogleHost) {
  struct GoogleHostCase {
    GURL url;
    bool expected_output;
  };

  const GoogleHostCase google_host_cases[] = {
      {GURL("http://.google.com"), true},
      {GURL("http://.youtube.com"), true},
      {GURL("http://.gmail.com"), true},
      {GURL("http://.doubleclick.net"), true},
      {GURL("http://.gstatic.com"), true},
      {GURL("http://.googlevideo.com"), true},
      {GURL("http://.googleusercontent.com"), true},
      {GURL("http://.googlesyndication.com"), true},
      {GURL("http://.google-analytics.com"), true},
      {GURL("http://.googleadservices.com"), true},
      {GURL("http://.googleapis.com"), true},
      {GURL("http://a.google.com"), true},
      {GURL("http://b.youtube.com"), true},
      {GURL("http://c.gmail.com"), true},
      {GURL("http://google.com"), false},
      {GURL("http://youtube.com"), false},
      {GURL("http://gmail.com"), false},
      {GURL("http://google.coma"), false},
      {GURL("http://agoogle.com"), false},
      {GURL("http://oogle.com"), false},
      {GURL("http://google.co"), false},
      {GURL("http://oggole.com"), false},
  };

  for (size_t i = 0; i < arraysize(google_host_cases); ++i) {
    EXPECT_EQ(google_host_cases[i].expected_output,
              HasGoogleHost(google_host_cases[i].url));
  }
}

struct NonUniqueNameTestData {
  bool is_unique;
  const char* const hostname;
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
