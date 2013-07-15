// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/stl_util.h"
#include "chrome/utility/local_discovery/service_discovery_client_impl.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/record_rdata.h"

namespace local_discovery {

ServiceDiscoveryClientImpl::ServiceDiscoveryClientImpl(
    net::MDnsClient* mdns_client) : mdns_client_(mdns_client) {
}

ServiceDiscoveryClientImpl::~ServiceDiscoveryClientImpl() {
}

scoped_ptr<ServiceWatcher> ServiceDiscoveryClientImpl::CreateServiceWatcher(
    const std::string& service_type,
    const ServiceWatcher::UpdatedCallback& callback) {
  return scoped_ptr<ServiceWatcher>(new ServiceWatcherImpl(
      service_type,  callback, mdns_client_));
}

scoped_ptr<ServiceResolver> ServiceDiscoveryClientImpl::CreateServiceResolver(
    const std::string& service_name,
    const ServiceResolver::ResolveCompleteCallback& callback) {
  return scoped_ptr<ServiceResolver>(new ServiceResolverImpl(
      service_name, callback, mdns_client_));
}

ServiceWatcherImpl::ServiceWatcherImpl(
    const std::string& service_type,
    const ServiceWatcher::UpdatedCallback& callback,
    net::MDnsClient* mdns_client)
    : service_type_(service_type), callback_(callback), started_(false),
      mdns_client_(mdns_client) {
}

void ServiceWatcherImpl::Start() {
  DCHECK(!started_);
  listener_ = mdns_client_->CreateListener(
      net::dns_protocol::kTypePTR, service_type_, this);
  started_ = listener_->Start();
  if (started_)
    ReadCachedServices();
}

ServiceWatcherImpl::~ServiceWatcherImpl() {
}

void ServiceWatcherImpl::DiscoverNewServices(bool force_update) {
  DCHECK(started_);
  if (force_update)
    services_.clear();
  CreateTransaction(true /*network*/, false /*cache*/, force_update,
                    &transaction_network_);
}

void ServiceWatcherImpl::ReadCachedServices() {
  DCHECK(started_);
  CreateTransaction(false /*network*/, true /*cache*/, false /*force refresh*/,
                    &transaction_cache_);
}

bool ServiceWatcherImpl::CreateTransaction(
    bool network, bool cache, bool force_refresh,
    scoped_ptr<net::MDnsTransaction>* transaction) {
  int transaction_flags = 0;
  if (network)
    transaction_flags |= net::MDnsTransaction::QUERY_NETWORK;

  if (cache)
    transaction_flags |= net::MDnsTransaction::QUERY_CACHE;

  // TODO(noamsml): Add flag for force_refresh when supported.

  if (transaction_flags) {
    *transaction = mdns_client_->CreateTransaction(
        net::dns_protocol::kTypePTR, service_type_, transaction_flags,
        base::Bind(&ServiceWatcherImpl::OnTransactionResponse,
                   base::Unretained(this), transaction));
    return (*transaction)->Start();
  }

  return true;
}

std::string ServiceWatcherImpl::GetServiceType() const {
  return listener_->GetName();
}

void ServiceWatcherImpl::OnRecordUpdate(
    net::MDnsListener::UpdateType update,
    const net::RecordParsed* record) {
  DCHECK(started_);
  if (record->type() == net::dns_protocol::kTypePTR) {
    DCHECK(record->name() == GetServiceType());
    const net::PtrRecordRdata* rdata = record->rdata<net::PtrRecordRdata>();

    switch (update) {
      case net::MDnsListener::RECORD_ADDED:
        AddService(rdata->ptrdomain());
        break;
      case net::MDnsListener::RECORD_CHANGED:
        NOTREACHED();
        break;
      case net::MDnsListener::RECORD_REMOVED:
        RemoveService(rdata->ptrdomain());
        break;
    }
  } else {
    DCHECK(record->type() == net::dns_protocol::kTypeSRV ||
           record->type() == net::dns_protocol::kTypeTXT);
    DCHECK(services_.find(record->name()) != services_.end());

    DeferUpdate(UPDATE_CHANGED, record->name());
  }
}

void ServiceWatcherImpl::OnCachePurged() {
  // Not yet implemented.
}

void ServiceWatcherImpl::OnTransactionResponse(
    scoped_ptr<net::MDnsTransaction>* transaction,
    net::MDnsTransaction::Result result,
    const net::RecordParsed* record) {
  DCHECK(started_);
  if (result == net::MDnsTransaction::RESULT_RECORD) {
    const net::PtrRecordRdata* rdata = record->rdata<net::PtrRecordRdata>();
    DCHECK(rdata);
    AddService(rdata->ptrdomain());
  } else if (result == net::MDnsTransaction::RESULT_DONE) {
    transaction->reset();
  }

  // Do nothing for NSEC records. It is an error for hosts to broadcast an NSEC
  // record for PTR records on any name.
}

ServiceWatcherImpl::ServiceListeners::ServiceListeners(
    const std::string& service_name,
    ServiceWatcherImpl* watcher,
    net::MDnsClient* mdns_client) : update_pending_(false) {
  srv_listener_ = mdns_client->CreateListener(
      net::dns_protocol::kTypeSRV, service_name, watcher);
  txt_listener_ = mdns_client->CreateListener(
      net::dns_protocol::kTypeTXT, service_name, watcher);
}

ServiceWatcherImpl::ServiceListeners::~ServiceListeners() {
}

bool ServiceWatcherImpl::ServiceListeners::Start() {
  if (!srv_listener_->Start())
    return false;
  return txt_listener_->Start();
}

void ServiceWatcherImpl::AddService(const std::string& service) {
  DCHECK(started_);
  std::pair<ServiceListenersMap::iterator, bool> found = services_.insert(
      make_pair(service, linked_ptr<ServiceListeners>(NULL)));
  if (found.second) {  // Newly inserted.
    found.first->second = linked_ptr<ServiceListeners>(
        new ServiceListeners(service, this, mdns_client_));
    bool success = found.first->second->Start();

    DeferUpdate(UPDATE_ADDED, service);

    DCHECK(success);
  }
}

void ServiceWatcherImpl::DeferUpdate(ServiceWatcher::UpdateType update_type,
                                     const std::string& service_name) {
  ServiceListenersMap::iterator found = services_.find(service_name);

  if (found != services_.end() && !found->second->update_pending()) {
    found->second->set_update_pending(true);
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ServiceWatcherImpl::DeliverDeferredUpdate, AsWeakPtr(),
                   update_type, service_name));
  }
}

