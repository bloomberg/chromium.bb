// Copyright 2008, Google Inc.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//  3. Neither the name of Google Inc. nor the names of its contributors may be
//     used to endorse or promote products derived from this software without
//     specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
// WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
// EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#if defined(WIN32) || defined(OS_WINCE)

#include "gears/geolocation/wifi_data_provider_windows_common.h"

#include <assert.h>
#include "gears/base/common/string_utils.h"
#include "gears/geolocation/device_data_provider.h"
#include "gears/geolocation/wifi_data_provider_common.h"

bool ConvertToGearsFormat(const NDIS_WLAN_BSSID &data,
                          AccessPointData *access_point_data) {
  // Currently we get only MAC address, signal strength and SSID.
  // TODO(steveblock): Work out how to get age, channel and signal-to-noise.
  assert(access_point_data);
  access_point_data->mac_address = MacAddressAsString16(data.MacAddress);
  access_point_data->radio_signal_strength = data.Rssi;
  // Note that _NDIS_802_11_SSID::Ssid::Ssid is not null-terminated.
  UTF8ToString16(reinterpret_cast<const char*>(data.Ssid.Ssid),
                 data.Ssid.SsidLength,
                 &access_point_data->ssid);
  return true;
}

int GetDataFromBssIdList(const NDIS_802_11_BSSID_LIST &bss_id_list,
                         int list_size,
                         WifiData::AccessPointDataSet *data) {
  // Walk through the BSS IDs.
  int found = 0;
  const uint8 *iterator = reinterpret_cast<const uint8*>(&bss_id_list.Bssid[0]);
  const uint8 *end_of_buffer =
      reinterpret_cast<const uint8*>(&bss_id_list) + list_size;
  for (int i = 0; i < static_cast<int>(bss_id_list.NumberOfItems); ++i) {
    const NDIS_WLAN_BSSID *bss_id =
        reinterpret_cast<const NDIS_WLAN_BSSID*>(iterator);
    // Check that the length of this BSS ID is reasonable.
    if (bss_id->Length < sizeof(NDIS_WLAN_BSSID) ||
        iterator + bss_id->Length > end_of_buffer) {
      break;
    }
    AccessPointData access_point_data;
    if (ConvertToGearsFormat(*bss_id, &access_point_data)) {
      data->insert(access_point_data);
      ++found;
    }
    // Move to the next BSS ID.
    iterator += bss_id->Length;
  }
  return found;
}

#endif  // WIN32 || OS_WINCE
