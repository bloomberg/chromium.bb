// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/querier_impl.h"

namespace openscreen {
namespace discovery {

QuerierImpl::QuerierImpl(cast::mdns::MdnsService* mdns_querier)
    : mdns_querier_(mdns_querier) {
  OSP_DCHECK(mdns_querier_);
}

void QuerierImpl::StartQuery(const absl::string_view& service,
                             const absl::string_view& domain,
                             Callback* callback) {
  // TODO(rwkeane): Implement this method.
}

void QuerierImpl::StopQuery(const absl::string_view& service,
                            const absl::string_view& domain,
                            Callback* callback) {
  // TODO(rwkeane): Implement this method.
}

void QuerierImpl::OnRecordChanged(const cast::mdns::MdnsRecord& record,
                                  cast::mdns::RecordChangedEvent event) {
  // TODO(rwkeane): Implement this method.
}

}  // namespace discovery
}  // namespace openscreen
