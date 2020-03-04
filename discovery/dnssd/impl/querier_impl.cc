// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/querier_impl.h"

#include <string>
#include <vector>

#include "platform/api/task_runner.h"
#include "util/logging.h"

namespace openscreen {
namespace discovery {
namespace {

static constexpr char kLocalDomain[] = "local";

}  // namespace

QuerierImpl::QuerierImpl(MdnsService* mdns_querier, TaskRunner* task_runner)
    : mdns_querier_(mdns_querier), task_runner_(task_runner) {
  OSP_DCHECK(mdns_querier_);
  OSP_DCHECK(task_runner_);
}

QuerierImpl::~QuerierImpl() = default;

void QuerierImpl::StartQuery(const std::string& service, Callback* callback) {
  OSP_DCHECK(callback);
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  OSP_DVLOG << "Starting query for service '" << service << "'";

  ServiceKey key(service, kLocalDomain);
  if (!IsQueryRunning(key)) {
    callback_map_[key] = {callback};
    StartDnsQuery(std::move(key));
  } else {
    callback_map_[key].push_back(callback);

    for (auto& kvp : received_records_) {
      if (kvp.first == key) {
        ErrorOr<DnsSdInstanceRecord> record = kvp.second.CreateRecord();
        if (record.is_value()) {
          callback->OnInstanceCreated(record.value());
        }
      }
    }
  }
}

bool QuerierImpl::IsQueryRunning(const std::string& service) const {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());
  return IsQueryRunning(ServiceKey(service, kLocalDomain));
}

void QuerierImpl::StopQuery(const std::string& service, Callback* callback) {
  OSP_DCHECK(callback);
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  OSP_DVLOG << "Stopping query for service '" << service << "'";

  ServiceKey key(service, kLocalDomain);
  auto callback_it = callback_map_.find(key);
  if (callback_it == callback_map_.end()) {
    return;
  }
  std::vector<Callback*>* callbacks = &callback_it->second;

  const auto it = std::find(callbacks->begin(), callbacks->end(), callback);
  if (it != callbacks->end()) {
    callbacks->erase(it);
    if (callbacks->empty()) {
      callback_map_.erase(callback_it);
      StopDnsQuery(std::move(key));
    }
  }
}

void QuerierImpl::ReinitializeQueries(const std::string& service) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  OSP_DVLOG << "Re-initializing query for service '" << service << "'";

  const ServiceKey key(service, kLocalDomain);

  // Stop instance-specific queries and erase all instance data received so far.
  std::vector<InstanceKey> keys_to_remove;
  for (const auto& pair : received_records_) {
    if (key == pair.first) {
      keys_to_remove.push_back(pair.first);
    }
  }
  for (InstanceKey& ik : keys_to_remove) {
    StopDnsQuery(std::move(ik), false);
  }

  // Restart top-level queries.
  mdns_querier_->ReinitializeQueries(GetPtrQueryInfo(key).name);
}

void QuerierImpl::OnRecordChanged(const MdnsRecord& record,
                                  RecordChangedEvent event) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  OSP_DVLOG << "Record with name '" << record.name().ToString()
            << "' and type '" << record.dns_type()
            << "' has received change of type '" << event << "'";

  IsPtrRecord(record) ? HandlePtrRecordChange(record, event)
                      : HandleNonPtrRecordChange(record, event);
}

Error QuerierImpl::HandlePtrRecordChange(const MdnsRecord& record,
                                         RecordChangedEvent event) {
  if (!HasValidDnsRecordAddress(record)) {
    // This means that the received record is malformed.
    return Error::Code::kParameterInvalid;
  }

  switch (event) {
    case RecordChangedEvent::kCreated:
      StartDnsQuery(InstanceKey(record));
      return Error::None();
    case RecordChangedEvent::kExpired:
      StopDnsQuery(InstanceKey(record));
      return Error::None();
    case RecordChangedEvent::kUpdated:
      return Error::Code::kOperationInvalid;
  }
  return Error::Code::kUnknownError;
}

