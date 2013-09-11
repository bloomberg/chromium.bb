// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MDNS_DNS_SD_REGISTRY_H_
#define CHROME_BROWSER_EXTENSIONS_API_MDNS_DNS_SD_REGISTRY_H_

#include <string>
#include <vector>

#include "base/observer_list.h"

namespace extensions {

// Network Service Discovery registry class for keeping track of discovered
// network services.
class DnsSdRegistry {
 public:
  typedef std::vector<std::string> DnsSdServiceList;

  class DnsSdObserver {
   public:
    virtual void OnDnsSdEvent(const std::string& service_type,
                              const DnsSdServiceList& services) = 0;

   protected:
    virtual ~DnsSdObserver() {}
  };

  explicit DnsSdRegistry();
  virtual ~DnsSdRegistry();

  // Observer registration for parties interested in discovery events.
  virtual void AddObserver(DnsSdObserver* observer);
  virtual void RemoveObserver(DnsSdObserver* observer);

  // DNS-SD-related discovery functionality.
  virtual void RegisterDnsSdListener(std::string service_type);
  virtual void UnregisterDnsSdListener(std::string service_type);

 protected:
  ObserverList<DnsSdObserver> observers_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MDNS_DNS_SD_REGISTRY_H_
