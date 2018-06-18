// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_SYSTEM_NETWORK_CONTEXT_MANAGER_H_
#define CHROME_BROWSER_NET_SYSTEM_NETWORK_CONTEXT_MANAGER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "chrome/browser/net/proxy_config_monitor.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/network/public/mojom/ssl_config.mojom.h"

class PrefRegistrySimple;
class SSLConfigServiceManager;

namespace network {
namespace mojom {
class URLLoaderFactory;
}
class SharedURLLoaderFactory;
}  // namespace network

// Responsible for creating and managing access to the system NetworkContext.
// Lives on the UI thread. The NetworkContext this owns is intended for requests
// not associated with a profile. It stores no data on disk, and has no HTTP
// cache, but it does have ephemeral cookie and channel ID stores. It also does
// not have access to HTTP proxy auth information the user has entered or that
// comes from extensions, and similarly, has no extension-provided per-profile
// proxy configuration information.
//
// This class is also responsible for configuring global NetworkService state.
//
// The "system" NetworkContext will either share a URLRequestContext with
// IOThread's SystemURLRequestContext and be part of IOThread's NetworkService
// (If the network service is disabled) or be an independent NetworkContext
// using the actual network service.
//
// This class is intended to eventually replace IOThread. Handling the two cases
// differently allows this to be used in production without breaking anything or
// requiring two separate paths, while IOThread consumers slowly transition over
// to being compatible with the network service.
class SystemNetworkContextManager {
 public:
  SystemNetworkContextManager();
  ~SystemNetworkContextManager();

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Initializes |network_context_params| as needed to set up a system
  // NetworkContext. If the network service is disabled,
  // |network_context_request| will be for the NetworkContext used by the
  // SystemNetworkContextManager. Otherwise, this method can still be used to
  // help set up the IOThread's in-process URLRequestContext.
  //
  // Must be called before the system NetworkContext is first used.
  //
  // |stub_resolver_enabled|, |dns_over_https_servers|,
  // |http_auth_static_params|, |http_auth_dynamic_params|, and
  // |is_quic_allowed| are used to pass initial NetworkService state to the
  // caller, so the NetworkService can be configured appropriately. Using
  // NetworkService's Mojo interface to set those options would lead to races
  // with other UI->IO thread network-related tasks, since Mojo doesn't preserve
  // execution order relative to PostTasks.
  void SetUp(network::mojom::NetworkContextRequest* network_context_request,
             network::mojom::NetworkContextParamsPtr* network_context_params,
             bool* stub_resolver_enabled,
             base::Optional<std::vector<network::mojom::DnsOverHttpsServerPtr>>*
                 dns_over_https_servers,
             network::mojom::HttpAuthStaticParamsPtr* http_auth_static_params,
             network::mojom::HttpAuthDynamicParamsPtr* http_auth_dynamic_params,
             bool* is_quic_allowed);

  // Returns the System NetworkContext. May only be called after SetUp(). Does
  // any initialization of the NetworkService that may be needed when first
  // called.
  network::mojom::NetworkContext* GetContext();

  // Returns a URLLoaderFactory owned by the SystemNetworkContextManager that is
  // backed by the SystemNetworkContext. Allows sharing of the URLLoaderFactory.
  // Prefer this to creating a new one.  Call Clone() on the value returned by
  // this method to get a URLLoaderFactory that can be used on other threads.
  network::mojom::URLLoaderFactory* GetURLLoaderFactory();

  // Returns a SharedURLLoaderFactory owned by the SystemNetworkContextManager
  // that is backed by the SystemNetworkContext.
  scoped_refptr<network::SharedURLLoaderFactory> GetSharedURLLoaderFactory();

  // Called when content creates a NetworkService. Creates the
  // SystemNetworkContext, if the network service is enabled.
  void OnNetworkServiceCreated(network::mojom::NetworkService* network_service);

  // Permanently disables QUIC, both for NetworkContexts using the IOThread's
  // NetworkService, and for those using the network service (if enabled).
  void DisableQuic();

  // Returns an SSLConfigClientRequest that can be passed as a
  // NetorkContextParam.
  network::mojom::SSLConfigClientRequest GetSSLConfigClientRequest();

  // Populates |initial_ssl_config| and |ssl_config_client_request| members of
  // |network_context_params|. As long as the SystemNetworkContextManager
  // exists, any NetworkContext created with the params will continue to get
  // SSL configuration updates.
  void AddSSLConfigToNetworkContextParams(
      network::mojom::NetworkContextParams* network_context_params);

  // Flushes all pending SSL configuration changes.
  void FlushSSLConfigManagerForTesting();

  // Flushes all pending proxy configuration changes.
  void FlushProxyConfigMonitorForTesting();

  // Call |FlushForTesting()| on Network Service related interfaces. For test
  // use only.
  void FlushNetworkInterfaceForTesting();

 private:
  class URLLoaderFactoryForSystem;

  void UpdateReferrersEnabled();

  // Creates parameters for the NetworkContext. May only be called once, since
  // it initializes some class members.
  network::mojom::NetworkContextParamsPtr CreateNetworkContextParams();

  // This is an instance of the default SSLConfigServiceManager for the current
  // platform and it gets SSL preferences from the BrowserProcess's local_state
  // object. It's shared with other NetworkContexts.
  std::unique_ptr<SSLConfigServiceManager> ssl_config_service_manager_;

  ProxyConfigMonitor proxy_config_monitor_;

  // NetworkContext using the network service, if the network service is
  // enabled. nullptr, otherwise.
  network::mojom::NetworkContextPtr network_service_network_context_;

  // This is a NetworkContext that wraps the IOThread's SystemURLRequestContext.
  // Always initialized in SetUp, but it's only returned by Context() when the
  // network service is disabled.
  network::mojom::NetworkContextPtr io_thread_network_context_;

  // URLLoaderFactory backed by the NetworkContext returned by GetContext(), so
  // consumers don't all need to create their own factory.
  scoped_refptr<URLLoaderFactoryForSystem> shared_url_loader_factory_;
  network::mojom::URLLoaderFactoryPtr url_loader_factory_;

  bool is_quic_allowed_ = true;

  PrefChangeRegistrar pref_change_registrar_;

  BooleanPrefMember enable_referrers_;

  DISALLOW_COPY_AND_ASSIGN(SystemNetworkContextManager);
};

#endif  // CHROME_BROWSER_NET_SYSTEM_NETWORK_CONTEXT_MANAGER_H_
