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
//
// WiFi card drivers for Linux implement the Wireless Extensions interface.
// This interface is part of the Linux kernel.
//
// Various sets of tools are available to manipulate the Wireless Extensions,
// of which Wireless Tools is the default implementation. Wireless Tools
// provides a C++ library (libiw) as well as a set of command line tools
// (iwconfig, iwlist etc). See
// http://www.hpl.hp.com/personal/Jean_Tourrilhes/Linux/Tools.html for details.
//
// Ideally, we would use libiw to obtain WiFi data. However, Wireless Tools is
// released under GPL, which is not compatible with Gears. Furthermore, little
// documentation is available for Wireless Extensions, so replicating libiw
// without copying it directly would be difficult.
//
// We therefore simply invoke iwlist (one of the Wireless Tools command line
// tools) and parse the output. Sample output is shown below.
//
// lo        Interface doesn't support scanning.
//
// ath0      Scan completed :
//           Cell 01 - Address: 00:24:86:11:4C:42
//                     ESSID:"Test SSID"
//                     Mode:Master
//                     Frequency:2.427 GHz (Channel 4)
//                     Quality=5/94  Signal level=-90 dBm  Noise level=-95 dBm
//                     Encryption key:off
//                     Bit Rates:1 Mb/s; 2 Mb/s; 5 Mb/s; 6 Mb/s; 9 Mb/s
//                               11 Mb/s; 12 Mb/s; 18 Mb/s
//                     Extra:bcn_int=100
//           Cell 02 - Address: 00:24:86:11:6F:E2
//                     ESSID:"Test SSID"
//                     Mode:Master
//                     Frequency:2.447 GHz (Channel 8)
//                     Quality=4/94  Signal level=-91 dBm  Noise level=-95 dBm
//                     Encryption key:off
//                     Bit Rates:1 Mb/s; 2 Mb/s; 5 Mb/s; 6 Mb/s; 9 Mb/s
//                               11 Mb/s; 12 Mb/s; 18 Mb/s
//                     Extra:bcn_int=100
//
// TODO(steveblock): Investigate the possibility of the author of Wireless Tools
// releasing libiw under a Gears-compatible license.

// TODO(cprince): remove platform-specific #ifdef guards when OS-specific
// sources (e.g. WIN32_CPPSRCS) are implemented
#if defined(LINUX) && !defined(OS_MACOSX)

#include "gears/geolocation/wifi_data_provider_linux.h"

#include <ctype.h>  // For isxdigit()
#include <stdio.h>
#include "gears/base/common/string_utils.h"
#include "gears/geolocation/wifi_data_provider_common.h"

// The time periods, in milliseconds, between successive polls of the wifi data.
extern const int kDefaultPollingInterval = 10000;  // 10s
extern const int kNoChangePollingInterval = 120000;  // 2 mins
extern const int kTwoNoChangePollingInterval = 600000;  // 10 mins

// Local function
static bool GetAccessPointData(WifiData::AccessPointDataSet *access_points);

// static
template<>
WifiDataProviderImplBase *WifiDataProvider::DefaultFactoryFunction() {
  return new LinuxWifiDataProvider();
}


LinuxWifiDataProvider::LinuxWifiDataProvider()
    : is_first_scan_complete_(false) {
  Start();
}

LinuxWifiDataProvider::~LinuxWifiDataProvider() {
  stop_event_.Signal();
  Join();
}

bool LinuxWifiDataProvider::GetData(WifiData *data) {
  assert(data);
  MutexLock lock(&data_mutex_);
  *data = wifi_data_;
  // If we've successfully completed a scan, indicate that we have all of the
  // data we can get.
  return is_first_scan_complete_;
}

// Thread implementation
void LinuxWifiDataProvider::Run() {
  // Regularly get the access point data.
  int polling_interval = kDefaultPollingInterval;
  do {
    WifiData new_data;
    if (GetAccessPointData(&new_data.access_point_data)) {
      bool update_available;
      data_mutex_.Lock();
      update_available = wifi_data_.DiffersSignificantly(new_data);
      wifi_data_ = new_data;
      data_mutex_.Unlock();
      polling_interval =
          UpdatePollingInterval(polling_interval, update_available);
      if (update_available) {
        is_first_scan_complete_ = true;
        NotifyListeners();
      }
    }
  } while (!stop_event_.WaitWithTimeout(polling_interval));
}

// Local functions

