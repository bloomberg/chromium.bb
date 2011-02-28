// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/geolocation/gateway_data_provider_win.h"

#include <iphlpapi.h>
#include <winsock2.h>

#include <set>

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_switches.h"
#include "content/browser/geolocation/empty_device_data_provider.h"

namespace {

string16 MacAsString16(const uint8 mac_as_int[6]) {
  // mac_as_int is big-endian. Write in byte chunks.
  // Format is XX-XX-XX-XX-XX-XX.
  static const wchar_t* const kMacFormatString =
      L"%02x-%02x-%02x-%02x-%02x-%02x";
  return WideToUTF16(StringPrintf(kMacFormatString,
                                  mac_as_int[0], mac_as_int[1],
                                  mac_as_int[2], mac_as_int[3],
                                  mac_as_int[4], mac_as_int[5]));
}

void FetchGatewayIps(std::set<IPAddr>* gateway_ips) {
  ULONG out_buf_len = 0;
  if (GetAdaptersInfo(NULL, &out_buf_len) != ERROR_BUFFER_OVERFLOW)
    return;
  scoped_ptr_malloc<IP_ADAPTER_INFO> adapter_list(
      static_cast<IP_ADAPTER_INFO*>(malloc(out_buf_len)));
  if (adapter_list == NULL)
    return;
  if (GetAdaptersInfo(adapter_list.get(), &out_buf_len) == NO_ERROR) {
    for (const IP_ADAPTER_INFO* adapter = adapter_list.get(); adapter != NULL;
         adapter = adapter->Next) {
      if (adapter->Type == IF_TYPE_ETHERNET_CSMACD) {
        // If we have multiple gateways with conflicting IPs we'll disregard
        // the duplicates.
        for (const IP_ADDR_STRING* gw = &adapter->GatewayList;
             gw != NULL; gw = gw->Next)
          gateway_ips->insert(inet_addr(gw->IpAddress.String));
      }
    }
  }
}

// Sends an ARP request to determine the MAC address of the destination.
bool GetMacFromIp(IPAddr dest_ip, ULONG* mac_addr, size_t mac_addr_size) {
  memset(mac_addr, 0xff, mac_addr_size);
  DWORD arp_result = 0;
  ULONG phys_addr_len = 6;
  IPAddr src_ip = 0;
  arp_result = SendARP(dest_ip, src_ip, mac_addr, &phys_addr_len);
  return arp_result == NO_ERROR && phys_addr_len == 6;
}

} // namespace

class WinGatewayApi : public GatewayDataProviderCommon::GatewayApiInterface {
 public:
  WinGatewayApi();
  virtual ~WinGatewayApi();

  // GatewayApiInterface
  virtual bool GetRouterData(GatewayData::RouterDataSet* data);
};

GatewayDataProviderCommon::GatewayApiInterface*
WinGatewayDataProvider::NewGatewayApi() {
  return new WinGatewayApi();
}

WinGatewayApi::WinGatewayApi() {
}

WinGatewayApi::~WinGatewayApi() {
}

bool WinGatewayApi::GetRouterData(GatewayData::RouterDataSet* data) {
  std::set<IPAddr> gateway_ips;
  FetchGatewayIps(&gateway_ips);
  if (gateway_ips.empty())
    return false;
  const size_t kMacAddrLength = 2;
  for (std::set<IPAddr>::const_iterator gateway_it =
       gateway_ips.begin(); gateway_it != gateway_ips.end(); ++gateway_it) {
    IPAddr adapter_gateway_ip = *gateway_it;
    ULONG mac_addr[kMacAddrLength];
    if (GetMacFromIp(adapter_gateway_ip, mac_addr, sizeof(mac_addr))) {
      RouterData router;
      uint8* phys_addr = reinterpret_cast<uint8*>(&mac_addr);
      router.mac_address = MacAsString16(phys_addr);
      data->insert(router);
    }
  }
  return true;
}

WinGatewayDataProvider::WinGatewayDataProvider() {
}

WinGatewayDataProvider::~WinGatewayDataProvider() {
}

template<>
GatewayDataProviderImplBase* GatewayDataProvider::DefaultFactoryFunction() {
  if (!CommandLine::ForCurrentProcess()
      ->HasSwitch(switches::kExperimentalLocationFeatures))
    return new EmptyDeviceDataProvider<GatewayData>();
  return new WinGatewayDataProvider();
}
