// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_PREF_PROXY_CONFIG_TRACKER_IMPL_H_
#define CHROME_BROWSER_NET_PREF_PROXY_CONFIG_TRACKER_IMPL_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/prefs/proxy_config_dictionary.h"
#include "content/public/browser/notification_observer.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_config_service.h"

class PrefService;
class PrefSetObserver;

// A net::ProxyConfigService implementation that applies preference proxy
// settings (pushed from PrefProxyConfigTrackerImpl) as overrides to the proxy
// configuration determined by a baseline delegate ProxyConfigService on
// non-ChromeOS platforms. ChromeOS has its own implementation of overrides in
// chromeos::ProxyConfigServiceImpl.
class ChromeProxyConfigService
    : public net::ProxyConfigService,
      public net::ProxyConfigService::Observer {
 public:
  // Takes ownership of the passed |base_service|.
  explicit ChromeProxyConfigService(net::ProxyConfigService* base_service);
  virtual ~ChromeProxyConfigService();

  // ProxyConfigService implementation:
  virtual void AddObserver(
      net::ProxyConfigService::Observer* observer) OVERRIDE;
  virtual void RemoveObserver(
      net::ProxyConfigService::Observer* observer) OVERRIDE;
  virtual ConfigAvailability GetLatestProxyConfig(
      net::ProxyConfig* config) OVERRIDE;
  virtual void OnLazyPoll() OVERRIDE;

  // Method on IO thread that receives the preference proxy settings pushed from
  // PrefProxyConfigTrackerImpl.
  void UpdateProxyConfig(ProxyPrefs::ConfigState config_state,
                         const net::ProxyConfig& config);

 private:
  // ProxyConfigService::Observer implementation:
  virtual void OnProxyConfigChanged(const net::ProxyConfig& config,
                                    ConfigAvailability availability) OVERRIDE;

  // Makes sure that the observer registration with the base service is set up.
  void RegisterObserver();

  scoped_ptr<net::ProxyConfigService> base_service_;
  ObserverList<net::ProxyConfigService::Observer, true> observers_;

  // Tracks configuration state of |pref_config_|. |pref_config_| is valid only
  // if |pref_config_state_| is not CONFIG_UNSET.
  ProxyPrefs::ConfigState pref_config_state_;

  // Configuration as defined by prefs.
  net::ProxyConfig pref_config_;

  // Indicates whether the base service registration is done.
  bool registered_observer_;

  DISALLOW_COPY_AND_ASSIGN(ChromeProxyConfigService);
};

// A class that tracks proxy preferences. It translates the configuration
// to net::ProxyConfig and pushes the result over to the IO thread for
// ChromeProxyConfigService::UpdateProxyConfig to use.
class PrefProxyConfigTrackerImpl : public content::NotificationObserver {
 public:
  explicit PrefProxyConfigTrackerImpl(PrefService* pref_service);
  virtual ~PrefProxyConfigTrackerImpl();

  // Sets the proxy config service to push the preference proxy to.
  void SetChromeProxyConfigService(
      ChromeProxyConfigService* proxy_config_service);

  // Notifies the tracker that the pref service passed upon construction is
  // about to go away. This must be called from the UI thread.
  void DetachFromPrefService();

  // Determines if |config_state| takes precedence regardless, which happens if
  // config is from policy or extension or other-precede.
  static bool PrefPrecedes(ProxyPrefs::ConfigState config_state);

  // Determines the proxy configuration that should take effect in the network
  // layer, based on prefs and system configurations.
  // |pref_state| refers to state of |pref_config|.
  // |system_availability| refers to availability of |system_config|.
  // |ignore_fallback_config| indicates if fallback config from prefs should
  // be ignored.
  // Returns effective |effective_config| and its state in
  // |effective_config_source|.
  static net::ProxyConfigService::ConfigAvailability GetEffectiveProxyConfig(
      ProxyPrefs::ConfigState pref_state,
      const net::ProxyConfig& pref_config,
      net::ProxyConfigService::ConfigAvailability system_availability,
      const net::ProxyConfig& system_config,
      bool ignore_fallback_config,
      ProxyPrefs::ConfigState* effective_config_state,
      net::ProxyConfig* effective_config);

  // Registers the proxy preference.
  static void RegisterPrefs(PrefService* user_prefs);

 protected:
  // Get the proxy configuration currently defined by preferences.
  // Status is indicated in the return value.
  // Writes the configuration to |config| unless the return value is
  // CONFIG_UNSET, in which case |config| and |config_source| are not touched.
  ProxyPrefs::ConfigState GetProxyConfig(net::ProxyConfig* config);

  // Called when there's a change in prefs proxy config.
  // Subclasses can extend it for changes in other sources of proxy config.
  virtual void OnProxyConfigChanged(ProxyPrefs::ConfigState config_state,
                                    const net::ProxyConfig& config);

  // content::NotificationObserver implementation:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Converts a ProxyConfigDictionary to net::ProxyConfig representation.
  // Returns true if the data from in the dictionary is valid, false otherwise.
  bool PrefConfigToNetConfig(const ProxyConfigDictionary& proxy_dict,
                             net::ProxyConfig* config);

  const PrefService* prefs() const { return pref_service_; }
  bool update_pending() const { return update_pending_; }

 private:
  // Creates a proxy configuration from proxy-related preferences. Configuration
  // is stored in |config|, return value indicates whether the configuration is
  // valid.
  ProxyPrefs::ConfigState ReadPrefConfig(net::ProxyConfig* config);

  // Tracks configuration state. |pref_config_| is valid only if |config_state_|
  // is not CONFIG_UNSET.
  ProxyPrefs::ConfigState config_state_;

  // Configuration as defined by prefs.
  net::ProxyConfig pref_config_;

  PrefService* pref_service_;
  ChromeProxyConfigService* chrome_proxy_config_service_;  // Weak ptr.
  bool update_pending_;  // True if config has not been pushed to network stack.
  scoped_ptr<PrefSetObserver> proxy_prefs_observer_;

  DISALLOW_COPY_AND_ASSIGN(PrefProxyConfigTrackerImpl);
};

#endif  // CHROME_BROWSER_NET_PREF_PROXY_CONFIG_TRACKER_IMPL_H_
