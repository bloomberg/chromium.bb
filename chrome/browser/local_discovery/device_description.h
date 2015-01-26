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
  DeviceDescription();
  explicit DeviceDescription(const ServiceDescription& service_description);
  ~DeviceDescription();

  bool IsValid() const;

  // Display attributes
  std::string name;
  std::string description;

  // Functional attributes
  std::string id;
  std::string type;
  int version;

  // Attributes related to local HTTP
  net::HostPortPair address;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_DEVICE_DESCRIPTION_H_
