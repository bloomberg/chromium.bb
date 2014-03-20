// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_MAC_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_MAC_H_

#include <string>

#include "base/mac/scoped_nsobject.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/local_discovery/service_discovery_shared_client.h"
#include "content/public/browser/browser_thread.h"

namespace base {
class Thread;
}

namespace local_discovery {

template <class T>
class ServiceDiscoveryThreadDeleter {
 public:
  inline void operator()(T* t) { t->DeleteSoon(); }
};

// Implementation of ServiceDiscoveryClient that uses the Bonjour SDK.
// https://developer.apple.com/library/mac/documentation/Networking/Conceptual/
// NSNetServiceProgGuide/Articles/BrowsingForServices.html
class ServiceDiscoveryClientMac : public ServiceDiscoverySharedClient {
 public:
  ServiceDiscoveryClientMac();

 private:
  virtual ~ServiceDiscoveryClientMac();

  // ServiceDiscoveryClient implementation.
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

  void StartThreadIfNotStarted();

  scoped_ptr<base::Thread> service_discovery_thread_;

  DISALLOW_COPY_AND_ASSIGN(ServiceDiscoveryClientMac);
};

class ServiceWatcherImplMac : public ServiceWatcher {
 public:
  class NetServiceBrowserContainer {
   public:
    NetServiceBrowserContainer(
        const std::string& service_type,
        const ServiceWatcher::UpdatedCallback& callback,
        scoped_refptr<base::MessageLoopProxy> service_discovery_runner);
    ~NetServiceBrowserContainer();

    void Start();
    void DiscoverNewServices();

    void OnServicesUpdate(ServiceWatcher::UpdateType update,
                          const std::string& service);

    void DeleteSoon();

   private:
    void StartOnDiscoveryThread();
    void DiscoverOnDiscoveryThread();

    bool IsOnServiceDiscoveryThread() {
      return base::MessageLoopProxy::current() ==
             service_discovery_runner_.get();
    }

    std::string service_type_;
    ServiceWatcher::UpdatedCallback callback_;

    scoped_refptr<base::MessageLoopProxy> callback_runner_;
    scoped_refptr<base::MessageLoopProxy> service_discovery_runner_;

    base::scoped_nsobject<id> delegate_;
    base::scoped_nsobject<NSNetServiceBrowser> browser_;
    base::WeakPtrFactory<NetServiceBrowserContainer> weak_factory_;
  };

  ServiceWatcherImplMac(
      const std::string& service_type,
      const ServiceWatcher::UpdatedCallback& callback,
      scoped_refptr<base::MessageLoopProxy> service_discovery_runner);

  void OnServicesUpdate(ServiceWatcher::UpdateType update,
                        const std::string& service);

 private:
  virtual ~ServiceWatcherImplMac();

  virtual void Start() OVERRIDE;
  virtual void DiscoverNewServices(bool force_update) OVERRIDE;
  virtual void SetActivelyRefreshServices(
      bool actively_refresh_services) OVERRIDE;
  virtual std::string GetServiceType() const OVERRIDE;

  std::string service_type_;
  ServiceWatcher::UpdatedCallback callback_;
  bool started_;

  scoped_ptr<NetServiceBrowserContainer,
             ServiceDiscoveryThreadDeleter<NetServiceBrowserContainer> >
      container_;
  base::WeakPtrFactory<ServiceWatcherImplMac> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWatcherImplMac);
};

class ServiceResolverImplMac : public ServiceResolver {
 public:
  class NetServiceContainer {
   public:
    NetServiceContainer(
        const std::string& service_name,
        const ServiceResolver::ResolveCompleteCallback& callback,
        scoped_refptr<base::MessageLoopProxy> service_discovery_runner);

    virtual ~NetServiceContainer();

    void StartResolving();

    void OnResolveUpdate(RequestStatus);

    void SetServiceForTesting(base::scoped_nsobject<NSNetService> service);

    void DeleteSoon();

   private:
    void StartResolvingOnDiscoveryThread();

    bool IsOnServiceDiscoveryThread() {
      return base::MessageLoopProxy::current() ==
             service_discovery_runner_.get();
    }

    const std::string service_name_;
    ServiceResolver::ResolveCompleteCallback callback_;

    scoped_refptr<base::MessageLoopProxy> callback_runner_;
    scoped_refptr<base::MessageLoopProxy> service_discovery_runner_;

    base::scoped_nsobject<id> delegate_;
    base::scoped_nsobject<NSNetService> service_;
    ServiceDescription service_description_;
    base::WeakPtrFactory<NetServiceContainer> weak_factory_;
  };

  ServiceResolverImplMac(
      const std::string& service_name,
      const ServiceResolver::ResolveCompleteCallback& callback,
      scoped_refptr<base::MessageLoopProxy> service_discovery_runner);

  // Testing methods.
  NetServiceContainer* GetContainerForTesting();

 private:
  virtual ~ServiceResolverImplMac();

  virtual void StartResolving() OVERRIDE;
  virtual std::string GetName() const OVERRIDE;

  void OnResolveComplete(RequestStatus status,
                         const ServiceDescription& description);

  const std::string service_name_;
  ServiceResolver::ResolveCompleteCallback callback_;
  bool has_resolved_;

  scoped_ptr<NetServiceContainer,
             ServiceDiscoveryThreadDeleter<NetServiceContainer> > container_;
  base::WeakPtrFactory<ServiceResolverImplMac> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceResolverImplMac);
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_SERVICE_DISCOVERY_CLIENT_MAC_H_
