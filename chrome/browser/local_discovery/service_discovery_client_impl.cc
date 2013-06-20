// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/stl_util.h"
#include "chrome/browser/local_discovery/service_discovery_client_impl.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/record_rdata.h"

namespace local_discovery {

ServiceDiscoveryClientImpl::ServiceDiscoveryClientImpl() {
}

ServiceDiscoveryClientImpl::~ServiceDiscoveryClientImpl() {
}

// static
ServiceDiscoveryClientImpl* ServiceDiscoveryClientImpl::GetInstance() {
  return Singleton<ServiceDiscoveryClientImpl>::get();
}

scoped_ptr<ServiceWatcher> ServiceDiscoveryClientImpl::CreateServiceWatcher(
    const std::string& service_type,
    ServiceWatcher::Delegate* delegate) {
  return scoped_ptr<ServiceWatcher>(new ServiceWatcherImpl(
      service_type,  delegate));
}

scoped_ptr<ServiceResolver> ServiceDiscoveryClientImpl::CreateServiceResolver(
    const std::string& service_name,
    const ServiceResolver::ResolveCompleteCallback& callback) {
  return scoped_ptr<ServiceResolver>(new ServiceResolverImpl(
      service_name, callback));
}

ServiceWatcherImpl::ServiceWatcherImpl(
    const std::string& service_type,
    ServiceWatcher::Delegate* delegate)
    : service_type_(service_type), delegate_(delegate), started_(false) {
}

bool ServiceWatcherImpl::Start() {
  DCHECK(!started_);
  listener_ = net::MDnsClient::GetInstance()->CreateListener(
      net::dns_protocol::kTypePTR, service_type_, this);
  if (!listener_->Start())
    return false;

  started_ = true;
  return true;
}

ServiceWatcherImpl::~ServiceWatcherImpl() {
}

void ServiceWatcherImpl::GetAvailableServices(
    std::vector<std::string>* services) const {
  DCHECK(started_);
  DCHECK(services);
  services->reserve(services_.size());
  for (ServiceListenersMap::const_iterator i = services_.begin();
       i != services_.end(); i++) {
    services->push_back(i->first);
  }
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
    *transaction = net::MDnsClient::GetInstance()->CreateTransaction(
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

    delegate_->OnServiceUpdated(UPDATE_CHANGED, record->name());
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
    ServiceWatcherImpl* watcher) {
  srv_listener_ = net::MDnsClient::GetInstance()->CreateListener(
      net::dns_protocol::kTypeSRV, service_name, watcher);
  txt_listener_ = net::MDnsClient::GetInstance()->CreateListener(
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
      make_pair(service, static_cast<ServiceListeners*>(NULL)));
  if (found.second) {  // Newly inserted.
    found.first->second = linked_ptr<ServiceListeners>(
        new ServiceListeners(service, this));
    bool success = found.first->second->Start();
    DCHECK(success);
    delegate_->OnServiceUpdated(UPDATE_ADDED, service);
  }
}

void ServiceWatcherImpl::RemoveService(const std::string& service) {
  DCHECK(started_);
  ServiceListenersMap::iterator found = services_.find(service);
  if (found != services_.end()) {
    services_.erase(found);
    delegate_->OnServiceUpdated(UPDATE_REMOVED, service);
  }
}

void ServiceWatcherImpl::OnNsecRecord(const std::string& name,
                                          unsigned rrtype) {
  // Do nothing. It is an error for hosts to broadcast an NSEC record for PTR
  // on any name.
}

ServiceResolverImpl::ServiceResolverImpl(
    const std::string& service_name,
    const ResolveCompleteCallback& callback)
    : service_name_(service_name), callback_(callback),
      is_resolving_(false), has_resolved_(false), metadata_resolved_(false),
      address_resolved_(false), service_staging_(new ServiceDescription),
      service_final_(new ServiceDescription) {
  service_staging_->service_name = service_name_;
  service_final_->service_name = service_name_;
}

bool ServiceResolverImpl::StartResolving() {
  is_resolving_ = true;
  address_resolved_ = false;
  metadata_resolved_ = false;

  if (!CreateTxtTransaction())
    return false;
  if (!CreateSrvTransaction())
    return false;
  return true;
}

bool ServiceResolverImpl::IsResolving() const {
  return is_resolving_;
}

bool ServiceResolverImpl::HasResolved() const {
  return has_resolved_;
}

ServiceResolverImpl::~ServiceResolverImpl() {
}

bool ServiceResolverImpl::CreateTxtTransaction() {
  txt_transaction_ = net::MDnsClient::GetInstance()->CreateTransaction(
      net::dns_protocol::kTypeTXT, service_name_,
      net::MDnsTransaction::SINGLE_RESULT | net::MDnsTransaction::QUERY_CACHE |
      net::MDnsTransaction::QUERY_NETWORK,
      base::Bind(&ServiceResolverImpl::TxtRecordTransactionResponse,
                 AsWeakPtr()));
  return txt_transaction_->Start();
}

// TODO(noamsml): quick-resolve for AAAA records.  Since A records tend to be in
void ServiceResolverImpl::CreateATransaction() {
  a_transaction_ = net::MDnsClient::GetInstance()->CreateTransaction(
      net::dns_protocol::kTypeA,
      service_staging_.get()->address.host(),
      net::MDnsTransaction::SINGLE_RESULT | net::MDnsTransaction::QUERY_CACHE,
      base::Bind(&ServiceResolverImpl::ARecordTransactionResponse,
                 AsWeakPtr()));
  a_transaction_->Start();
}

bool ServiceResolverImpl::CreateSrvTransaction() {
  srv_transaction_ = net::MDnsClient::GetInstance()->CreateTransaction(
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
    service_staging_.get()->address = RecordToAddress(record);
    service_staging_.get()->last_seen = record->time_created();
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
    service_staging_.get()->metadata = RecordToMetadata(record);
  } else {
    service_staging_.get()->metadata = std::vector<std::string>();
  }

  metadata_resolved_ = true;
  AlertCallbackIfReady();
}

void ServiceResolverImpl::ARecordTransactionResponse(
    net::MDnsTransaction::Result status, const net::RecordParsed* record) {
  a_transaction_.reset();

  if (status == net::MDnsTransaction::RESULT_RECORD) {
    DCHECK(record);
    service_staging_.get()->ip_address = RecordToIPAddress(record);
  } else {
    service_staging_.get()->ip_address = net::IPAddressNumber();
  }

  address_resolved_ = true;
  AlertCallbackIfReady();
}

void ServiceResolverImpl::AlertCallbackIfReady() {
  if (metadata_resolved_ && address_resolved_) {
    txt_transaction_.reset();
    srv_transaction_.reset();
    a_transaction_.reset();
    has_resolved_ = true;
    is_resolving_ = false;
    service_final_.swap(service_staging_);
    callback_.Run(STATUS_SUCCESS, GetServiceDescription());
  }
}

void ServiceResolverImpl::ServiceNotFound(
    ServiceResolver::RequestStatus status) {
  txt_transaction_.reset();
  srv_transaction_.reset();
  a_transaction_.reset();
  is_resolving_ = false;

  callback_.Run(status, GetServiceDescription());
}

const ServiceDescription& ServiceResolverImpl::GetServiceDescription() const {
  return *service_final_.get();
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
