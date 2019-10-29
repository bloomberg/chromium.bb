// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_MDNS_PUBLIC_MDNS_SERVICE_H_
#define CAST_COMMON_MDNS_PUBLIC_MDNS_SERVICE_H_

#include "cast/common/mdns/public/mdns_constants.h"

namespace cast {
namespace mdns {

class DomainName;
class MdnsRecord;
class MdnsRecordChangedCallback;

class MdnsService {
 public:
  virtual ~MdnsService() = default;

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

}  // namespace mdns
}  // namespace cast

#endif  // CAST_COMMON_MDNS_PUBLIC_MDNS_SERVICE_H_
