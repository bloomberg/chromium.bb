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

#if defined(OS_WIN)
#include "net/base/net_util_win.h"
#else  // OS_WIN
#include "net/base/net_util_posix.h"
#if defined(OS_MACOSX)
#include "net/base/net_util_mac.h"
#else  // OS_MACOSX
#include "net/base/net_util_linux.h"
#endif  // OS_MACOSX
#endif  // OS_WIN

using base::ASCIIToUTF16;
using base::WideToUTF16;

namespace net {

namespace {

struct HeaderCase {
  const char* const header_name;
  const char* const expected;
};

#if defined(OS_LINUX) || defined(OS_ANDROID) || defined(OS_CHROMEOS)
const char kWiFiSSID[] = "TestWiFi";
const char kInterfaceWithDifferentSSID[] = "wlan999";

std::string TestGetInterfaceSSID(const std::string& ifname) {
  return (ifname == kInterfaceWithDifferentSSID) ? "AnotherSSID" : kWiFiSSID;
}
#endif

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

#if defined(OS_MACOSX)
class IPAttributesGetterTest : public internal::IPAttributesGetterMac {
 public:
  IPAttributesGetterTest() : native_attributes_(0) {}
  bool IsInitialized() const override { return true; }
  bool GetIPAttributes(const char* ifname,
                       const sockaddr* sock_addr,
                       int* native_attributes) override {
    *native_attributes = native_attributes_;
    return true;
  }
  void set_native_attributes(int native_attributes) {
    native_attributes_ = native_attributes;
  }

 private:
  int native_attributes_;
};

// Helper function to create a single valid ifaddrs
bool FillIfaddrs(ifaddrs* interfaces,
                 const char* ifname,
                 uint flags,
                 const IPAddressNumber& ip_address,
                 const IPAddressNumber& ip_netmask,
                 sockaddr_storage sock_addrs[2]) {
  interfaces->ifa_next = NULL;
  interfaces->ifa_name = const_cast<char*>(ifname);
  interfaces->ifa_flags = flags;

  socklen_t sock_len = sizeof(sockaddr_storage);

  // Convert to sockaddr for next check.
  if (!IPEndPoint(ip_address, 0)
           .ToSockAddr(reinterpret_cast<sockaddr*>(&sock_addrs[0]),
                       &sock_len)) {
    return false;
  }
  interfaces->ifa_addr = reinterpret_cast<sockaddr*>(&sock_addrs[0]);

  sock_len = sizeof(sockaddr_storage);
  if (!IPEndPoint(ip_netmask, 0)
           .ToSockAddr(reinterpret_cast<sockaddr*>(&sock_addrs[1]),
                       &sock_len)) {
    return false;
  }
  interfaces->ifa_netmask = reinterpret_cast<sockaddr*>(&sock_addrs[1]);

  return true;
}
#endif  // OS_MACOSX

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

// Just a bunch of fake headers.
const char google_headers[] =
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
  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::string result =
        GetSpecificHeader(google_headers, tests[i].header_name);
    EXPECT_EQ(result, tests[i].expected);
  }

