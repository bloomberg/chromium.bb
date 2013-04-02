// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/network_change_notifier_chromeos.h"

#include <string>

#include "base/basictypes.h"
#include "base/strings/string_split.h"
#include "chromeos/network/network_change_notifier_factory_chromeos.h"
#include "chromeos/network/network_state.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char kDnsServers1[] = "192.168.0.1,192.168.0.2";
const char kDnsServers2[] = "192.168.3.1,192.168.3.2";
const char kIpAddress1[] = "192.168.1.1";
const char kIpAddress2[] = "192.168.1.2";
const char kService1[] = "/service/1";
const char kService2[] = "/service/2";
const char kService3[] = "/service/3";

struct NotifierState {
  net::NetworkChangeNotifier::ConnectionType type;
  const char* service_path;
  const char* ip_address;
  const char* dns_servers;
};

struct DefaultNetworkState {
  bool is_connected;
  const char* type;
  const char* technology;
  const char* service_path;
  const char* ip_address;
  const char* dns_servers;
};

struct NotifierUpdateTestCase {
  const char* test_description;
  NotifierState initial_state;
  DefaultNetworkState default_network_state;
  NotifierState expected_state;
  bool expected_type_changed;
  bool expected_ip_changed;
  bool expected_dns_changed;
};

} // namespace

using net::NetworkChangeNotifier;

TEST(NetworkChangeNotifierChromeosTest, ConnectionTypeFromShill) {
  struct TypeMapping {
    const char* shill_type;
    const char* technology;
    NetworkChangeNotifier::ConnectionType connection_type;
  };
  TypeMapping type_mappings[] = {
    { flimflam::kTypeEthernet, "", NetworkChangeNotifier::CONNECTION_ETHERNET },
    { flimflam::kTypeWifi, "", NetworkChangeNotifier::CONNECTION_WIFI },
    { flimflam::kTypeWimax, "", NetworkChangeNotifier::CONNECTION_4G },
    { "unknown type", "unknown technology",
      NetworkChangeNotifier::CONNECTION_UNKNOWN },
    { flimflam::kTypeCellular, flimflam::kNetworkTechnology1Xrtt,
      NetworkChangeNotifier::CONNECTION_2G },
    { flimflam::kTypeCellular, flimflam::kNetworkTechnologyGprs,
      NetworkChangeNotifier::CONNECTION_2G },
    { flimflam::kTypeCellular, flimflam::kNetworkTechnologyEdge,
      NetworkChangeNotifier::CONNECTION_2G },
    { flimflam::kTypeCellular, flimflam::kNetworkTechnologyEvdo,
      NetworkChangeNotifier::CONNECTION_3G },
    { flimflam::kTypeCellular, flimflam::kNetworkTechnologyGsm,
      NetworkChangeNotifier::CONNECTION_3G },
    { flimflam::kTypeCellular, flimflam::kNetworkTechnologyUmts,
      NetworkChangeNotifier::CONNECTION_3G },
    { flimflam::kTypeCellular, flimflam::kNetworkTechnologyHspa,
      NetworkChangeNotifier::CONNECTION_3G },
    { flimflam::kTypeCellular,  flimflam::kNetworkTechnologyHspaPlus,
      NetworkChangeNotifier::CONNECTION_4G },
    { flimflam::kTypeCellular, flimflam::kNetworkTechnologyLte,
      NetworkChangeNotifier::CONNECTION_4G },
    { flimflam::kTypeCellular, flimflam::kNetworkTechnologyLteAdvanced,
      NetworkChangeNotifier::CONNECTION_4G },
    { flimflam::kTypeCellular, "unknown technology",
      NetworkChangeNotifier::CONNECTION_2G }
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(type_mappings); ++i) {
    NetworkChangeNotifier::ConnectionType type =
        NetworkChangeNotifierChromeos::ConnectionTypeFromShill(
            type_mappings[i].shill_type, type_mappings[i].technology);
    EXPECT_EQ(type_mappings[i].connection_type, type);
  }
}

class NetworkChangeNotifierChromeosUpdateTest : public testing::Test {
 protected:
  NetworkChangeNotifierChromeosUpdateTest() : default_network_("") {
  }
  virtual ~NetworkChangeNotifierChromeosUpdateTest() {}

  void SetNotifierState(const NotifierState& notifier_state) {
    notifier_.connection_type_ = notifier_state.type;
    notifier_.service_path_ = notifier_state.service_path;
    notifier_.ip_address_ = notifier_state.ip_address;
    std::vector<std::string> dns_servers;
    base::SplitString(notifier_state.dns_servers, ',', &dns_servers);
    notifier_.dns_servers_ = dns_servers;
  }

  void VerifyNotifierState(const NotifierState& notifier_state) {
    EXPECT_EQ(notifier_state.type, notifier_.connection_type_);
    EXPECT_EQ(notifier_state.service_path, notifier_.service_path_);
    EXPECT_EQ(notifier_state.ip_address, notifier_.ip_address_);
    std::vector<std::string> dns_servers;
    base::SplitString(notifier_state.dns_servers, ',', &dns_servers);
    EXPECT_EQ(dns_servers, notifier_.dns_servers_);
  }