void ServiceWatcherImpl::DeliverDeferredUpdate(
    ServiceWatcher::UpdateType update_type, const std::string& service_name) {
  ServiceListenersMap::iterator found = services_.find(service_name);

  if (found != services_.end()) {
    found->second->set_update_pending(false);
    if (!callback_.is_null())
      callback_.Run(update_type, service_name);
  }
}

void ServiceWatcherImpl::RemoveService(const std::string& service) {
  DCHECK(started_);
  ServiceListenersMap::iterator found = services_.find(service);
  if (found != services_.end()) {
    services_.erase(found);
    if (!callback_.is_null())
      callback_.Run(UPDATE_REMOVED, service);
  }
}

void ServiceWatcherImpl::OnNsecRecord(const std::string& name,
                                      unsigned rrtype) {
  // Do nothing. It is an error for hosts to broadcast an NSEC record for PTR
  // on any name.
}

ServiceResolverImpl::ServiceResolverImpl(
    const std::string& service_name,
    const ResolveCompleteCallback& callback,
    net::MDnsClient* mdns_client)
    : service_name_(service_name), callback_(callback),
      metadata_resolved_(false), address_resolved_(false),
      mdns_client_(mdns_client) {
  service_staging_.service_name = service_name_;
}

bool ServiceResolverImpl::StartResolving() {
  address_resolved_ = false;
  metadata_resolved_ = false;

  if (!CreateTxtTransaction())
    return false;
  if (!CreateSrvTransaction())
    return false;
  return true;
}

ServiceResolverImpl::~ServiceResolverImpl() {
}

bool ServiceResolverImpl::CreateTxtTransaction() {
  txt_transaction_ = mdns_client_->CreateTransaction(
      net::dns_protocol::kTypeTXT, service_name_,
      net::MDnsTransaction::SINGLE_RESULT | net::MDnsTransaction::QUERY_CACHE |
      net::MDnsTransaction::QUERY_NETWORK,
      base::Bind(&ServiceResolverImpl::TxtRecordTransactionResponse,
                 AsWeakPtr()));
  return txt_transaction_->Start();
}

// TODO(noamsml): quick-resolve for AAAA records.  Since A records tend to be in
void ServiceResolverImpl::CreateATransaction() {
  a_transaction_ = mdns_client_->CreateTransaction(
      net::dns_protocol::kTypeA,
      service_staging_.address.host(),
      net::MDnsTransaction::SINGLE_RESULT | net::MDnsTransaction::QUERY_CACHE,
      base::Bind(&ServiceResolverImpl::ARecordTransactionResponse,
                 AsWeakPtr()));
  a_transaction_->Start();
}

