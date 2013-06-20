// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_IMPL_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_IMPL_H_

#include <map>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/cancelable_callback.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/local_discovery/service_discovery_client.h"
#include "net/dns/mdns_client.h"

namespace local_discovery {

class ServiceDiscoveryClientImpl : public ServiceDiscoveryClient {
 public:
  ServiceDiscoveryClientImpl();
  virtual ~ServiceDiscoveryClientImpl();

  // ServiceDiscoveryClient implementation:
  virtual scoped_ptr<ServiceWatcher> CreateServiceWatcher(
      const std::string& service_type,
      ServiceWatcher::Delegate* delegate) OVERRIDE;

  virtual scoped_ptr<ServiceResolver> CreateServiceResolver(
      const std::string& service_name,
      const ServiceResolver::ResolveCompleteCallback& callback) OVERRIDE;

  static ServiceDiscoveryClientImpl* GetInstance();
 private:
  DISALLOW_COPY_AND_ASSIGN(ServiceDiscoveryClientImpl);
};

class ServiceWatcherImpl : public ServiceWatcher,
                           public net::MDnsListener::Delegate {
 public:
  ServiceWatcherImpl(const std::string& service_type,
                     ServiceWatcher::Delegate* delegate);
  // Listening will automatically stop when the destructor is called.
  virtual ~ServiceWatcherImpl();

  // ServiceWatcher implementation:
  virtual bool Start() OVERRIDE;

  virtual void GetAvailableServices(
      std::vector<std::string>* services) const OVERRIDE;

  virtual void DiscoverNewServices(bool force_update) OVERRIDE;

  virtual std::string GetServiceType() const OVERRIDE;

  virtual void ReadCachedServices() OVERRIDE;

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
                     ServiceWatcherImpl* watcher);
    ~ServiceListeners();
    bool Start();

   private:
    scoped_ptr<net::MDnsListener> srv_listener_;
    scoped_ptr<net::MDnsListener> txt_listener_;
  };

  typedef std::map<std::string, linked_ptr<ServiceListeners>>
      ServiceListenersMap;

  void AddService(const std::string& service);
  void RemoveService(const std::string& service);
  bool CreateTransaction(bool active, bool alert_existing_services,
                         bool force_refresh,
                         scoped_ptr<net::MDnsTransaction>* transaction);

  std::string service_type_;
  ServiceListenersMap services_;
  scoped_ptr<net::MDnsTransaction> transaction_network_;
  scoped_ptr<net::MDnsTransaction> transaction_cache_;
  scoped_ptr<net::MDnsListener> listener_;

  ServiceWatcher::Delegate* delegate_;
  bool started_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWatcherImpl);
};

class ServiceResolverImpl
    : public ServiceResolver,
      public base::SupportsWeakPtr<ServiceResolverImpl> {
 public:
  ServiceResolverImpl(const std::string& service_name,
                      const ServiceResolver::ResolveCompleteCallback& callback);

  virtual ~ServiceResolverImpl();

  // ServiceResolver implementation:
  virtual bool StartResolving() OVERRIDE;

  virtual bool IsResolving() const OVERRIDE;

  virtual bool HasResolved() const OVERRIDE;

  virtual std::string GetName() const OVERRIDE;

  virtual const ServiceDescription& GetServiceDescription() const OVERRIDE;

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

  bool is_resolving_;
  bool has_resolved_;

  bool metadata_resolved_;
  bool address_resolved_;

  scoped_ptr<net::MDnsTransaction> txt_transaction_;
  scoped_ptr<net::MDnsTransaction> srv_transaction_;
  scoped_ptr<net::MDnsTransaction> a_transaction_;

  scoped_ptr<ServiceDescription> service_staging_;
  scoped_ptr<ServiceDescription> service_final_;

  DISALLOW_COPY_AND_ASSIGN(ServiceResolverImpl);
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_IMPL_H_
