// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PROXY_CONFIG_SERVICE_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_PROXY_CONFIG_SERVICE_IMPL_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "components/onc/onc_constants.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"

namespace chromeos {

class NetworkState;

// Implementation of proxy config service for chromeos that:
// - extends PrefProxyConfigTrackerImpl (and so lives and runs entirely on UI
//   thread) to handle proxy from prefs (via PrefProxyConfigTrackerImpl) and
//   system i.e. network (via shill notifications)
// - exists one per profile and one per local state
// - persists proxy setting per network in flimflim
// - provides network stack with latest effective proxy configuration for
//   currently active network via PrefProxyConfigTrackerImpl's mechanism of
//   pushing config to ChromeProxyConfigService
class ProxyConfigServiceImpl : public PrefProxyConfigTrackerImpl,
                               public NetworkStateHandlerObserver {
 public:
  // ProxyConfigServiceImpl is created in ProxyServiceFactory::
  // CreatePrefProxyConfigTrackerImpl via Profile::GetProxyConfigTracker() for
  // profile or via IOThread constructor for local state and is owned by the
  // respective classes.
  //
  // The user's proxy config, proxy policies and proxy from prefs, are used to
  // determine the effective proxy config, which is then pushed through
  // PrefProxyConfigTrackerImpl to ChromeProxyConfigService to the
  // network stack.
  //
  // |profile_prefs| can be NULL if this object should only track prefs from
  // local state (e.g., for system request context or sigin-in screen).
  explicit ProxyConfigServiceImpl(PrefService* profile_prefs,
                                  PrefService* local_state_prefs);
  ~ProxyConfigServiceImpl() override;

  // PrefProxyConfigTrackerImpl implementation.
  void OnProxyConfigChanged(ProxyPrefs::ConfigState config_state,
                            const net::ProxyConfig& config) override;

  // NetworkStateHandlerObserver implementation.
  void DefaultNetworkChanged(const NetworkState* network) override;
  void OnShuttingDown() override;

 protected:
  friend class UIProxyConfigService;

  // Returns true if proxy is to be ignored for this network profile and
  // |onc_source|, e.g. this happens if the network is shared and
  // use-shared-proxies is turned off. |profile_prefs| may be NULL.
  static bool IgnoreProxy(const PrefService* profile_prefs,
                          const std::string network_profile_path,
                          onc::ONCSource onc_source);

 private:
  // Called when any proxy preference changes.
  void OnProxyPrefChanged();

  // Determines effective proxy config based on prefs from config tracker, the
  // current default network and if user is using shared proxies.  The effective
  // config is stored in |active_config_| and activated on network stack, and
  // hence, picked up by observers.
  void DetermineEffectiveConfigFromDefaultNetwork();

  // State of |active_config_|.  |active_config_| is only valid if
  // |active_config_state_| is not ProxyPrefs::CONFIG_UNSET.
  ProxyPrefs::ConfigState active_config_state_;

  // Active proxy configuration, which could be from prefs or network.
  net::ProxyConfig active_config_;

  // Track changes in profile preferences: UseSharedProxies and
  // OpenNetworkConfiguration.
  PrefChangeRegistrar profile_pref_registrar_;

  // Track changes in local state preferences: DeviceOpenNetworkConfiguration.
  PrefChangeRegistrar local_state_pref_registrar_;

  // Not owned. NULL if tracking only local state prefs (e.g. in the system
  // request context or sign-in screen).
  PrefService* profile_prefs_;

  // Not owned.
  PrefService* local_state_prefs_;

  base::WeakPtrFactory<ProxyConfigServiceImpl> pointer_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProxyConfigServiceImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PROXY_CONFIG_SERVICE_IMPL_H_