bool ServiceResolverImpl::CreateSrvTransaction() {
  srv_transaction_ = mdns_client_->CreateTransaction(
      net::dns_protocol::kTypeSRV, service_name_,
      net::MDnsTransaction::SINGLE_RESULT | net::MDnsTransaction::QUERY_CACHE |
      net::MDnsTransaction::QUERY_NETWORK,
      base::Bind(&ServiceResolverImpl::SrvRecordTransactionResponse,
                 AsWeakPtr()));
  return srv_transaction_->Start();
}

std::string ServiceResolverImpl::GetName() const {
  return service_name_;
}

void ServiceResolverImpl::SrvRecordTransactionResponse(
    net::MDnsTransaction::Result status, const net::RecordParsed* record) {
  srv_transaction_.reset();
  if (status == net::MDnsTransaction::RESULT_RECORD) {
    DCHECK(record);
    service_staging_.address = RecordToAddress(record);
    service_staging_.last_seen = record->time_created();
    CreateATransaction();
  } else {
    ServiceNotFound(MDnsStatusToRequestStatus(status));
  }
}

void ServiceResolverImpl::TxtRecordTransactionResponse(
    net::MDnsTransaction::Result status, const net::RecordParsed* record) {
  txt_transaction_.reset();
  if (status == net::MDnsTransaction::RESULT_RECORD) {
    DCHECK(record);
    service_staging_.metadata = RecordToMetadata(record);
  } else {
    service_staging_.metadata = std::vector<std::string>();
  }

  metadata_resolved_ = true;
  AlertCallbackIfReady();
}

void ServiceResolverImpl::ARecordTransactionResponse(
    net::MDnsTransaction::Result status, const net::RecordParsed* record) {
  a_transaction_.reset();

  if (status == net::MDnsTransaction::RESULT_RECORD) {
    DCHECK(record);
    service_staging_.ip_address = RecordToIPAddress(record);
  } else {
    service_staging_.ip_address = net::IPAddressNumber();
  }

  address_resolved_ = true;
  AlertCallbackIfReady();
}

void ServiceResolverImpl::AlertCallbackIfReady() {
  if (metadata_resolved_ && address_resolved_) {
    txt_transaction_.reset();
    srv_transaction_.reset();
    a_transaction_.reset();
    if (!callback_.is_null())
      callback_.Run(STATUS_SUCCESS, service_staging_);
    service_staging_ = ServiceDescription();
  }
}

void ServiceResolverImpl::ServiceNotFound(
    ServiceResolver::RequestStatus status) {
  txt_transaction_.reset();
  srv_transaction_.reset();
  a_transaction_.reset();
  if (!callback_.is_null())
    callback_.Run(status, ServiceDescription());
}

ServiceResolver::RequestStatus ServiceResolverImpl::MDnsStatusToRequestStatus(
    net::MDnsTransaction::Result status) const {
  switch (status) {
    case net::MDnsTransaction::RESULT_RECORD:
      return ServiceResolver::STATUS_SUCCESS;
    case net::MDnsTransaction::RESULT_NO_RESULTS:
      return ServiceResolver::STATUS_REQUEST_TIMEOUT;
    case net::MDnsTransaction::RESULT_NSEC:
      return ServiceResolver::STATUS_KNOWN_NONEXISTENT;
    case net::MDnsTransaction::RESULT_DONE:  // Pass through.
    default:
      NOTREACHED();
      return ServiceResolver::STATUS_REQUEST_TIMEOUT;
  }
}

const std::vector<std::string>& ServiceResolverImpl::RecordToMetadata(
    const net::RecordParsed* record) const {
  DCHECK(record->type() == net::dns_protocol::kTypeTXT);
  const net::TxtRecordRdata* txt_rdata = record->rdata<net::TxtRecordRdata>();
  DCHECK(txt_rdata);
  return txt_rdata->texts();
}

net::HostPortPair ServiceResolverImpl::RecordToAddress(
    const net::RecordParsed* record) const {
  DCHECK(record->type() == net::dns_protocol::kTypeSRV);
  const net::SrvRecordRdata* srv_rdata = record->rdata<net::SrvRecordRdata>();
  DCHECK(srv_rdata);
  return net::HostPortPair(srv_rdata->target(), srv_rdata->port());
}

const net::IPAddressNumber& ServiceResolverImpl::RecordToIPAddress(
    const net::RecordParsed* record) const {
  DCHECK(record->type() == net::dns_protocol::kTypeA);
  const net::ARecordRdata* a_rdata = record->rdata<net::ARecordRdata>();
  DCHECK(a_rdata);
  return a_rdata->address();
}

}  // namespace local_discovery
