// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_DEVICE_DESCRIPTION_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_DEVICE_DESCRIPTION_H_

#include <string>

#include "base/time/time.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_util.h"

namespace local_discovery {

struct ServiceDescription;

struct DeviceDescription {
  enum ConnectionState {
    ONLINE,
    OFFLINE,
    CONNECTING,
    NOT_CONFIGURED,
    UNKNOWN
  };

  DeviceDescription();
  ~DeviceDescription();

  void FillFromServiceDescription(
      const ServiceDescription& service_description);

  // Display attributes
  std::string name;
  std::string description;

  // Functional attributes
  std::string url;
  std::string id;
  std::string type;
  int version;
  ConnectionState connection_state;

  // Attributes related to local HTTP
  net::HostPortPair address;
  net::IPAddressNumber ip_address;
  base::Time last_seen;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_DEVICE_DESCRIPTION_H_
