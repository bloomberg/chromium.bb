// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_PUBLIC_MDNS_SERVICE_H_
#define DISCOVERY_MDNS_PUBLIC_MDNS_SERVICE_H_

#include <memory>

#include "discovery/mdns/public/mdns_constants.h"

namespace openscreen {

struct IPEndpoint;
class TaskRunner;

namespace discovery {

class DomainName;
class MdnsRecord;
class MdnsRecordChangedCallback;

class MdnsService {
 public:
  virtual ~MdnsService() = default;

  // Creates a new MdnsService instance, to be owned by the caller. On failure,
  // returns nullptr.
  static std::unique_ptr<MdnsService> Create(TaskRunner* task_runner);

  virtual void StartQuery(const DomainName& name,
                          DnsType dns_type,
                          DnsClass dns_class,
                          MdnsRecordChangedCallback* callback) = 0;

  virtual void StopQuery(const DomainName& name,
                         DnsType dns_type,
                         DnsClass dns_class,
                         MdnsRecordChangedCallback* callback) = 0;

  virtual void RegisterRecord(const MdnsRecord& record) = 0;

  virtual void DeregisterRecord(const MdnsRecord& record) = 0;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_MDNS_PUBLIC_MDNS_SERVICE_H_
