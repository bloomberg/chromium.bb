// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_PUBLIC_PUBLISHER_H_
#define DISCOVERY_DNSSD_PUBLIC_PUBLISHER_H_

#include "absl/strings/string_view.h"
#include "discovery/dnssd/public/instance_record.h"

namespace openscreen {
namespace discovery {

class DnsSdPublisher {
 public:
  virtual ~DnsSdPublisher() = default;

  // Publishes the PTR, SRV, TXT, A, and AAAA records provided in the
  // DnsSdInstanceRecord.
  // TODO(rwkeane): Document behavior if the record already exists.
  virtual void Register(const DnsSdInstanceRecord& record) = 0;

  // Unpublishes any PTR, SRV, TXT, A, and AAAA records associated with this
  // (service, domain) pair. If no such records are published, this operation
  // will be a no-op.
  virtual void DeregisterAll(const absl::string_view& service,
                             const absl::string_view& domain) = 0;
};

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_PUBLIC_PUBLISHER_H_