  // Sets the default network state used for notifier updates.
  void SetDefaultNetworkState(
      const DefaultNetworkState& default_network_state) {
    if (default_network_state.is_connected)
      default_network_.connection_state_ = flimflam::kStateOnline;
    else
      default_network_.connection_state_ = flimflam::kStateConfiguration;
    default_network_.type_ = default_network_state.type;
    default_network_.technology_ = default_network_state.technology;
    default_network_.path_ = default_network_state.service_path;
    default_network_.set_ip_address(default_network_state.ip_address);
    std::vector<std::string> dns_servers;
    base::SplitString(default_network_state.dns_servers, ',', &dns_servers);
    default_network_.set_dns_servers(dns_servers);
  }

  // Process an default network update based on the state of |default_network_|.
  void ProcessDefaultNetworkUpdate(bool* type_changed,
                                   bool* ip_changed,
                                   bool* dns_changed) {
    notifier_.UpdateState(&default_network_, type_changed, ip_changed,
                          dns_changed);
  }

 private:
  NetworkState default_network_;
  NetworkChangeNotifierChromeos notifier_;
};

NotifierUpdateTestCase test_cases[] = {
  { "Online -> Offline",
    { NetworkChangeNotifier::CONNECTION_ETHERNET, kService1, kIpAddress1,
      kDnsServers1 },
    { false, flimflam::kTypeEthernet, "", kService1, "", "" },
    { NetworkChangeNotifier::CONNECTION_NONE, "", "", "" },
    true, true, true
  },
  { "Offline -> Offline",
    { NetworkChangeNotifier::CONNECTION_NONE, "", "", "" },
    { false, flimflam::kTypeEthernet, "", kService1, kIpAddress1,
      kDnsServers1 },
    { NetworkChangeNotifier::CONNECTION_NONE, "", "", "" },
    false, false, false
  },
  { "Offline -> Online",
    { NetworkChangeNotifier::CONNECTION_NONE, "", "", "" },
    { true, flimflam::kTypeEthernet, "", kService1, kIpAddress1, kDnsServers1 },
    { NetworkChangeNotifier::CONNECTION_ETHERNET, kService1, kIpAddress1,
      kDnsServers1 },
    true, true, true
  },
  { "Online -> Online (new default service, different connection type)",
    { NetworkChangeNotifier::CONNECTION_ETHERNET, kService1, kIpAddress1,
      kDnsServers1 },
    { true, flimflam::kTypeWifi, "", kService2, kIpAddress1, kDnsServers1 },
    { NetworkChangeNotifier::CONNECTION_WIFI, kService2, kIpAddress1,
      kDnsServers1 },
    true, true, true
  },
  { "Online -> Online (new default service, same connection type)",
    { NetworkChangeNotifier::CONNECTION_WIFI, kService2, kIpAddress1,
      kDnsServers1 },
    { true, flimflam::kTypeWifi, "", kService3, kIpAddress1, kDnsServers1 },
    { NetworkChangeNotifier::CONNECTION_WIFI, kService3, kIpAddress1,
      kDnsServers1 },
    false, true, true
  },
  { "Online -> Online (same default service, new IP address, same DNS)",
    { NetworkChangeNotifier::CONNECTION_WIFI, kService3, kIpAddress1,
      kDnsServers1 },
    { true, flimflam::kTypeWifi, "", kService3, kIpAddress2, kDnsServers1 },
    { NetworkChangeNotifier::CONNECTION_WIFI, kService3, kIpAddress2,
      kDnsServers1 },
    false, true, false
  },
  { "Online -> Online (same default service, same IP address, new DNS)",
    { NetworkChangeNotifier::CONNECTION_WIFI, kService3, kIpAddress2,
      kDnsServers1 },
    { true, flimflam::kTypeWifi, "", kService3, kIpAddress2, kDnsServers2 },
    { NetworkChangeNotifier::CONNECTION_WIFI, kService3, kIpAddress2,
      kDnsServers2 },
    false, false, true
  }
};

TEST_F(NetworkChangeNotifierChromeosUpdateTest, UpdateDefaultNetwork) {
  for (size_t i = 0; i < arraysize(test_cases); ++i) {
    SCOPED_TRACE(test_cases[i].test_description);
    SetNotifierState(test_cases[i].initial_state);
    SetDefaultNetworkState(test_cases[i].default_network_state);
    bool type_changed = false, ip_changed = false, dns_changed = false;
    ProcessDefaultNetworkUpdate(&type_changed, &ip_changed, &dns_changed);
    VerifyNotifierState(test_cases[i].expected_state);
    EXPECT_TRUE(type_changed == test_cases[i].expected_type_changed);
    EXPECT_TRUE(ip_changed == test_cases[i].expected_ip_changed);
    EXPECT_TRUE(dns_changed == test_cases[i].expected_dns_changed);
  }
}

}  // namespace chromeos
