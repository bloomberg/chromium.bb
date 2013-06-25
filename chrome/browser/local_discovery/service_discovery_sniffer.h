// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_SNIFFER_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_SNIFFER_H_

#include <map>
#include <string>

#include "base/memory/linked_ptr.h"
#include "chrome/browser/local_discovery/service_discovery_client.h"

namespace local_discovery {

class Service {
 public:
  explicit Service(std::string service_name);
  ~Service();

  void Added();
  void Changed();
  void Removed();

 private:
  void OnServiceResolved(ServiceResolver::RequestStatus status,
                         const ServiceDescription& service);

  bool changed_;
  scoped_ptr<ServiceResolver> service_resolver_;
};

class ServiceType : public ServiceWatcher::Delegate {
 public:
  explicit ServiceType(std::string service_type);
  virtual ~ServiceType();

  virtual void OnServiceUpdated(ServiceWatcher::UpdateType,
                                const std::string& service_name) OVERRIDE;

 private:
  typedef std::map<std::string, linked_ptr<Service> > ServiceMap;

  ServiceMap services_;
  scoped_ptr<ServiceWatcher> watcher_;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_SNIFFER_H_