static bool IsValidMacAddress(const char *mac_address) {
  return isxdigit(mac_address[0]) &&
         isxdigit(mac_address[1]) &&
         mac_address[2] == ':' &&
         isxdigit(mac_address[3]) &&
         isxdigit(mac_address[4]) &&
         mac_address[5] == ':' &&
         isxdigit(mac_address[6]) &&
         isxdigit(mac_address[7]) &&
         mac_address[8] == ':' &&
         isxdigit(mac_address[9]) &&
         isxdigit(mac_address[10]) &&
         mac_address[11] == ':' &&
         isxdigit(mac_address[12]) &&
         isxdigit(mac_address[13]) &&
         mac_address[14] == ':' &&
         isxdigit(mac_address[15]) &&
         isxdigit(mac_address[16]);
}

static void ParseLine(const std::string &line,
                      const std::string &mac_address_string,
                      const std::string &ssid_string,
                      const std::string &signal_strength_string,
                      AccessPointData *access_point_data) {
  // Currently we get only MAC address, SSID and signal strength.
  // TODO(steveblock): Work out how to get age, channel and signal-to-noise.
  std::string::size_type index;
  if ((index = line.find(mac_address_string)) != std::string::npos) {
    // MAC address
    if (IsValidMacAddress(&line.at(index + mac_address_string.size()))) {
      UTF8ToString16(&line.at(index + mac_address_string.size()),
                     17,  // XX:XX:XX:XX:XX:XX
                     &access_point_data->mac_address);
    }
  } else if ((index = line.find(ssid_string)) != std::string::npos) {
    // SSID
    // The string should be quoted.
    std::string::size_type start = index + ssid_string.size() + 1;
    std::string::size_type end = line.find('\"', start);
    // If we can't find a trailing quote, something has gone wrong.
    if (end != std::string::npos) {
      UTF8ToString16(&line.at(start), end - start, &access_point_data->ssid);
    }
  } else if ((index = line.find(signal_strength_string)) != std::string::npos) {
    // Signal strength
    // iwlist will convert to dBm if it can. If it has failed to do so, we can't
    // make use of the data.
    if (line.find("dBm") != std::string::npos) {
      // atoi will ignore trailing non-numeric characters
      access_point_data->radio_signal_strength =
          atoi(&line.at(index + signal_strength_string.size()));
    }
  }
}

static void ParseAccessPoint(const std::string &text,
                             const std::string &mac_address_string,
                             const std::string &ssid_string,
                             const std::string &signal_strength_string,
                             AccessPointData *access_point_data) {
  // Split response into lines to aid parsing.
  std::string::size_type start = 0;
  std::string::size_type end;
  do {
    end = text.find('\n', start);
    std::string::size_type length = (end == std::string::npos) ?
                                    std::string::npos : end - start;
    ParseLine(text.substr(start, length),
              mac_address_string,
              ssid_string,
              signal_strength_string,
              access_point_data);
    start = end + 1;
  } while (end != std::string::npos);
}

// Issues the specified command, and parses the response. Data for each access
// point is separated by the given delimiter. Within each block of data, the
// repsonse is split into lines and data is extracted by searching for the MAC
// address, SSID and signal strength strings.
bool IssueCommandAndParseResult(const char *command,
                                const char *delimiter,
                                const std::string &mac_address_string,
                                const std::string &ssid_string,
                                const std::string &signal_strength_string,
                                WifiData::AccessPointDataSet *access_points) {
  // Open pipe in read mode.
  FILE *result_pipe = popen(command, "r");
  if (result_pipe == NULL) {
    LOG(("IssueCommand(): Failed to open pipe.\n"));
    return false;
  }

  // Read results of command.
  static const int kBufferSize = 1024;
  char buffer[kBufferSize];
  size_t bytes_read;
  std::string result;
  do {
    bytes_read = fread(buffer, 1, kBufferSize, result_pipe);
    result.append(buffer, bytes_read);
  } while (static_cast<int>(bytes_read) == kBufferSize);
  pclose(result_pipe);


  // Parse results.
  assert(access_points);
  access_points->clear();
  std::string::size_type start = result.find(delimiter);
  while (start != std::string::npos) {
    std::string::size_type end = result.find(delimiter, start + 1);
    std::string::size_type length = (end == std::string::npos) ?
                                    std::string::npos : end - start;
    AccessPointData access_point_data;
    ParseAccessPoint(result.substr(start, length),
                     mac_address_string,
                     ssid_string,
                     signal_strength_string,
                     &access_point_data);
    access_points->insert(access_point_data);
    start = end;
  }

  return !access_points->empty();
}

static bool GetAccessPointData(WifiData::AccessPointDataSet *access_points) {
  return IssueCommandAndParseResult("iwlist scan 2> /dev/null",
                                    "Cell ",
                                    "Address: ",
                                    "ESSID:",
                                    "Signal level=",
                                    access_points) ||
         IssueCommandAndParseResult("iwconfig 2> /dev/null",
                                    "ESSID:\"",
                                    "Access Point: ",
                                    "ESSID:",
                                    "Signal level=",
                                    access_points);
}

#endif  // LINUX && !OS_MACOSX
