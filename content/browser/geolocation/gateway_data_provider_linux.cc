// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Provides MAC addresses of connected routers by using the /proc/net/
// directory which contains files with network information.
// This directory is used in most Linux based operating systems.

#include "content/browser/geolocation/gateway_data_provider_linux.h"

#include <algorithm>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <set>
#include <string>
#include <sys/ioctl.h>
#include <vector>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/string_number_conversions.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "content/browser/geolocation/empty_device_data_provider.h"
#include "content/browser/geolocation/gateway_data_provider_common.h"

namespace {
const unsigned int kMaxArpIterations = 30;
const unsigned int kMaxRouteIterations = 20;
const char kNoGateway[] = "00000000";
const char kArpFilePath[] = "/proc/net/arp";
const char kRouteFilePath[] = "/proc/net/route";

// Converts an IP address held in a little endian paired hex format to period
// separated decimal format. For example 0101A8C0 becomes 192.168.1.1
std::string HexToIpv4Format(std::string& hex_address) {
  if (hex_address.size() != 8)
    return "";
  std::vector<uint8> bytes;
  if (!base::HexStringToBytes(hex_address, &bytes) ||
      bytes.size() != 4) {
    return "";
  }
  struct in_addr in;
  uint32 ip_native_endian;
  memcpy(&ip_native_endian, &bytes[0], 4);
  in.s_addr = htonl(ip_native_endian);
  return inet_ntoa(in);
}

// TODO(joth): Cache the sets of gateways and MAC addresses to avoid reading
// through the arp table when the routing table hasn't changed.
class LinuxGatewayApi : public GatewayDataProviderCommon::GatewayApiInterface {
 public:
  LinuxGatewayApi() {}
  virtual ~LinuxGatewayApi() {}

  static LinuxGatewayApi* Create() {
    return new LinuxGatewayApi();
  }

  // GatewayApiInterface
  virtual bool GetRouterData(GatewayData::RouterDataSet* data);

 private:
  DISALLOW_COPY_AND_ASSIGN(LinuxGatewayApi);
};

// Gets a set of gateways using data from /proc/net/route
// Returns false if we cannot read the route file or we cannot classify the
// type of network adapter.
// Routing data is held in the following format
// Note: "|" represents a delimeter.
// Iface|Destination|Gateway|Flags|RefCnt|Use|Metric|Mask|MTU|Window|IRTT
// The delimiter for this table is "\t".
// ioctl calls are used to verify the type of network adapter, using interface
// names from the route table.
bool GetGateways(std::set<std::string>* gateways) {
  FilePath route_file_path(kRouteFilePath);
  std::string route_file;
  if (!file_util::ReadFileToString(route_file_path, &route_file)) {
    // Unable to read file.
    return false;
  }
  StringTokenizer tokenized_route_file(route_file, "\n");
  std::vector<std::string> route_data;
  // Remove column labels.
  tokenized_route_file.GetNext();

  // Get rows of data.
  while (tokenized_route_file.GetNext() &&
         route_data.size() < kMaxRouteIterations)
    route_data.push_back(tokenized_route_file.token());

  if (route_data.empty()) {
    // Having no data is not an error case.
    return true;
  }

  int sock;
  sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == -1)
    return false;
  struct ifreq network_device;

  for (size_t j = 0; j < route_data.size(); ++j) {
    // Get gateway address.
    std::string gateway;
    StringTokenizer tokenized_route_data(route_data[j], "\t");
    // Check if device is an Ethernet device.
    if (!tokenized_route_data.GetNext()) // Get Iface.
      continue;
    // Ensure space for the null term too.
    if (tokenized_route_data.token().size() >=
        arraysize(network_device.ifr_name))
      continue;
    memset(&network_device, 0, sizeof(network_device));
    base::strlcpy(network_device.ifr_name,
                  tokenized_route_data.token().c_str(),
                  arraysize(network_device.ifr_name));
    if (ioctl(sock, SIOCGIFHWADDR,
        reinterpret_cast<char*>(&network_device)) < 0) {
      // Could not get device type.
      continue;
    }
    if (network_device.ifr_hwaddr.sa_family != ARPHRD_ETHER) {
      // Device is not an Ethernet device.
      continue;
    }
    if (!tokenized_route_data.GetNext()) // Skip Destination.
          continue;
    if (!tokenized_route_data.GetNext()) // Get Gateway.
          continue;
    gateway = tokenized_route_data.token();
    if (gateway != kNoGateway) {
      // TODO(joth): Currently only works for gateways using IPv4.
      std::string gateway_int_format = HexToIpv4Format(gateway);
      if (!gateway_int_format.empty())
        gateways->insert(gateway_int_format);
    }
  }

  close(sock);
  return true;
}

// Gets a RouterDataSet containing MAC addresses related to any connected
// routers. Returns false if we cannot read the arp file.
// The /proc/net/arp file contains arp data in the following format
// Note: "|" represents a delimeter.
// IP address|HW type|Flags|HW address|Mask|Device
// The delimiter for this table is " ".
bool GetMacAddresses(GatewayData::RouterDataSet* data,
                     std::set<std::string>* gateways) {
  // Find MAC addresses of routers
  FilePath arp_file_path(kArpFilePath);
  std::string arp_file_out;
  if (!file_util::ReadFileToString(arp_file_path, &arp_file_out)) {
    // Unable to read file.
    return false;
  }
  StringTokenizer tokenized_arp_file(arp_file_out, "\n");
  std::vector<std::string> arp_data;
  // Remove column labels.
  tokenized_arp_file.GetNext();

  // Get rows of data.
  while (tokenized_arp_file.GetNext() && arp_data.size() < kMaxArpIterations)
    arp_data.push_back(tokenized_arp_file.token());

  for (size_t k = 0; k < arp_data.size(); k++) {
    std::string ip_address;
    StringTokenizer tokenized_arp_data(arp_data[k], " ");
    if (!tokenized_arp_data.GetNext()) // Get IP address.
      continue;
    ip_address = tokenized_arp_data.token();
    if (gateways->find(ip_address) == gateways->end())
      continue;
    if (!tokenized_arp_data.GetNext()) // Skip HW type.
      continue;
    if (!tokenized_arp_data.GetNext()) // Skip flags.
      continue;
    if (!tokenized_arp_data.GetNext()) // Get HW address.
      continue;
    RouterData router;
    std::string mac_add;
    mac_add = tokenized_arp_data.token();
    std::replace(mac_add.begin(), mac_add.end(), ':', '-');
    router.mac_address = UTF8ToUTF16(mac_add);
    data->insert(router);
  }

  return true;
}

bool LinuxGatewayApi::GetRouterData(GatewayData::RouterDataSet* data) {
  std::set<std::string> gateways;
  if (!GetGateways(&gateways))
    return false;
  if (gateways.empty())
    return true;
  return GetMacAddresses(data, &gateways);
}
} // namespace

// static
template<>
GatewayDataProviderImplBase* GatewayDataProvider::DefaultFactoryFunction() {
  if (!CommandLine::ForCurrentProcess()
      ->HasSwitch(switches::kExperimentalLocationFeatures))
    return new EmptyDeviceDataProvider<GatewayData>();
  return new GatewayDataProviderLinux();
}

GatewayDataProviderLinux::GatewayDataProviderLinux() {
}

GatewayDataProviderLinux::~GatewayDataProviderLinux() {
}

GatewayDataProviderCommon::GatewayApiInterface*
    GatewayDataProviderLinux::NewGatewayApi() {
  return LinuxGatewayApi::Create();
}
