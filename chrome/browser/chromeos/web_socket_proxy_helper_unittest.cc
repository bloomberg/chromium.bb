// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>
#include "base/string_number_conversions.h"
#include "chrome/browser/chromeos/web_socket_proxy_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

class WebSocketProxyHelperTest : public testing::Test {
 public:
  void FetchAndTest(const std::string& input,
                    const std::string& passport,
                    const std::string& ip,
                    const std::string& hostname,
                    int port, bool success) {
    std::string passport_out;
    std::string ip_out;
    std::string hostname_out;
    int port_out;
    bool result = WebSocketProxyHelper::FetchPassportAddrNamePort(
        (uint8*)input.data(), (uint8*)input.data() + input.length(),
        &passport_out, &ip_out, &hostname_out, &port_out);
    ASSERT_EQ(success, result) << "Input was: " << input;
    if (success) {
      EXPECT_EQ(passport, passport_out);
      EXPECT_EQ(ip, ip_out);
      EXPECT_EQ(hostname, hostname_out);
      EXPECT_EQ(port, port_out);
    }
  }
};

TEST_F(WebSocketProxyHelperTest, FetchPassportAddrNamePortSuccess) {
  std::vector<std::string> ips;
  ips.push_back("127.0.0.1");
  ips.push_back("[ab:ab:ab:00:ed:78]");
  std::vector<std::string> hostnames = ips;
  hostnames.push_back("www.site.com");
  hostnames.push_back("localhost");
  hostnames.push_back("ab:ab:ab:ab:ab:ab");
  ips.push_back("");  // Also valid ip, but not hostname.
  std::vector<int> ports;
  ports.push_back(1);
  ports.push_back(65535);
  for (size_t i = 0; i < ips.size(); ++i) {
    for (size_t j = 0; j < hostnames.size(); ++j) {
      for (size_t k = 0; k < ports.size(); ++k) {
        std::string input = "passport:" + ips[i] + ":" +
            hostnames[j] + ":" + base::IntToString(ports[k]) + ":";
        FetchAndTest(input, "passport", ips[i], hostnames[j], ports[k], true);
      }
    }
  }
}

TEST_F(WebSocketProxyHelperTest, FetchPassportAddrNamePortEmptyPassport) {
  FetchAndTest("::localhost:1:", "", "", "", 0, false);
}

TEST_F(WebSocketProxyHelperTest, FetchPassportAddrNamePortBadIpv6) {
  FetchAndTest("passport:[12:localhost:1:", "", "", "", 0, false);
}

TEST_F(WebSocketProxyHelperTest, FetchPassportAddrNameEmptyHostname) {
  FetchAndTest("passport:::1:", "", "", "", 0, false);
}

TEST_F(WebSocketProxyHelperTest, FetchPassportAddrNameSmallPort) {
  FetchAndTest("passport:::0:", "", "", "", 0, false);
}

TEST_F(WebSocketProxyHelperTest, FetchPassportAddrNameBigPort) {
  FetchAndTest("passport:::65536:", "", "", "", 0, false);
}

TEST_F(WebSocketProxyHelperTest, FetchPassportAddrNoLastColon) {
  FetchAndTest("passport::localhost:1", "", "", "", 0, false);
}

}  // namespace chromeos
