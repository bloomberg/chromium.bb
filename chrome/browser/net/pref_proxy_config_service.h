// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_PREF_PROXY_CONFIG_SERVICE_H_
#define CHROME_BROWSER_NET_PREF_PROXY_CONFIG_SERVICE_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/prefs/proxy_config_dictionary.h"
#include "content/common/notification_observer.h"
#include "net/proxy/proxy_config.h"
#include "net/proxy/proxy_config_service.h"

class PrefService;
class PrefSetObserver;

// A helper class that tracks proxy preferences. It translates the configuration
// to net::ProxyConfig and proxies the result over to the IO thread for
// PrefProxyConfigService to use.
class PrefProxyConfigTracker
    : public base::RefCountedThreadSafe<PrefProxyConfigTracker>,
      public NotificationObserver {
 public:
  // Observer interface used to send out notifications on the IO thread about
  // changes to the proxy configuration.
  class Observer {
   public:
    virtual ~Observer() {}
    virtual void OnPrefProxyConfigChanged() = 0;
  };

  // Return codes for GetProxyConfig.
  enum ConfigState {
    // Configuration is valid and present.
    CONFIG_PRESENT,
    // There is a fallback configuration present.
    CONFIG_FALLBACK,
    // Configuration is known to be not set.
    CONFIG_UNSET,
  };

  explicit PrefProxyConfigTracker(PrefService* pref_service);

  // Observer manipulation is only valid on the IO thread.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Get the proxy configuration currently defined by preferences. Status is
  // indicated in the return value. Writes the configuration to |config| unless
  // the return value is CONFIG_UNSET, in which case |config| is not touched.
  ConfigState GetProxyConfig(net::ProxyConfig* config);

  // Notifies the tracker that the pref service passed upon construction is
  // about to go away. This must be called from the UI thread.
  void DetachFromPrefService();

 private:
  friend class base::RefCountedThreadSafe<PrefProxyConfigTracker>;
  virtual ~PrefProxyConfigTracker();

  // NotificationObserver implementation:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Install a new configuration. This is invoked on the IO thread to update
  // the internal state after handling a pref change on the UI thread.
  // |config_state| indicates the new state we're switching to, and |config| is
  // the new preference-based proxy configuration if |config_state| is different
  // from CONFIG_UNSET.
  void InstallProxyConfig(const net::ProxyConfig& config, ConfigState state);

  // Creates a proxy configuration from proxy-related preferences. Configuration
  // is stored in |config| and the return value indicates whether the
  // configuration is valid.
  ConfigState ReadPrefConfig(net::ProxyConfig* config);

  // Converts a ProxyConfigDictionary to net::ProxyConfig representation.
  // Returns true if the data from in the dictionary is valid, false otherwise.
  static bool PrefConfigToNetConfig(const ProxyConfigDictionary& proxy_dict,
                                    net::ProxyConfig* config);

  // Configuration as defined by prefs. Only to be accessed from the IO thread
  // (except for construction).
  net::ProxyConfig pref_config_;

  // Tracks configuration state. |pref_config_| is valid only if |config_state_|
  // is not CONFIG_UNSET.
  ConfigState config_state_;

  // List of observers, accessed exclusively from the IO thread.
  ObserverList<Observer, true> observers_;

  // Pref-related members that should only be accessed from the UI thread.
  PrefService* pref_service_;
  scoped_ptr<PrefSetObserver> proxy_prefs_observer_;

  DISALLOW_COPY_AND_ASSIGN(PrefProxyConfigTracker);
};

// A net::ProxyConfigService implementation that applies preference proxy
// settings as overrides to the proxy configuration determined by a baseline
// delegate ProxyConfigService.
class PrefProxyConfigService
    : public net::ProxyConfigService,
      public net::ProxyConfigService::Observer,
      public PrefProxyConfigTracker::Observer {
 public:
  // Takes ownership of the passed |base_service|.
  PrefProxyConfigService(PrefProxyConfigTracker* tracker,
                         net::ProxyConfigService* base_service);
  virtual ~PrefProxyConfigService();

  // ProxyConfigService implementation:
  virtual void AddObserver(net::ProxyConfigService::Observer* observer);
  virtual void RemoveObserver(net::ProxyConfigService::Observer* observer);
  virtual ConfigAvailability GetLatestProxyConfig(net::ProxyConfig* config);
  virtual void OnLazyPoll();

  static void RegisterPrefs(PrefService* user_prefs);

 private:
  // ProxyConfigService::Observer implementation:
  virtual void OnProxyConfigChanged(const net::ProxyConfig& config,
                                    ConfigAvailability availability);

  // PrefProxyConfigTracker::Observer implementation:
  virtual void OnPrefProxyConfigChanged();

  // Makes sure that the observer registrations with the base service and the
  // tracker object are set up.
  void RegisterObservers();

  scoped_ptr<net::ProxyConfigService> base_service_;
  ObserverList<net::ProxyConfigService::Observer, true> observers_;
  scoped_refptr<PrefProxyConfigTracker> pref_config_tracker_;

  // Indicates whether the base service and tracker registrations are done.
  bool registered_observers_;

  DISALLOW_COPY_AND_ASSIGN(PrefProxyConfigService);
};

#endif  // CHROME_BROWSER_NET_PREF_PROXY_CONFIG_SERVICE_H_
