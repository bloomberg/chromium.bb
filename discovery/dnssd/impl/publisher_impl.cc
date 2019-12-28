// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/publisher_impl.h"

#include <map>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "discovery/mdns/public/mdns_constants.h"
#include "platform/base/error.h"

namespace openscreen {
namespace discovery {

PublisherImpl::PublisherImpl(MdnsService* publisher)
    : mdns_publisher_(publisher) {}

PublisherImpl::~PublisherImpl() = default;

Error PublisherImpl::Register(const DnsSdInstanceRecord& record) {
  if (std::find(published_records_.begin(), published_records_.end(), record) !=
      published_records_.end()) {
    return Error::Code::kItemAlreadyExists;
  }

  published_records_.push_back(record);
  for (const auto& mdns_record : GetDnsRecords(record)) {
    mdns_publisher_->RegisterRecord(mdns_record);
  }
  return Error::None();
}

Error PublisherImpl::UpdateRegistration(const DnsSdInstanceRecord& record) {
  auto it = std::find_if(published_records_.begin(), published_records_.end(),
                         [&record](const DnsSdInstanceRecord& other) {
                           return record.instance_id() == other.instance_id() &&
                                  record.service_id() == other.service_id() &&
                                  record.domain_id() == other.domain_id();
                         });

  // Check preconditions called out in header. Specifically, the updated record
  // must be making changes to an already published record.
  if (it == published_records_.end() || *it == record) {
    return Error::Code::kParameterInvalid;
  }

  // Get all records which have changed.
  // First in each pair is the old records, second is the new record.
  std::map<DnsType,
           std::pair<absl::optional<MdnsRecord>, absl::optional<MdnsRecord>>>
      changed_records;
  std::vector<MdnsRecord> old_records = GetDnsRecords(*it);
  std::vector<MdnsRecord> new_records = GetDnsRecords(record);

  for (size_t i = 0; i < old_records.size(); i++) {
    auto key = old_records[i].dns_type();
    OSP_DCHECK(changed_records.find(key) == changed_records.end());
    auto value = std::make_pair(std::move(old_records[i]), absl::nullopt);
    changed_records.emplace(key, std::move(value));
  }

  for (size_t i = 0; i < new_records.size(); i++) {
    auto key = new_records[i].dns_type();
    auto find_it = changed_records.find(key);
    if (find_it == changed_records.end()) {
      std::pair<absl::optional<MdnsRecord>, absl::optional<MdnsRecord>> value(
          absl::nullopt, std::move(new_records[i]));
      changed_records.emplace(key, std::move(value));
    } else {
      find_it->second.second = std::move(new_records[i]);
    }
  }

  // Apply record changes.
  for (const auto& pair : changed_records) {
    OSP_DCHECK(pair.second.first != absl::nullopt ||
               pair.second.second != absl::nullopt);
    if (pair.second.first == absl::nullopt) {
      mdns_publisher_->RegisterRecord(pair.second.second.value());
    } else if (pair.second.second == absl::nullopt) {
      mdns_publisher_->DeregisterRecord(pair.second.first.value());
    } else if (pair.second.first.value() != pair.second.second.value()) {
      mdns_publisher_->UpdateRegisteredRecord(pair.second.first.value(),
                                              pair.second.second.value());
    }
  }

  return Error::None();
}

int PublisherImpl::DeregisterAll(const std::string& service) {
  int removed_count = 0;
  for (auto it = published_records_.begin(); it != published_records_.end();) {
    if (it->service_id() == service) {
      for (const auto& mdns_record : GetDnsRecords(*it)) {
        mdns_publisher_->DeregisterRecord(mdns_record);
      }
      removed_count++;
      it = published_records_.erase(it);
    } else {
      it++;
    }
  }

  return removed_count;
}

}  // namespace discovery
}  // namespace openscreen