  // Test again with empty headers.
  for (size_t i = 0; i < arraysize(tests); ++i) {
    std::string result = GetSpecificHeader(std::string(), tests[i].header_name);
    EXPECT_EQ(result, std::string());
  }
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
    {".", false},
    {"9", true},
    {"9a", true},
    {"a.", true},
    {"a.a", true},
    {"9.a", true},
    {"a.9", true},
    {"_9a", false},
    {"-9a", false},
    {"a.a9", true},
    {"a.-a9", false},
    {"a+9a", false},
    {"-a.a9", true},
    {"1-.a-b", true},
    {"1_.a-b", false},
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
    uint8 addr[16];
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

TEST(NetUtilTest, SetExplicitlyAllowedPortsTest) {
  std::string invalid[] = { "1,2,a", "'1','2'", "1, 2, 3", "1 0,11,12" };
  std::string valid[] = { "", "1", "1,2", "1,2,3", "10,11,12,13" };

  for (size_t i = 0; i < arraysize(invalid); ++i) {
    SetExplicitlyAllowedPorts(invalid[i]);
    EXPECT_EQ(0, static_cast<int>(GetCountOfExplicitlyAllowedPorts()));
  }

  for (size_t i = 0; i < arraysize(valid); ++i) {
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

TEST(NetUtilTest, ParseURLHostnameToNumber_FailParse) {
  IPAddressNumber number;

  EXPECT_FALSE(ParseURLHostnameToNumber("bad value", &number));
  EXPECT_FALSE(ParseURLHostnameToNumber("bad:value", &number));
  EXPECT_FALSE(ParseURLHostnameToNumber(std::string(), &number));
  EXPECT_FALSE(ParseURLHostnameToNumber("192.168.0.1:30", &number));
  EXPECT_FALSE(ParseURLHostnameToNumber("  192.168.0.1  ", &number));
  EXPECT_FALSE(ParseURLHostnameToNumber("::1", &number));
}

TEST(NetUtilTest, ParseURLHostnameToNumber_IPv4) {
  IPAddressNumber number;
  EXPECT_TRUE(ParseURLHostnameToNumber("192.168.0.1", &number));
  EXPECT_EQ("192,168,0,1", DumpIPNumber(number));
  EXPECT_EQ("192.168.0.1", IPAddressToString(number));
}

TEST(NetUtilTest, ParseURLHostnameToNumber_IPv6) {
  IPAddressNumber number;
  EXPECT_TRUE(ParseURLHostnameToNumber("[1:abcd::3:4:ff]", &number));
  EXPECT_EQ("0,1,171,205,0,0,0,0,0,0,0,3,0,4,0,255", DumpIPNumber(number));
  EXPECT_EQ("1:abcd::3:4:ff", IPAddressToString(number));
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
  const char* const bad_literals[] = {
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
    const char* const cidr_literal;
    const char* const ip_literal;
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
  for (size_t i = 0; i < arraysize(tests); ++i) {
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
    EXPECT_GT(it->prefix_length, 1u);
    EXPECT_LE(it->prefix_length, it->address.size() * 8);

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
    if (it->type == NetworkChangeNotifier::CONNECTION_WIFI) {
      EXPECT_NE(WIFI_PHY_LAYER_PROTOCOL_NONE, GetWifiPHYLayerProtocol());
    }
#elif !defined(OS_ANDROID)
    char name[IF_NAMESIZE];
    EXPECT_TRUE(if_indextoname(it->interface_index, name));
    EXPECT_STREQ(it->name.c_str(), name);
#endif
  }
}

static const char ifname_em1[] = "em1";
#if defined(OS_WIN)
static const char ifname_vm[] = "VMnet";
#else
static const char ifname_vm[] = "vmnet";
#endif  // OS_WIN

static const unsigned char kIPv6LocalAddr[] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1};

// The following 3 addresses need to be changed together. IPv6Addr is the IPv6
// address. IPv6Netmask is the mask address with as many leading bits set to 1
// as the prefix length. IPv6AddrPrefix needs to match IPv6Addr with the same
// number of bits as the prefix length.
static const unsigned char kIPv6Addr[] =
  {0x24, 0x01, 0xfa, 0x00, 0x00, 0x04, 0x10, 0x00, 0xbe, 0x30, 0x5b, 0xff,
   0xfe, 0xe5, 0x00, 0xc3};
#if defined(OS_WIN)
static const unsigned char kIPv6AddrPrefix[] =
  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00};
#endif  // OS_WIN
#if defined(OS_MACOSX)
static const unsigned char kIPv6Netmask[] =
  {0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00};
#endif  // OS_MACOSX

#if !defined(OS_MACOSX) && !defined(OS_WIN) && !defined(OS_NACL)

char* CopyInterfaceName(const char* ifname, int ifname_size, char* output) {
  EXPECT_LT(ifname_size, IF_NAMESIZE);
  memcpy(output, ifname, ifname_size);
  return output;
}

char* GetInterfaceName(int interface_index, char* ifname) {
  return CopyInterfaceName(ifname_em1, arraysize(ifname_em1), ifname);
}

char* GetInterfaceNameVM(int interface_index, char* ifname) {
  return CopyInterfaceName(ifname_vm, arraysize(ifname_vm), ifname);
}

TEST(NetUtilTest, GetNetworkListTrimming) {
  IPAddressNumber ipv6_local_address(
      kIPv6LocalAddr, kIPv6LocalAddr + arraysize(kIPv6LocalAddr));
  IPAddressNumber ipv6_address(kIPv6Addr, kIPv6Addr + arraysize(kIPv6Addr));

  NetworkInterfaceList results;
  ::base::hash_set<int> online_links;
  net::internal::AddressTrackerLinux::AddressMap address_map;

  // Interface 1 is offline.
  struct ifaddrmsg msg = {
      AF_INET6,
      1,               /* prefix length */
      IFA_F_TEMPORARY, /* address flags */
      0,               /* link scope */
      1                /* link index */
  };

  // Address of offline links should be ignored.
  ASSERT_TRUE(address_map.insert(std::make_pair(ipv6_address, msg)).second);
  EXPECT_TRUE(
      net::internal::GetNetworkListImpl(&results,
                                        INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES,
                                        online_links,
                                        address_map,
                                        GetInterfaceName));
  EXPECT_EQ(results.size(), 0ul);

  // Mark interface 1 online.
  online_links.insert(1);

  // Local address should be trimmed out.
  address_map.clear();
  ASSERT_TRUE(
      address_map.insert(std::make_pair(ipv6_local_address, msg)).second);
  EXPECT_TRUE(
      net::internal::GetNetworkListImpl(&results,
                                        INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES,
                                        online_links,
                                        address_map,
                                        GetInterfaceName));
  EXPECT_EQ(results.size(), 0ul);

  // vmware address should return by default.
  address_map.clear();
  ASSERT_TRUE(address_map.insert(std::make_pair(ipv6_address, msg)).second);
  EXPECT_TRUE(
      net::internal::GetNetworkListImpl(&results,
                                        INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES,
                                        online_links,
                                        address_map,
                                        GetInterfaceNameVM));
  EXPECT_EQ(results.size(), 1ul);
  EXPECT_EQ(results[0].name, ifname_vm);
  EXPECT_EQ(results[0].prefix_length, 1ul);
  EXPECT_EQ(results[0].address, ipv6_address);
  results.clear();

  // vmware address should be trimmed out if policy specified so.
  address_map.clear();
  ASSERT_TRUE(address_map.insert(std::make_pair(ipv6_address, msg)).second);
  EXPECT_TRUE(
      net::internal::GetNetworkListImpl(&results,
                                        EXCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES,
                                        online_links,
                                        address_map,
                                        GetInterfaceNameVM));
  EXPECT_EQ(results.size(), 0ul);
  results.clear();

  // Addresses with banned attributes should be ignored.
  address_map.clear();
  msg.ifa_flags = IFA_F_TENTATIVE;
  ASSERT_TRUE(address_map.insert(std::make_pair(ipv6_address, msg)).second);
  EXPECT_TRUE(
      net::internal::GetNetworkListImpl(&results,
                                        INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES,
                                        online_links,
                                        address_map,
                                        GetInterfaceName));
  EXPECT_EQ(results.size(), 0ul);
  results.clear();

  // Addresses with allowed attribute IFA_F_TEMPORARY should be returned and
  // attributes should be translated correctly.
  address_map.clear();
  msg.ifa_flags = IFA_F_TEMPORARY;
  ASSERT_TRUE(address_map.insert(std::make_pair(ipv6_address, msg)).second);
  EXPECT_TRUE(
      net::internal::GetNetworkListImpl(&results,
                                        INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES,
                                        online_links,
                                        address_map,
                                        GetInterfaceName));
  EXPECT_EQ(results.size(), 1ul);
  EXPECT_EQ(results[0].name, ifname_em1);
  EXPECT_EQ(results[0].prefix_length, 1ul);
  EXPECT_EQ(results[0].address, ipv6_address);
  EXPECT_EQ(results[0].ip_address_attributes, IP_ADDRESS_ATTRIBUTE_TEMPORARY);
  results.clear();

  // Addresses with allowed attribute IFA_F_DEPRECATED should be returned and
  // attributes should be translated correctly.
  address_map.clear();
  msg.ifa_flags = IFA_F_DEPRECATED;
  ASSERT_TRUE(address_map.insert(std::make_pair(ipv6_address, msg)).second);
  EXPECT_TRUE(
      net::internal::GetNetworkListImpl(&results,
                                        INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES,
                                        online_links,
                                        address_map,
                                        GetInterfaceName));
  EXPECT_EQ(results.size(), 1ul);
  EXPECT_EQ(results[0].name, ifname_em1);
  EXPECT_EQ(results[0].prefix_length, 1ul);
  EXPECT_EQ(results[0].address, ipv6_address);
  EXPECT_EQ(results[0].ip_address_attributes, IP_ADDRESS_ATTRIBUTE_DEPRECATED);
  results.clear();
}

#elif defined(OS_MACOSX)

TEST(NetUtilTest, GetNetworkListTrimming) {
  IPAddressNumber ipv6_local_address(
      kIPv6LocalAddr, kIPv6LocalAddr + arraysize(kIPv6LocalAddr));
  IPAddressNumber ipv6_address(kIPv6Addr, kIPv6Addr + arraysize(kIPv6Addr));
  IPAddressNumber ipv6_netmask(kIPv6Netmask,
                               kIPv6Netmask + arraysize(kIPv6Netmask));

  NetworkInterfaceList results;
  IPAttributesGetterTest ip_attributes_getter;
  sockaddr_storage addresses[2];
  ifaddrs interface;

  // Address of offline links should be ignored.
  ASSERT_TRUE(FillIfaddrs(&interface, ifname_em1, IFF_UP, ipv6_address,
                          ipv6_netmask, addresses));
  EXPECT_TRUE(net::internal::GetNetworkListImpl(
      &results, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, &interface,
      &ip_attributes_getter));
  EXPECT_EQ(results.size(), 0ul);

  // Local address should be trimmed out.
  ASSERT_TRUE(FillIfaddrs(&interface, ifname_em1, IFF_RUNNING,
                          ipv6_local_address, ipv6_netmask, addresses));
  EXPECT_TRUE(net::internal::GetNetworkListImpl(
      &results, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, &interface,
      &ip_attributes_getter));
  EXPECT_EQ(results.size(), 0ul);

  // vmware address should return by default.
  ASSERT_TRUE(FillIfaddrs(&interface, ifname_vm, IFF_RUNNING, ipv6_address,
                          ipv6_netmask, addresses));
  EXPECT_TRUE(net::internal::GetNetworkListImpl(
      &results, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, &interface,
      &ip_attributes_getter));
  EXPECT_EQ(results.size(), 1ul);
  EXPECT_EQ(results[0].name, ifname_vm);
  EXPECT_EQ(results[0].prefix_length, 1ul);
  EXPECT_EQ(results[0].address, ipv6_address);
  results.clear();

  // vmware address should be trimmed out if policy specified so.
  ASSERT_TRUE(FillIfaddrs(&interface, ifname_vm, IFF_RUNNING, ipv6_address,
                          ipv6_netmask, addresses));
  EXPECT_TRUE(net::internal::GetNetworkListImpl(
      &results, EXCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, &interface,
      &ip_attributes_getter));
  EXPECT_EQ(results.size(), 0ul);
  results.clear();

#if !defined(OS_IOS)
  // Addresses with banned attributes should be ignored.
  ip_attributes_getter.set_native_attributes(IN6_IFF_ANYCAST);
  ASSERT_TRUE(FillIfaddrs(&interface, ifname_em1, IFF_RUNNING, ipv6_address,
                          ipv6_netmask, addresses));
  EXPECT_TRUE(net::internal::GetNetworkListImpl(
      &results, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, &interface,
      &ip_attributes_getter));
  EXPECT_EQ(results.size(), 0ul);
  results.clear();

  // Addresses with allowed attribute IFA_F_TEMPORARY should be returned and
  // attributes should be translated correctly.
  ip_attributes_getter.set_native_attributes(IN6_IFF_TEMPORARY);
  ASSERT_TRUE(FillIfaddrs(&interface, ifname_em1, IFF_RUNNING, ipv6_address,
                          ipv6_netmask, addresses));
  EXPECT_TRUE(net::internal::GetNetworkListImpl(
      &results, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, &interface,
      &ip_attributes_getter));
  EXPECT_EQ(results.size(), 1ul);
  EXPECT_EQ(results[0].name, ifname_em1);
  EXPECT_EQ(results[0].prefix_length, 1ul);
  EXPECT_EQ(results[0].address, ipv6_address);
  EXPECT_EQ(results[0].ip_address_attributes, IP_ADDRESS_ATTRIBUTE_TEMPORARY);
  results.clear();

  // Addresses with allowed attribute IFA_F_DEPRECATED should be returned and
  // attributes should be translated correctly.
  ip_attributes_getter.set_native_attributes(IN6_IFF_DEPRECATED);
  ASSERT_TRUE(FillIfaddrs(&interface, ifname_em1, IFF_RUNNING, ipv6_address,
                          ipv6_netmask, addresses));
  EXPECT_TRUE(net::internal::GetNetworkListImpl(
      &results, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, &interface,
      &ip_attributes_getter));
  EXPECT_EQ(results.size(), 1ul);
  EXPECT_EQ(results[0].name, ifname_em1);
  EXPECT_EQ(results[0].prefix_length, 1ul);
  EXPECT_EQ(results[0].address, ipv6_address);
  EXPECT_EQ(results[0].ip_address_attributes, IP_ADDRESS_ATTRIBUTE_DEPRECATED);
  results.clear();
#endif  // !OS_IOS
}
#elif defined(OS_WIN)  // !OS_MACOSX && !OS_WIN && !OS_NACL

// Helper function to create a valid IP_ADAPTER_ADDRESSES with reasonable
// default value. The output is the |adapter_address|. All the rests are input
// to fill the |adapter_address|. |sock_addrs| are temporary storage used by
// |adapter_address| once the function is returned.
bool FillAdapterAddress(IP_ADAPTER_ADDRESSES* adapter_address,
                        const char* ifname,
                        const IPAddressNumber& ip_address,
                        const IPAddressNumber& ip_netmask,
                        sockaddr_storage sock_addrs[2]) {
  adapter_address->AdapterName = const_cast<char*>(ifname);
  adapter_address->FriendlyName = const_cast<PWCHAR>(L"interface");
  adapter_address->IfType = IF_TYPE_ETHERNET_CSMACD;
  adapter_address->OperStatus = IfOperStatusUp;
  adapter_address->FirstUnicastAddress->DadState = IpDadStatePreferred;
  adapter_address->FirstUnicastAddress->PrefixOrigin = IpPrefixOriginOther;
  adapter_address->FirstUnicastAddress->SuffixOrigin = IpSuffixOriginOther;
  adapter_address->FirstUnicastAddress->PreferredLifetime = 100;
  adapter_address->FirstUnicastAddress->ValidLifetime = 1000;

  socklen_t sock_len = sizeof(sockaddr_storage);

  // Convert to sockaddr for next check.
  if (!IPEndPoint(ip_address, 0)
           .ToSockAddr(reinterpret_cast<sockaddr*>(&sock_addrs[0]),
                       &sock_len)) {
    return false;
  }
  adapter_address->FirstUnicastAddress->Address.lpSockaddr =
      reinterpret_cast<sockaddr*>(&sock_addrs[0]);
  adapter_address->FirstUnicastAddress->Address.iSockaddrLength = sock_len;
  adapter_address->FirstUnicastAddress->OnLinkPrefixLength = 1;

  sock_len = sizeof(sockaddr_storage);
  if (!IPEndPoint(ip_netmask, 0)
           .ToSockAddr(reinterpret_cast<sockaddr*>(&sock_addrs[1]),
                       &sock_len)) {
    return false;
  }
  adapter_address->FirstPrefix->Address.lpSockaddr =
      reinterpret_cast<sockaddr*>(&sock_addrs[1]);
  adapter_address->FirstPrefix->Address.iSockaddrLength = sock_len;
  adapter_address->FirstPrefix->PrefixLength = 1;

  DCHECK_EQ(sock_addrs[0].ss_family, sock_addrs[1].ss_family);
  if (sock_addrs[0].ss_family == AF_INET6) {
    adapter_address->Ipv6IfIndex = 0;
  } else {
    DCHECK_EQ(sock_addrs[0].ss_family, AF_INET);
    adapter_address->IfIndex = 0;
  }

  return true;
}

TEST(NetUtilTest, GetNetworkListTrimming) {
  IPAddressNumber ipv6_local_address(
      kIPv6LocalAddr, kIPv6LocalAddr + arraysize(kIPv6LocalAddr));
  IPAddressNumber ipv6_address(kIPv6Addr, kIPv6Addr + arraysize(kIPv6Addr));
  IPAddressNumber ipv6_prefix(kIPv6AddrPrefix,
                              kIPv6AddrPrefix + arraysize(kIPv6AddrPrefix));

  NetworkInterfaceList results;
  sockaddr_storage addresses[2];
  IP_ADAPTER_ADDRESSES adapter_address = {0};
  IP_ADAPTER_UNICAST_ADDRESS address = {0};
  IP_ADAPTER_PREFIX adapter_prefix = {0};
  adapter_address.FirstUnicastAddress = &address;
  adapter_address.FirstPrefix = &adapter_prefix;

  // Address of offline links should be ignored.
  ASSERT_TRUE(FillAdapterAddress(
      &adapter_address /* adapter_address */, ifname_em1 /* ifname */,
      ipv6_address /* ip_address */, ipv6_prefix /* ip_netmask */,
      addresses /* sock_addrs */));
  adapter_address.OperStatus = IfOperStatusDown;

  EXPECT_TRUE(net::internal::GetNetworkListImpl(
      &results, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, true, &adapter_address));

  EXPECT_EQ(results.size(), 0ul);

  // Address on loopback interface should be trimmed out.
  ASSERT_TRUE(FillAdapterAddress(
      &adapter_address /* adapter_address */, ifname_em1 /* ifname */,
      ipv6_local_address /* ip_address */, ipv6_prefix /* ip_netmask */,
      addresses /* sock_addrs */));
  adapter_address.IfType = IF_TYPE_SOFTWARE_LOOPBACK;

  EXPECT_TRUE(net::internal::GetNetworkListImpl(
      &results, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, true, &adapter_address));
  EXPECT_EQ(results.size(), 0ul);

  // vmware address should return by default.
  ASSERT_TRUE(FillAdapterAddress(
      &adapter_address /* adapter_address */, ifname_vm /* ifname */,
      ipv6_address /* ip_address */, ipv6_prefix /* ip_netmask */,
      addresses /* sock_addrs */));
  EXPECT_TRUE(net::internal::GetNetworkListImpl(
      &results, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, true, &adapter_address));
  EXPECT_EQ(results.size(), 1ul);
  EXPECT_EQ(results[0].name, ifname_vm);
  EXPECT_EQ(results[0].prefix_length, 1ul);
  EXPECT_EQ(results[0].address, ipv6_address);
  EXPECT_EQ(results[0].ip_address_attributes, IP_ADDRESS_ATTRIBUTE_NONE);
  results.clear();

  // vmware address should be trimmed out if policy specified so.
  ASSERT_TRUE(FillAdapterAddress(
      &adapter_address /* adapter_address */, ifname_vm /* ifname */,
      ipv6_address /* ip_address */, ipv6_prefix /* ip_netmask */,
      addresses /* sock_addrs */));
  EXPECT_TRUE(net::internal::GetNetworkListImpl(
      &results, EXCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, true, &adapter_address));
  EXPECT_EQ(results.size(), 0ul);
  results.clear();

  // Addresses with incompleted DAD should be ignored.
  ASSERT_TRUE(FillAdapterAddress(
      &adapter_address /* adapter_address */, ifname_em1 /* ifname */,
      ipv6_address /* ip_address */, ipv6_prefix /* ip_netmask */,
      addresses /* sock_addrs */));
  adapter_address.FirstUnicastAddress->DadState = IpDadStateTentative;

  EXPECT_TRUE(net::internal::GetNetworkListImpl(
      &results, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, true, &adapter_address));
  EXPECT_EQ(results.size(), 0ul);
  results.clear();

  // Addresses with allowed attribute IpSuffixOriginRandom should be returned
  // and attributes should be translated correctly to
  // IP_ADDRESS_ATTRIBUTE_TEMPORARY.
  ASSERT_TRUE(FillAdapterAddress(
      &adapter_address /* adapter_address */, ifname_em1 /* ifname */,
      ipv6_address /* ip_address */, ipv6_prefix /* ip_netmask */,
      addresses /* sock_addrs */));
  adapter_address.FirstUnicastAddress->PrefixOrigin =
      IpPrefixOriginRouterAdvertisement;
  adapter_address.FirstUnicastAddress->SuffixOrigin = IpSuffixOriginRandom;

  EXPECT_TRUE(net::internal::GetNetworkListImpl(
      &results, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, true, &adapter_address));
  EXPECT_EQ(results.size(), 1ul);
  EXPECT_EQ(results[0].name, ifname_em1);
  EXPECT_EQ(results[0].prefix_length, 1ul);
  EXPECT_EQ(results[0].address, ipv6_address);
  EXPECT_EQ(results[0].ip_address_attributes, IP_ADDRESS_ATTRIBUTE_TEMPORARY);
  results.clear();

  // Addresses with preferred lifetime 0 should be returned and
  // attributes should be translated correctly to
  // IP_ADDRESS_ATTRIBUTE_DEPRECATED.
  ASSERT_TRUE(FillAdapterAddress(
      &adapter_address /* adapter_address */, ifname_em1 /* ifname */,
      ipv6_address /* ip_address */, ipv6_prefix /* ip_netmask */,
      addresses /* sock_addrs */));
  adapter_address.FirstUnicastAddress->PreferredLifetime = 0;
  adapter_address.FriendlyName = const_cast<PWCHAR>(L"FriendlyInterfaceName");
  EXPECT_TRUE(net::internal::GetNetworkListImpl(
      &results, INCLUDE_HOST_SCOPE_VIRTUAL_INTERFACES, true, &adapter_address));
  EXPECT_EQ(results.size(), 1ul);
  EXPECT_EQ(results[0].friendly_name, "FriendlyInterfaceName");
  EXPECT_EQ(results[0].name, ifname_em1);
  EXPECT_EQ(results[0].prefix_length, 1ul);
  EXPECT_EQ(results[0].address, ipv6_address);
  EXPECT_EQ(results[0].ip_address_attributes, IP_ADDRESS_ATTRIBUTE_DEPRECATED);
  results.clear();
}

#endif  // !OS_MACOSX && !OS_WIN && !OS_NACL

TEST(NetUtilTest, GetWifiSSID) {
  // We can't check the result of GetWifiSSID() directly, since the result
  // will differ across machines. Simply exercise the code path and hope that it
  // doesn't crash.
  EXPECT_NE((const char*)NULL, GetWifiSSID().c_str());
}

#if defined(OS_LINUX) || defined(OS_ANDROID) || defined(OS_CHROMEOS)
TEST(NetUtilTest, GetWifiSSIDFromInterfaceList) {
  NetworkInterfaceList list;
  EXPECT_EQ(std::string(), net::internal::GetWifiSSIDFromInterfaceListInternal(
                               list, TestGetInterfaceSSID));

  NetworkInterface interface1;
  interface1.name = "wlan0";
  interface1.type = NetworkChangeNotifier::CONNECTION_WIFI;
  list.push_back(interface1);
  ASSERT_EQ(1u, list.size());
  EXPECT_EQ(std::string(kWiFiSSID),
            net::internal::GetWifiSSIDFromInterfaceListInternal(
                list, TestGetInterfaceSSID));

  NetworkInterface interface2;
  interface2.name = "wlan1";
  interface2.type = NetworkChangeNotifier::CONNECTION_WIFI;
  list.push_back(interface2);
  ASSERT_EQ(2u, list.size());
  EXPECT_EQ(std::string(kWiFiSSID),
            net::internal::GetWifiSSIDFromInterfaceListInternal(
                list, TestGetInterfaceSSID));

  NetworkInterface interface3;
  interface3.name = kInterfaceWithDifferentSSID;
  interface3.type = NetworkChangeNotifier::CONNECTION_WIFI;
  list.push_back(interface3);
  ASSERT_EQ(3u, list.size());
  EXPECT_EQ(std::string(), net::internal::GetWifiSSIDFromInterfaceListInternal(
                               list, TestGetInterfaceSSID));

  list.pop_back();
  NetworkInterface interface4;
  interface4.name = "eth0";
  interface4.type = NetworkChangeNotifier::CONNECTION_ETHERNET;
  list.push_back(interface4);
  ASSERT_EQ(3u, list.size());
  EXPECT_EQ(std::string(), net::internal::GetWifiSSIDFromInterfaceListInternal(
                               list, TestGetInterfaceSSID));
}
#endif  // OS_LINUX

namespace {

#if defined(OS_WIN)
bool read_int_or_bool(DWORD data_size,
                      PVOID data) {
  switch (data_size) {
    case 1:
      return !!*reinterpret_cast<uint8*>(data);
    case 4:
      return !!*reinterpret_cast<uint32*>(data);
    default:
      LOG(FATAL) << "That is not a type I know!";
      return false;
  }
}

int GetWifiOptions() {
  const internal::WlanApi& wlanapi = internal::WlanApi::GetInstance();
  if (!wlanapi.initialized)
    return -1;

  internal::WlanHandle client;
  DWORD cur_version = 0;
  const DWORD kMaxClientVersion = 2;
  DWORD result = wlanapi.OpenHandle(
      kMaxClientVersion, &cur_version, &client);
  if (result != ERROR_SUCCESS)
    return -1;

  WLAN_INTERFACE_INFO_LIST* interface_list_ptr = NULL;
  result = wlanapi.enum_interfaces_func(client.Get(), NULL,
                                        &interface_list_ptr);
  if (result != ERROR_SUCCESS)
    return -1;
  scoped_ptr<WLAN_INTERFACE_INFO_LIST, internal::WlanApiDeleter> interface_list(
      interface_list_ptr);

  for (unsigned i = 0; i < interface_list->dwNumberOfItems; ++i) {
    WLAN_INTERFACE_INFO* info = &interface_list->InterfaceInfo[i];
    DWORD data_size;
    PVOID data;
    int options = 0;
    result = wlanapi.query_interface_func(
        client.Get(),
        &info->InterfaceGuid,
        wlan_intf_opcode_background_scan_enabled,
        NULL,
        &data_size,
        &data,
        NULL);
    if (result != ERROR_SUCCESS)
      continue;
    if (!read_int_or_bool(data_size, data)) {
      options |= WIFI_OPTIONS_DISABLE_SCAN;
    }
    internal::WlanApi::GetInstance().free_memory_func(data);

    result = wlanapi.query_interface_func(
        client.Get(),
        &info->InterfaceGuid,
        wlan_intf_opcode_media_streaming_mode,
        NULL,
        &data_size,
        &data,
        NULL);
    if (result != ERROR_SUCCESS)
      continue;
    if (read_int_or_bool(data_size, data)) {
      options |= WIFI_OPTIONS_MEDIA_STREAMING_MODE;
    }
    internal::WlanApi::GetInstance().free_memory_func(data);

    // Just the the options from the first succesful
    // interface.
    return options;
  }

  // No wifi interface found.
  return -1;
}

#else  // OS_WIN

int GetWifiOptions() {
  // Not supported.
  return -1;
}

#endif  // OS_WIN

void TryChangeWifiOptions(int options) {
  int previous_options = GetWifiOptions();
  scoped_ptr<ScopedWifiOptions> scoped_options = SetWifiOptions(options);
  EXPECT_EQ(previous_options | options, GetWifiOptions());
  scoped_options.reset();
  EXPECT_EQ(previous_options, GetWifiOptions());
}

};  // namespace

// Test SetWifiOptions().
TEST(NetUtilTest, SetWifiOptionsTest) {
  TryChangeWifiOptions(0);
  TryChangeWifiOptions(WIFI_OPTIONS_DISABLE_SCAN);
  TryChangeWifiOptions(WIFI_OPTIONS_MEDIA_STREAMING_MODE);
  TryChangeWifiOptions(WIFI_OPTIONS_DISABLE_SCAN |
                       WIFI_OPTIONS_MEDIA_STREAMING_MODE);
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
