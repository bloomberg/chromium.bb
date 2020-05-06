// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/dnssd/impl/querier_impl.h"

#include <string>
#include <vector>

#include "discovery/dnssd/impl/network_interface_config.h"
#include "platform/api/task_runner.h"
#include "util/osp_logging.h"

namespace openscreen {
namespace discovery {
namespace {

static constexpr char kLocalDomain[] = "local";

std::vector<PendingQueryChange> GetDnsQueriesDelayed(
    std::vector<DnsQueryInfo> query_infos,
    QuerierImpl* callback,
    PendingQueryChange::ChangeType change_type) {
  std::vector<PendingQueryChange> pending_changes;
  for (auto& info : query_infos) {
    pending_changes.push_back({std::move(info.name), info.dns_type,
                               info.dns_class, callback, change_type});
  }
  return pending_changes;
}

}  // namespace

QuerierImpl::QuerierImpl(MdnsService* mdns_querier,
                         TaskRunner* task_runner,
                         const NetworkInterfaceConfig* network_config)
    : mdns_querier_(mdns_querier),
      task_runner_(task_runner),
      network_config_(network_config) {
  OSP_DCHECK(mdns_querier_);
  OSP_DCHECK(task_runner_);
  OSP_DCHECK(network_config_);
}

QuerierImpl::~QuerierImpl() = default;

void QuerierImpl::StartQuery(const std::string& service, Callback* callback) {
  OSP_DCHECK(callback);
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  OSP_DVLOG << "Starting query for service '" << service << "'";

  ServiceKey key(service, kLocalDomain);
  if (!IsQueryRunning(key)) {
    callback_map_[key] = {callback};
    auto queries = GetDataToStartDnsQuery(std::move(key));
    StartDnsQueriesImmediately(queries);
  } else {
    callback_map_[key].push_back(callback);

    for (auto& kvp : received_records_) {
      if (kvp.first == key) {
        ErrorOr<DnsSdInstanceEndpoint> endpoint = kvp.second.CreateEndpoint();
        if (endpoint.is_value()) {
          callback->OnEndpointCreated(endpoint.value());
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
      auto queries = GetDataToStopDnsQuery(std::move(key));
      StopDnsQueriesImmediately(queries);
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
    auto queries = GetDataToStopDnsQuery(std::move(ik), false);
    StopDnsQueriesImmediately(queries);
  }

  // Restart top-level queries.
  mdns_querier_->ReinitializeQueries(GetPtrQueryInfo(key).name);
}

std::vector<PendingQueryChange> QuerierImpl::OnRecordChanged(
    const MdnsRecord& record,
    RecordChangedEvent event) {
  OSP_DCHECK(task_runner_->IsRunningOnTaskRunner());

  OSP_DVLOG << "Record with name '" << record.name().ToString()
            << "' and type '" << record.dns_type()
            << "' has received change of type '" << event << "'";

  if (IsPtrRecord(record)) {
    ErrorOr<std::vector<PendingQueryChange>> pending_changes =
        HandlePtrRecordChange(record, event);
    if (pending_changes.is_error()) {
      OSP_LOG << "Failed to handle PTR record change of type " << event
              << " with error " << pending_changes.error();
      return {};
    } else {
      return pending_changes.value();
    }
  } else {
    Error error = HandleNonPtrRecordChange(record, event);
    if (!error.ok()) {
      OSP_LOG << "Failed to handle " << record.dns_type()
              << " record change of type " << event << " with error " << error;
    }
    return {};
  }
}

ErrorOr<std::vector<PendingQueryChange>> QuerierImpl::HandlePtrRecordChange(
    const MdnsRecord& record,
    RecordChangedEvent event) {
  if (!HasValidDnsRecordAddress(record)) {
    // This means that the received record is malformed.
    return Error::Code::kParameterInvalid;
  }

  std::vector<DnsQueryInfo> changes;
  switch (event) {
    case RecordChangedEvent::kCreated:
      changes = GetDataToStartDnsQuery(InstanceKey(record));
      return StartDnsQueriesDelayed(std::move(changes));
    case RecordChangedEvent::kExpired:
      changes = GetDataToStopDnsQuery(InstanceKey(record));
      return StopDnsQueriesDelayed(std::move(changes));
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

  // Get the current InstanceEndpoint data associated with the received record.
  const InstanceKey id(record);
  ErrorOr<DnsSdInstanceEndpoint> old_instance_endpoint =
      Error::Code::kItemNotFound;
  auto it = received_records_.find(id);
  if (it == received_records_.end()) {
    it = received_records_
             .emplace(id, DnsData(id, network_config_->network_interface()))
             .first;
  } else {
    old_instance_endpoint = it->second.CreateEndpoint();
  }
  DnsData* data = &it->second;

  // Apply the changes specified by the received event to the stored
  // InstanceEndpoint.
  Error apply_result = data->ApplyDataRecordChange(record, event);
  if (!apply_result.ok()) {
    OSP_LOG_ERROR << "Received erroneous record change. Error: "
                  << apply_result;
    return apply_result;
  }

  // Send an update to the user, based on how the new and old records compare.
  ErrorOr<DnsSdInstanceEndpoint> new_instance_endpoint = data->CreateEndpoint();
  NotifyCallbacks(callbacks, old_instance_endpoint, new_instance_endpoint);

  return Error::None();
}

void QuerierImpl::NotifyCallbacks(
    const std::vector<Callback*>& callbacks,
    const ErrorOr<DnsSdInstanceEndpoint>& old_endpoint,
    const ErrorOr<DnsSdInstanceEndpoint>& new_endpoint) {
  if (old_endpoint.is_value() && new_endpoint.is_value()) {
    for (Callback* callback : callbacks) {
      callback->OnEndpointUpdated(new_endpoint.value());
    }
  } else if (old_endpoint.is_value() && !new_endpoint.is_value()) {
    for (Callback* callback : callbacks) {
      callback->OnEndpointDeleted(old_endpoint.value());
    }
  } else if (!old_endpoint.is_value() && new_endpoint.is_value()) {
    for (Callback* callback : callbacks) {
      callback->OnEndpointCreated(new_endpoint.value());
    }
  }
}

std::vector<DnsQueryInfo> QuerierImpl::GetDataToStartDnsQuery(InstanceKey key) {
  auto pair = received_records_.emplace(
      key, DnsData(key, network_config_->network_interface()));
  if (!pair.second) {
    // This means that a query is already ongoing.
    return {};
  }

  return {GetInstanceQueryInfo(key)};
}

std::vector<DnsQueryInfo> QuerierImpl::GetDataToStopDnsQuery(
    InstanceKey key,
    bool should_inform_callbacks) {
  // If the instance is not being queried for, return.
  auto record_it = received_records_.find(key);
  if (record_it == received_records_.end()) {
    return {};
  }

  // If the instance has enough associated data that an instance was provided to
  // the higher layer, call the deleted callback for all associated callbacks.
  ErrorOr<DnsSdInstanceEndpoint> instance_endpoint =
      record_it->second.CreateEndpoint();
  if (should_inform_callbacks && instance_endpoint.is_value()) {
    const auto it = callback_map_.find(key);
    if (it != callback_map_.end()) {
      for (Callback* callback : it->second) {
        callback->OnEndpointDeleted(instance_endpoint.value());
      }
    }
  }

  // Erase the key to mark the instance as no longer being queried for.
  received_records_.erase(record_it);

  // Call to the mDNS layer to stop the query.
  return {GetInstanceQueryInfo(key)};
}

std::vector<DnsQueryInfo> QuerierImpl::GetDataToStartDnsQuery(ServiceKey key) {
  return {GetPtrQueryInfo(key)};
}

std::vector<DnsQueryInfo> QuerierImpl::GetDataToStopDnsQuery(ServiceKey key) {
  std::vector<DnsQueryInfo> query_infos = {GetPtrQueryInfo(key)};

  // Stop any ongoing instance-specific queries.
  std::vector<InstanceKey> keys_to_remove;
  for (const auto& pair : received_records_) {
    const bool key_is_service_from_query = (key == pair.first);
    if (key_is_service_from_query) {
      keys_to_remove.push_back(pair.first);
    }
  }
  for (auto it = keys_to_remove.begin(); it != keys_to_remove.end(); it++) {
    std::vector<DnsQueryInfo> instance_query_infos =
        GetDataToStopDnsQuery(std::move(*it));
    query_infos.insert(query_infos.begin(), instance_query_infos.begin(),
                       instance_query_infos.end());
  }

  return query_infos;
}

void QuerierImpl::StartDnsQueriesImmediately(
    const std::vector<DnsQueryInfo>& query_infos) {
  for (const auto& query : query_infos) {
    mdns_querier_->StartQuery(query.name, query.dns_type, query.dns_class,
                              this);
  }
}

void QuerierImpl::StopDnsQueriesImmediately(
    const std::vector<DnsQueryInfo>& query_infos) {
  for (const auto& query : query_infos) {
    mdns_querier_->StopQuery(query.name, query.dns_type, query.dns_class, this);
  }
}

std::vector<PendingQueryChange> QuerierImpl::StartDnsQueriesDelayed(
    std::vector<DnsQueryInfo> query_infos) {
  return GetDnsQueriesDelayed(std::move(query_infos), this,
                              PendingQueryChange::kStartQuery);
}

std::vector<PendingQueryChange> QuerierImpl::StopDnsQueriesDelayed(
    std::vector<DnsQueryInfo> query_infos) {
  return GetDnsQueriesDelayed(std::move(query_infos), this,
                              PendingQueryChange::kStopQuery);
}

}  // namespace discovery
}  // namespace openscreen
