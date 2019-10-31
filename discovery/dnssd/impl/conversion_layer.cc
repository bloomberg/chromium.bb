// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/conversion_layer.h"

#include "absl/strings/str_join.h"
#include "cast/common/mdns/mdns_records.h"
#include "discovery/dnssd/impl/constants.h"

namespace openscreen {
namespace discovery {

ErrorOr<DnsSdTxtRecord> CreateFromDnsTxt(
    const cast::mdns::TxtRecordRdata& txt_data) {
  DnsSdTxtRecord txt;
  if (txt_data.texts().size() == 1 && txt_data.texts()[0] == "") {
    return txt;
  }

  // Iterate backwards so that the first key of each type is the one that is
  // present at the end, as pet spec.
  for (auto it = txt_data.texts().rbegin(); it != txt_data.texts().rend();
       it++) {
    const std::string& text = *it;
    size_t index_of_eq = text.find_first_of('=');
    if (index_of_eq != std::string::npos) {
      if (index_of_eq == 0) {
        return Error::Code::kParameterInvalid;
      }
      std::string key = text.substr(0, index_of_eq);
      std::string value = text.substr(index_of_eq + 1);
      absl::Span<const uint8_t> data(
          reinterpret_cast<const uint8_t*>(value.c_str()), value.size());
      const auto set_result = txt.SetValue(key, data);
      if (!set_result.ok()) {
        return set_result;
      }
    } else {
      const auto set_result = txt.SetFlag(text, true);
      if (!set_result.ok()) {
        return set_result;
      }
    }
  }

  return txt;
}

ErrorOr<InstanceKey> GetInstanceKey(const cast::mdns::MdnsRecord& record) {
  const cast::mdns::DomainName& names =
      !IsPtrRecord(record)
          ? record.name()
          : absl::get<cast::mdns::PtrRecordRdata>(record.rdata()).ptr_domain();
  if (names.labels().size() < 4) {
    return Error::Code::kParameterInvalid;
  }

  auto it = names.labels().begin();
  InstanceKey result;
  result.instance_id = *it++;
  std::string service_name = *it++;
  std::string protocol = *it++;
  result.service_id = service_name.append(".").append(protocol);
  result.domain_id = absl::StrJoin(it, record.name().labels().end(), ".");
  return result;
}

ErrorOr<ServiceKey> GetServiceKey(const cast::mdns::MdnsRecord& record) {
  ErrorOr<InstanceKey> key_or_error = GetInstanceKey(record);
  if (key_or_error.is_error()) {
    return key_or_error.error();
  }
  return ServiceKey{key_or_error.value().service_id,
                    key_or_error.value().domain_id};
}

ServiceKey GetServiceKey(absl::string_view service, absl::string_view domain) {
  return {service.data(), domain.data()};
}

bool IsPtrRecord(const cast::mdns::MdnsRecord& record) {
  return record.dns_type() == cast::mdns::DnsType::kPTR;
}

}  // namespace discovery
}  // namespace openscreen
