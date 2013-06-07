// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_PROXY_CONFIG_SERVICE_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_PROXY_CONFIG_SERVICE_IMPL_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/prefs/pref_member.h"
#include "chrome/browser/net/pref_proxy_config_tracker_impl.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "chromeos/network/onc/onc_constants.h"

class PrefRegistrySimple;

namespace user_prefs {
class PrefRegistrySyncable;
}

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
  // profile or IOThread constructor for local state and is owned by the
  // respective classes.
  //
  // The new modified proxy config, together with proxy from prefs if available,
  // are used to determine the effective proxy config, which is then pushed
  // through PrefProxyConfigTrackerImpl to ChromeProxyConfigService to the
  // network stack.
  explicit ProxyConfigServiceImpl(PrefService* pref_service);
  virtual ~ProxyConfigServiceImpl();

  // PrefProxyConfigTrackerImpl implementation.
  virtual void OnProxyConfigChanged(ProxyPrefs::ConfigState config_state,
                                    const net::ProxyConfig& config) OVERRIDE;

  // NetworkStateHandlerObserver implementation.
  virtual void DefaultNetworkChanged(const NetworkState* network) OVERRIDE;

  // Register UseShardProxies preference.
  static void RegisterPrefs(PrefRegistrySimple* registry);
  static void RegisterUserPrefs(user_prefs::PrefRegistrySyncable* registry);

 protected:
  friend class UIProxyConfigService;

  // Returns value of UseSharedProxies preference if it's not default, else
  // returns false if user is logged in and true otherwise.
  static bool GetUseSharedProxies(const PrefService* pref_service);

  // Returns true if proxy is to be ignored for this profile and |onc_source|,
  // e.g. this happens if the network is shared and use-shared-proxies is turned
  // off.
  static bool IgnoreProxy(const PrefService* pref_service,
                          const std::string network_profile_path,
                          onc::ONCSource onc_source);

 private:
  // Called when the kUseSharedProxies preference changes.
  void OnUseSharedProxiesChanged();

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

  // Track changes in UseSharedProxies user preference.
  BooleanPrefMember use_shared_proxies_;

  base::WeakPtrFactory<ProxyConfigServiceImpl> pointer_factory_;

  DISALLOW_COPY_AND_ASSIGN(ProxyConfigServiceImpl);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_PROXY_CONFIG_SERVICE_IMPL_H_