Error QuerierImpl::HandleNonPtrRecordChange(const MdnsRecord& record,
                                            RecordChangedEvent event) {
  if (!HasValidDnsRecordAddress(record)) {
    // This means that the call received had malformed data.
    return Error::Code::kParameterInvalid;
  }

  const ServiceKey key(record);
  if (!IsQueryRunning(key)) {
    // This means that the call was already queued up on the TaskRunner when the
    // callback was removed. The caller no longer cares, so drop the record.
    return Error::Code::kOperationCancelled;
  }
  const std::vector<Callback*>& callbacks = callback_map_[key];

  // Get the current InstanceRecord data associated with the received record.
  const InstanceKey id(record);
  ErrorOr<DnsSdInstanceRecord> old_instance_record = Error::Code::kItemNotFound;
  auto it = received_records_.find(id);
  if (it == received_records_.end()) {
    it = received_records_.emplace(id, DnsData(id)).first;
  } else {
    old_instance_record = it->second.CreateRecord();
  }
  DnsData* data = &it->second;

  // Apply the changes specified by the received event to the stored
  // InstanceRecord.
  Error apply_result = data->ApplyDataRecordChange(record, event);
  if (!apply_result.ok()) {
    OSP_LOG_ERROR << "Received erroneous record change. Error: "
                  << apply_result;
    return apply_result;
  }

  // Send an update to the user, based on how the new and old records compare.
  ErrorOr<DnsSdInstanceRecord> new_instance_record = data->CreateRecord();
  NotifyCallbacks(callbacks, old_instance_record, new_instance_record);

  return Error::None();
}

void QuerierImpl::NotifyCallbacks(
    const std::vector<Callback*>& callbacks,
    const ErrorOr<DnsSdInstanceRecord>& old_record,
    const ErrorOr<DnsSdInstanceRecord>& new_record) {
  if (old_record.is_value() && new_record.is_value()) {
    for (Callback* callback : callbacks) {
      callback->OnInstanceUpdated(new_record.value());
    }
  } else if (old_record.is_value() && !new_record.is_value()) {
    for (Callback* callback : callbacks) {
      callback->OnInstanceDeleted(old_record.value());
    }
  } else if (!old_record.is_value() && new_record.is_value()) {
    for (Callback* callback : callbacks) {
      callback->OnInstanceCreated(new_record.value());
    }
  }
}

void QuerierImpl::StartDnsQuery(InstanceKey key) {
  auto pair = received_records_.emplace(key, DnsData(key));
  if (!pair.second) {
    // This means that a query is already ongoing.
    return;
  }

  DnsQueryInfo query = GetInstanceQueryInfo(key);
  mdns_querier_->StartQuery(query.name, query.dns_type, query.dns_class, this);
}

void QuerierImpl::StopDnsQuery(InstanceKey key, bool should_inform_callbacks) {
  // If the instance is not being queried for, return.
  auto record_it = received_records_.find(key);
  if (record_it == received_records_.end()) {
    return;
  }

  // If the instance has enough associated data that an instance was provided to
  // the higher layer, call the deleted callback for all associated callbacks.
  ErrorOr<DnsSdInstanceRecord> instance_record =
      record_it->second.CreateRecord();
  if (should_inform_callbacks && instance_record.is_value()) {
    const auto it = callback_map_.find(key);
    if (it != callback_map_.end()) {
      for (Callback* callback : it->second) {
        callback->OnInstanceDeleted(instance_record.value());
      }
    }
  }

  // Erase the key to mark the instance as no longer being queried for.
  received_records_.erase(record_it);

  // Call to the mDNS layer to stop the query.
  DnsQueryInfo query = GetInstanceQueryInfo(key);
  mdns_querier_->StopQuery(query.name, query.dns_type, query.dns_class, this);
}

void QuerierImpl::StartDnsQuery(ServiceKey key) {
  DnsQueryInfo query = GetPtrQueryInfo(key);
  mdns_querier_->StartQuery(query.name, query.dns_type, query.dns_class, this);
}

void QuerierImpl::StopDnsQuery(ServiceKey key) {
  DnsQueryInfo query = GetPtrQueryInfo(key);
  mdns_querier_->StopQuery(query.name, query.dns_type, query.dns_class, this);

  // Stop any ongoing instance-specific queries.
  std::vector<InstanceKey> keys_to_remove;
  for (const auto& pair : received_records_) {
    const bool key_is_service_from_query = (key == pair.first);
    if (key_is_service_from_query) {
      keys_to_remove.push_back(pair.first);
    }
  }
  for (auto it = keys_to_remove.begin(); it != keys_to_remove.end(); it++) {
    StopDnsQuery(std::move(*it));
  }
}

}  // namespace discovery
}  // namespace openscreen
