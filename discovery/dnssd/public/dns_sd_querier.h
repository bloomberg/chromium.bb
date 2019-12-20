// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_PUBLIC_DNS_SD_QUERIER_H_
#define DISCOVERY_DNSSD_PUBLIC_DNS_SD_QUERIER_H_

#include "discovery/dnssd/public/dns_sd_instance_record.h"

namespace openscreen {
namespace discovery {

class DnsSdQuerier {
 public:
  // TODO(rwkeane): Add support for expiring records in addition to deleting
  // them.
  class Callback {
   public:
    virtual ~Callback() = default;

    // Callback fired when a new InstanceRecord is created.
    virtual void OnInstanceCreated(const DnsSdInstanceRecord& new_record) = 0;

    // Callback fired when an existing InstanceRecord is updated.
    virtual void OnInstanceUpdated(
        const DnsSdInstanceRecord& modified_record) = 0;

    // Callback fired when an existing InstanceRecord is deleted.
    virtual void OnInstanceDeleted(const DnsSdInstanceRecord& old_record) = 0;
  };

  virtual ~DnsSdQuerier() = default;

  // Begins a new query. The provided callback will be called whenever new
  // information about the provided (service, domain) pair becomes available.
  // The Callback provided is expected to persist until the StopQuery method is
  // called or this instance is destroyed.
  // NOTE: The provided service value is expected to be valid, as defined by the
  // IsServiceValid() method.
  // NOTE: The callback must be called on the TaskRunner thread.
  virtual void StartQuery(const std::string& service, Callback* cb) = 0;

  // Stops an already running query.
  // NOTE: The provided service value is expected to be valid, as defined by the
  // IsServiceValid() method.
  virtual void StopQuery(const std::string& service, Callback* cb) = 0;

  // Re-initializes the process of service discovery for the provided service
  // id. All ongoing queries for this domain are restarted and any previously
  // received query results are discarded.
  virtual void ReinitializeQueries(const std::string& service) = 0;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_PUBLIC_DNS_SD_QUERIER_H_
