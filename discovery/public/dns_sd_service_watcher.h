// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_PUBLIC_DNS_SD_SERVICE_WATCHER_H_
#define DISCOVERY_PUBLIC_DNS_SD_SERVICE_WATCHER_H_

#include "discovery/dnssd/public/dns_sd_instance_record.h"
#include "discovery/dnssd/public/dns_sd_querier.h"
#include "discovery/dnssd/public/dns_sd_service.h"
#include "platform/base/error.h"

namespace openscreen {
namespace discovery {

// This class represents a top-level discovery API which sits on top of DNS-SD.
// T is the service-specific type which stores information regarding a specific
// service instance.
template <typename T>
class DnsSdServiceWatcher : public DnsSdQuerier::Callback {
 public:
  using ConstRefT = std::reference_wrapper<const T>;

  // The method which will be called when any new service instance is
  // discovered, a service instance changes its data (such as TXT or A data), or
  // a previously discovered service instance ceases to be available. The vector
  // is the set of all currently active service instances which have been
  // discovered so far.
  using ServicesUpdatedCallback =
      std::function<void(std::vector<ConstRefT> services)>;

  // This function type is responsible for converting from a DNS service
  // instance (received from another mDNS endpoint) to a T type to be returned
  // to the caller.
  using ServiceConverter = std::function<T(const DnsSdInstanceRecord&)>;

  DnsSdServiceWatcher(DnsSdService* service,
                      std::string service_name,
                      ServiceConverter conversion,
                      ServicesUpdatedCallback callback)
      : conversion_(conversion),
        service_(service),
        service_name_(std::move(service_name)),
        callback_(std::move(callback)) {}

  ~DnsSdServiceWatcher() = default;

  // Starts service discovery.
  void StartDiscovery() {
    // TODO(rwkeane): Call DnsSdQuerier::StartQuery.
  }

  // Stops service discovery.
  void StopDiscovery() {
    // TODO(rwkeane): Call DnsSdQuerier::StopQuery.
  }

  // Returns whether or not discovery is currently ongoing.
  bool IsRunning() const {
    // TODO(rwkeane): Implement this method.
    return true;
  }

  // Re-initializes the process of service discovery, even if the underlying
  // implementation would not normally do so at this time. All previously
  // received service data is discarded.
  // NOTE: This call will return an error if StartDiscovery has not yet been
  // called.
  Error ForceRefresh() {
    // TODO(rwkeane): Implement this method.
    return Error::None();
  }

  // Re-initializes the process of service discovery, even if the underlying
  // implementation would not normally do so at this time. All previously
  // received service data is persisted.
  // NOTE: This call will return an error if StartDiscovery has not yet been
  // called.
  Error DiscoverNow() {
    // TODO(rwkeane): Implement this method.
    return Error::None();
  }

  // Returns the set of services which have been received so far.
  std::vector<ConstRefT> services() const {
    // TODO(rwkeane): Implement this method.
  }

 private:
  // DnsSdQuerier::Callback overrides.
  void OnInstanceCreated(const DnsSdInstanceRecord& new_record) override {
    // TODO(rwkeane): Implement this method.
  }

  void OnInstanceUpdated(const DnsSdInstanceRecord& modified_record) override {
    // TODO(rwkeane): Implement this method.
  }

  void OnInstanceDeleted(const DnsSdInstanceRecord& old_record) override {
    // TODO(rwkeane): Implement this method.
  }

  // Converts from the DNS-SD representation of a service to the outside
  // representation.
  ServiceConverter conversion_;

  DnsSdService* const service_;
  std::string service_name_;
  ServicesUpdatedCallback callback_;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_PUBLIC_DNS_SD_SERVICE_WATCHER_H_
