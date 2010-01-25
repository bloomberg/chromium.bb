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

#include "gears/geolocation/wifi_data_provider_common.h"

#include <assert.h>

#if defined(WIN32) || defined(OS_MACOSX)

#if defined(WIN32)
#include <windows.h>
#elif defined(OS_MACOSX)
#include <CoreFoundation/CoreFoundation.h>
#include "gears/base/common/string_utils_osx.h"
#include "gears/base/safari/scoped_cf.h"
#include "third_party/scoped_ptr/scoped_ptr.h"  // For scoped_array
#endif

std::string16 MacAddressAsString16(const uint8 mac_as_int[6]) {
  // mac_as_int is big-endian. Write in byte chunks.
  // Format is XX-XX-XX-XX-XX-XX.
  static const char16 *kMacFormatString =
      STRING16(L"%02x-%02x-%02x-%02x-%02x-%02x");
#ifdef OS_MACOSX
  scoped_cftype<CFStringRef> format_string(CFStringCreateWithCharacters(
      kCFAllocatorDefault,
      kMacFormatString,
      std::char_traits<char16>::length(kMacFormatString)));
  scoped_cftype<CFStringRef> mac_string(CFStringCreateWithFormat(
      kCFAllocatorDefault,
      NULL,  // not implemented
      format_string.get(),
      mac_as_int[0], mac_as_int[1], mac_as_int[2],
      mac_as_int[3], mac_as_int[4], mac_as_int[5]));
  std::string16 mac;
  CFStringRefToString16(mac_string.get(), &mac);
  return mac;
#else
  char16 mac[18];
#ifdef DEBUG
  int num_characters =
#endif
      wsprintf(mac, kMacFormatString,
               mac_as_int[0], mac_as_int[1], mac_as_int[2],
               mac_as_int[3], mac_as_int[4], mac_as_int[5]);
  assert(num_characters == 17);
  return mac;
#endif
}

#endif  // WIN32 || OS_MACOSX

#if defined(WIN32) || defined(OS_MACOSX) || defined(LINUX)

// These constants are defined for each platfrom in wifi_data_provider_xxx.cc.
extern const int kDefaultPollingInterval;
extern const int kNoChangePollingInterval;
extern const int kTwoNoChangePollingInterval;

int UpdatePollingInterval(int polling_interval, bool scan_results_differ) {
  if (scan_results_differ) {
    return kDefaultPollingInterval;
  }
  if (polling_interval == kDefaultPollingInterval) {
    return kNoChangePollingInterval;
  } else {
    assert(polling_interval == kNoChangePollingInterval ||
           polling_interval == kTwoNoChangePollingInterval);
    return kTwoNoChangePollingInterval;
  }
}

#endif  // WIN32 || OS_MACOSX || LINUX
