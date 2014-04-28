// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_IMPL_H_
#define CHROME_COMMON_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_IMPL_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "chrome/common/local_discovery/service_discovery_client.h"
#include "net/dns/mdns_client.h"

namespace local_discovery {

class ServiceDiscoveryClientImpl : public ServiceDiscoveryClient {
 public:
  // |mdns_client| must outlive the Service Discovery Client.
  explicit ServiceDiscoveryClientImpl(net::MDnsClient* mdns_client);
  virtual ~ServiceDiscoveryClientImpl();

  // ServiceDiscoveryClient implementation:
  virtual scoped_ptr<ServiceWatcher> CreateServiceWatcher(
      const std::string& service_type,
      const ServiceWatcher::UpdatedCallback& callback) OVERRIDE;

  virtual scoped_ptr<ServiceResolver> CreateServiceResolver(
      const std::string& service_name,
      const ServiceResolver::ResolveCompleteCallback& callback) OVERRIDE;

  virtual scoped_ptr<LocalDomainResolver> CreateLocalDomainResolver(
      const std::string& domain,
      net::AddressFamily address_family,
      const LocalDomainResolver::IPAddressCallback& callback) OVERRIDE;

 private:
  net::MDnsClient* mdns_client_;

  DISALLOW_COPY_AND_ASSIGN(ServiceDiscoveryClientImpl);
};

class ServiceWatcherImpl : public ServiceWatcher,
                           public net::MDnsListener::Delegate,
                           public base::SupportsWeakPtr<ServiceWatcherImpl> {
 public:
  ServiceWatcherImpl(const std::string& service_type,
                     const ServiceWatcher::UpdatedCallback& callback,
                     net::MDnsClient* mdns_client);
  // Listening will automatically stop when the destructor is called.
  virtual ~ServiceWatcherImpl();

  // ServiceWatcher implementation:
  virtual void Start() OVERRIDE;

  virtual void DiscoverNewServices(bool force_update) OVERRIDE;

  virtual void SetActivelyRefreshServices(
      bool actively_refresh_services) OVERRIDE;

  virtual std::string GetServiceType() const OVERRIDE;

  virtual void OnRecordUpdate(net::MDnsListener::UpdateType update,
                              const net::RecordParsed* record) OVERRIDE;

  virtual void OnNsecRecord(const std::string& name, unsigned rrtype) OVERRIDE;

  virtual void OnCachePurged() OVERRIDE;

  virtual void OnTransactionResponse(
      scoped_ptr<net::MDnsTransaction>* transaction,
      net::MDnsTransaction::Result result,
      const net::RecordParsed* record);

 private:
  struct ServiceListeners {
    ServiceListeners(const std::string& service_name,
                     ServiceWatcherImpl* watcher,
                     net::MDnsClient* mdns_client);
    ~ServiceListeners();
    bool Start();
    void SetActiveRefresh(bool auto_update);

    void set_update_pending(bool update_pending) {
      update_pending_ = update_pending;
    }

    bool update_pending() { return update_pending_; }

    void set_has_ptr(bool has_ptr) {
      has_ptr_ = has_ptr;
    }

    void set_has_srv(bool has_srv);

    bool has_ptr_or_srv() { return has_ptr_ || has_srv_; }

   private:
    void OnSRVRecord(net::MDnsTransaction::Result result,
                     const net::RecordParsed* record);

    void DoQuerySRV();

    scoped_ptr<net::MDnsListener> srv_listener_;
    scoped_ptr<net::MDnsListener> txt_listener_;
    scoped_ptr<net::MDnsTransaction> srv_transaction_;

    std::string service_name_;
    net::MDnsClient* mdns_client_;
    bool update_pending_;

    bool has_ptr_;
    bool has_srv_;
  };

  typedef std::map<std::string, linked_ptr<ServiceListeners> >
      ServiceListenersMap;

  void ReadCachedServices();
  void AddService(const std::string& service);
  void RemovePTR(const std::string& service);
  void RemoveSRV(const std::string& service);
  void AddSRV(const std::string& service);
  bool CreateTransaction(bool active, bool alert_existing_services,
                         bool force_refresh,
                         scoped_ptr<net::MDnsTransaction>* transaction);

  void DeferUpdate(ServiceWatcher::UpdateType update_type,
                   const std::string& service_name);
  void DeliverDeferredUpdate(ServiceWatcher::UpdateType update_type,
                             const std::string& service_name);

  void ScheduleQuery(int timeout_seconds);

  void SendQuery(int next_timeout_seconds, bool force_update);

  std::string service_type_;
  ServiceListenersMap services_;
  scoped_ptr<net::MDnsTransaction> transaction_network_;
  scoped_ptr<net::MDnsTransaction> transaction_cache_;
  scoped_ptr<net::MDnsListener> listener_;

  ServiceWatcher::UpdatedCallback callback_;
  bool started_;
  bool actively_refresh_services_;

  net::MDnsClient* mdns_client_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWatcherImpl);
};

