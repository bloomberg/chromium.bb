// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_DNSSD_IMPL_CONVERSION_LAYER_H_
#define DISCOVERY_DNSSD_IMPL_CONVERSION_LAYER_H_

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "cast/common/mdns/mdns_records.h"
#include "discovery/dnssd/impl/constants.h"
#include "discovery/dnssd/public/instance_record.h"
#include "discovery/dnssd/public/txt_record.h"
#include "platform/base/error.h"

namespace cast {
namespace mdns {

class MdnsRecord;
class TxtRecordRdata;

}  // namespace mdns
}  // namespace cast

namespace openscreen {
namespace discovery {

// Attempts to create a new TXT record from the provided set of strings,
// returning a TxtRecord on success or an error if the provided strings are
// not valid.
ErrorOr<DnsSdTxtRecord> CreateFromDnsTxt(const cast::mdns::TxtRecordRdata& txt);

// Helper functions to get the Key associated with a given DNS Record.
ErrorOr<InstanceKey> GetInstanceKey(const cast::mdns::MdnsRecord& record);
ErrorOr<ServiceKey> GetServiceKey(const cast::mdns::MdnsRecord& record);

// Creats the ServiceKey associated with the provided service and instance
// names.
ServiceKey GetServiceKey(absl::string_view service, absl::string_view instance);

bool IsPtrRecord(const cast::mdns::MdnsRecord& record);

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_DNSSD_IMPL_CONVERSION_LAYER_H_
