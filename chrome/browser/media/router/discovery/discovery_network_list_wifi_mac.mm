// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/discovery_network_list_wifi.h"

#include <CoreWLAN/CoreWLAN.h>
#include <string.h>

#include <utility>

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"

bool MaybeGetWifiSSID(const std::string& if_name, std::string* ssid_out) {
  DCHECK(ssid_out);

  NSString* ns_ifname = base::SysUTF8ToNSString(if_name.data());
  CWInterface* interface = [CWInterface interfaceWithName:ns_ifname];
  if (interface == nil) {
    return false;
  }
  std::string ssid(base::SysNSStringToUTF8([interface ssid]));
  if (ssid.empty()) {
    return false;
  }
  ssid_out->assign(std::move(ssid));
  return true;
}