class ServiceResolverImpl
    : public ServiceResolver,
      public base::SupportsWeakPtr<ServiceResolverImpl> {
 public:
  ServiceResolverImpl(const std::string& service_name,
                      const ServiceResolver::ResolveCompleteCallback& callback,
                      net::MDnsClient* mdns_client);

  virtual ~ServiceResolverImpl();

  // ServiceResolver implementation:
  virtual void StartResolving() OVERRIDE;

  virtual std::string GetName() const OVERRIDE;

 private:
  // Respond to transaction finishing for SRV records.
  void SrvRecordTransactionResponse(net::MDnsTransaction::Result status,
                                    const net::RecordParsed* record);

  // Respond to transaction finishing for TXT records.
  void TxtRecordTransactionResponse(net::MDnsTransaction::Result status,
                                    const net::RecordParsed* record);

  // Respond to transaction finishing for A records.
  void ARecordTransactionResponse(net::MDnsTransaction::Result status,
                                  const net::RecordParsed* record);

  void AlertCallbackIfReady();

  void ServiceNotFound(RequestStatus status);

  // Convert a TXT record to a vector of strings (metadata).
  const std::vector<std::string>& RecordToMetadata(
      const net::RecordParsed* record) const;

  // Convert an SRV record to a host and port pair.
  net::HostPortPair RecordToAddress(
      const net::RecordParsed* record) const;

  // Convert an A record to an IP address.
  const net::IPAddressNumber& RecordToIPAddress(
      const net::RecordParsed* record) const;

  // Convert an MDns status to a service discovery status.
  RequestStatus MDnsStatusToRequestStatus(
      net::MDnsTransaction::Result status) const;

  bool CreateTxtTransaction();
  bool CreateSrvTransaction();
  void CreateATransaction();

  std::string service_name_;
  ResolveCompleteCallback callback_;

  bool has_resolved_;

  bool metadata_resolved_;
  bool address_resolved_;

  scoped_ptr<net::MDnsTransaction> txt_transaction_;
  scoped_ptr<net::MDnsTransaction> srv_transaction_;
  scoped_ptr<net::MDnsTransaction> a_transaction_;

  ServiceDescription service_staging_;

  net::MDnsClient* mdns_client_;

  DISALLOW_COPY_AND_ASSIGN(ServiceResolverImpl);
};

class LocalDomainResolverImpl : public LocalDomainResolver {
 public:
  LocalDomainResolverImpl(const std::string& domain,
                          net::AddressFamily address_family,
                          const IPAddressCallback& callback,
                          net::MDnsClient* mdns_client);
  virtual ~LocalDomainResolverImpl();

  virtual void Start() OVERRIDE;

  const std::string& domain() { return domain_; }

 private:
  void OnTransactionComplete(
      net::MDnsTransaction::Result result,
      const net::RecordParsed* record);

  scoped_ptr<net::MDnsTransaction> CreateTransaction(uint16 type);

  bool IsSuccess();

  void SendResolvedAddresses();

  std::string domain_;
  net::AddressFamily address_family_;
  IPAddressCallback callback_;

  scoped_ptr<net::MDnsTransaction> transaction_a_;
  scoped_ptr<net::MDnsTransaction> transaction_aaaa_;

  int transactions_finished_;

  net::MDnsClient* mdns_client_;

  net::IPAddressNumber address_ipv4_;
  net::IPAddressNumber address_ipv6_;

  base::CancelableCallback<void()> timeout_callback_;

  DISALLOW_COPY_AND_ASSIGN(LocalDomainResolverImpl);
};


}  // namespace local_discovery

#endif  // CHROME_COMMON_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_IMPL_H_
