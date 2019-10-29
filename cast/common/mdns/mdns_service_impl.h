// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CAST_COMMON_MDNS_MDNS_SERVICE_IMPL_H_
#define CAST_COMMON_MDNS_MDNS_SERVICE_IMPL_H_

#include "cast/common/mdns/public/mdns_service.h"

namespace cast {
namespace mdns {

class MdnsServiceImpl : MdnsService {
 public:
  void StartQuery(const DomainName& name,
                  DnsType dns_type,
                  DnsClass dns_class,
                  MdnsRecordChangedCallback* callback) override;

  void StopQuery(const DomainName& name,
                 DnsType dns_type,
                 DnsClass dns_class,
                 MdnsRecordChangedCallback* callback) override;

  void RegisterRecord(const MdnsRecord& record) override;

  void DeregisterRecord(const MdnsRecord& record) override;
};

}  // namespace mdns
}  // namespace cast

#endif  // CAST_COMMON_MDNS_MDNS_SERVICE_IMPL_H_
