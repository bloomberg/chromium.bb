// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/publisher_impl.h"

#include <map>
#include <utility>
#include <vector>

#include "absl/types/optional.h"
#include "discovery/dnssd/impl/conversion_layer.h"
#include "discovery/dnssd/impl/instance_key.h"
#include "discovery/mdns/public/mdns_constants.h"
#include "platform/base/error.h"

namespace openscreen {
namespace discovery {
namespace {

DnsSdInstanceRecord UpdateDomain(const DomainName& domain,
                                 const DnsSdInstanceRecord& record) {
  InstanceKey key(domain);
  const IPEndpoint& v4 = record.address_v4();
  const IPEndpoint& v6 = record.address_v6();
  if (v4 && v6) {
    return DnsSdInstanceRecord(key.instance_id(), key.service_id(),
                               key.domain_id(), v4, v6, record.txt());
  } else {
    const IPEndpoint& endpoint = v4 ? v4 : v6;
    return DnsSdInstanceRecord(key.instance_id(), key.service_id(),
                               key.domain_id(), endpoint, record.txt());
  }
}

inline std::vector<DnsSdInstanceRecord>::iterator FindKey(
    std::vector<DnsSdInstanceRecord>* records,
    const InstanceKey& key) {
  return std::find_if(records->begin(), records->end(),
                      [&key](const DnsSdInstanceRecord& record) {
                        return key == InstanceKey(record);
                      });
}

template <typename T>
inline typename std::map<DnsSdInstanceRecord, T>::iterator FindKey(
    std::map<DnsSdInstanceRecord, T>* records,
    const InstanceKey& key) {
  return std::find_if(
      records->begin(), records->end(),
      [&key](const std::pair<DnsSdInstanceRecord, DnsSdInstanceRecord>& pair) {
        return key == InstanceKey(pair.first);
      });
}

int EraseRecordsWithServiceId(std::vector<DnsSdInstanceRecord>* records,
                              const std::string& service_id) {
  int removed_count = 0;
  for (auto it = records->begin(); it != records->end();) {
    if (it->service_id() == service_id) {
      removed_count++;
      it = records->erase(it);
    } else {
      it++;
    }
  }

  return removed_count;
}

}  // namespace

PublisherImpl::PublisherImpl(MdnsService* publisher)
    : mdns_publisher_(publisher) {}

PublisherImpl::~PublisherImpl() = default;

Error PublisherImpl::Register(const DnsSdInstanceRecord& record) {
  if (published_records_.find(record) != published_records_.end()) {
    return Error::Code::kItemAlreadyExists;
  } else if (std::find(pending_records_.begin(), pending_records_.end(),
                       record) != pending_records_.end()) {
    return Error::Code::kItemAlreadyExists;
  }

  InstanceKey key(record);
  IPEndpoint endpoint =
      record.address_v4() ? record.address_v4() : record.address_v6();
  pending_records_.push_back(record);
  mdns_publisher_->StartProbe(this, GetDomainName(key), endpoint.address);
  return Error::None();
}

Error PublisherImpl::UpdateRegistration(const DnsSdInstanceRecord& record) {
  // Check if the record is still pending publication.
  auto it = FindKey(&pending_records_, InstanceKey(record));

  // If it is a pending record, update it. Else, try to update a published
  // record.
  if (it != pending_records_.end()) {
    // The instance, service, and domain ids have not changed, so only the
    // remaining data needs to change. The ongoing probe does not need to be
    // modified.
    *it = record;
    return Error::None();
  } else {
    return UpdatePublishedRegistration(record);
  }
}

Error PublisherImpl::UpdatePublishedRegistration(
    const DnsSdInstanceRecord& record) {
  auto published_record_it = FindKey(&published_records_, InstanceKey(record));

  // Check preconditions called out in header. Specifically, the updated record
  // must be making changes to an already published record.
  if (published_record_it == published_records_.end() ||
      published_record_it->first == record) {
    return Error::Code::kParameterInvalid;
  }

  // Get all records which have changed. By design, there an only be one record
  // of each DnsType, so use that here to simplify this step.
  // First in each pair is the old records, second is the new record.
  std::map<DnsType,
           std::pair<absl::optional<MdnsRecord>, absl::optional<MdnsRecord>>>
      changed_records;
  const std::vector<MdnsRecord> old_records =
      GetDnsRecords(published_record_it->second);
  const DnsSdInstanceRecord updated_record = UpdateDomain(
      GetDomainName(InstanceKey(published_record_it->second)), record);
  const std::vector<MdnsRecord> new_records = GetDnsRecords(updated_record);

  // Populate the first part of each pair in |changed_records|.
  for (size_t i = 0; i < old_records.size(); i++) {
    const auto key = old_records[i].dns_type();
    OSP_DCHECK(changed_records.find(key) == changed_records.end());
    auto value = std::make_pair(std::move(old_records[i]), absl::nullopt);
    changed_records.emplace(key, std::move(value));
  }

  // Populate the second part of each pair in |changed_records|.
  for (size_t i = 0; i < new_records.size(); i++) {
    const auto key = new_records[i].dns_type();
    auto find_it = changed_records.find(key);
    if (find_it == changed_records.end()) {
      std::pair<absl::optional<MdnsRecord>, absl::optional<MdnsRecord>> value(
          absl::nullopt, std::move(new_records[i]));
      changed_records.emplace(key, std::move(value));
    } else {
      find_it->second.second = std::move(new_records[i]);
    }
  }

  // Apply changes called out in |changed_records|.
  for (const auto& pair : changed_records) {
    OSP_DCHECK(pair.second.first != absl::nullopt ||
               pair.second.second != absl::nullopt);
    if (pair.second.first == absl::nullopt) {
      mdns_publisher_->RegisterRecord(pair.second.second.value());
    } else if (pair.second.second == absl::nullopt) {
      mdns_publisher_->UnregisterRecord(pair.second.first.value());
    } else if (pair.second.first.value() != pair.second.second.value()) {
      mdns_publisher_->UpdateRegisteredRecord(pair.second.first.value(),
                                              pair.second.second.value());
    }
  }

  // Replace the old records with the new ones.
  published_records_.erase(published_record_it);
  published_records_.emplace(record, std::move(updated_record));

  return Error::None();
}

int PublisherImpl::DeregisterAll(const std::string& service) {
  int removed_count = 0;

  for (auto it = published_records_.begin(); it != published_records_.end();) {
    if (it->second.service_id() == service) {
      for (const auto& mdns_record : GetDnsRecords(it->second)) {
        mdns_publisher_->UnregisterRecord(mdns_record);
      }
      removed_count++;
      it = published_records_.erase(it);
    } else {
      it++;
    }
  }

  removed_count += EraseRecordsWithServiceId(&pending_records_, service);

  return removed_count;
}

void PublisherImpl::OnDomainFound(const DomainName& requested_name,
                                  const DomainName& confirmed_name) {
  auto it = FindKey(&pending_records_, InstanceKey(requested_name));

  if (it == pending_records_.end()) {
    // This will be hit if the record was deregistered before the probe phase
    // was completed.
    return;
  }

  DnsSdInstanceRecord requested_record = std::move(*it);
  DnsSdInstanceRecord publication = requested_record;
  pending_records_.erase(it);

  InstanceKey requested_key(requested_record);

  if (requested_name != confirmed_name) {
    OSP_DCHECK(HasValidDnsRecordAddress(confirmed_name));
    publication = UpdateDomain(confirmed_name, requested_record);
  }

  for (const auto& mdns_record : GetDnsRecords(publication)) {
    mdns_publisher_->RegisterRecord(mdns_record);
  }

  published_records_.emplace(std::move(requested_record),
                             std::move(publication));
}

}  // namespace discovery
}  // namespace openscreen
