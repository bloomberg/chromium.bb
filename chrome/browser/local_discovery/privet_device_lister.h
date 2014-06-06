// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_DEVICE_LISTER_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_DEVICE_LISTER_H_

#include <string>

#include "base/callback.h"
#include "base/time/time.h"
#include "chrome/browser/local_discovery/device_description.h"
#include "net/base/host_port_pair.h"
#include "net/base/net_util.h"

namespace local_discovery {

class PrivetDeviceLister {
 public:
  PrivetDeviceLister();
  virtual ~PrivetDeviceLister();

  class Delegate {
   public:
    virtual void DeviceChanged(bool added,
                               const std::string& name,
                               const DeviceDescription& description) = 0;
    virtual void DeviceRemoved(const std::string& name) = 0;
    virtual void DeviceCacheFlushed() = 0;
  };

  // Start the PrivetServiceLister.
  virtual void Start() = 0;

  virtual void DiscoverNewDevices(bool force_update) = 0;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_PRIVET_DEVICE_LISTER_H_
