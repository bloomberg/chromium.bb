// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_PREF_PROXY_CONFIG_SERVICE_H_
#define CHROME_BROWSER_NET_PREF_PROXY_CONFIG_SERVICE_H_
#pragma once

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
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

  explicit PrefProxyConfigTracker(PrefService* pref_service);
  virtual ~PrefProxyConfigTracker();

  // Observer manipulation is only valid on the IO thread.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Get the proxy configuration currently defined by preferences. Writes the
  // configuration to |config| and returns true on success. |config| is not
  // touched and false is returned if there is no configuration defined. This
  // must be called on the IO thread.
  bool GetProxyConfig(net::ProxyConfig* config);

  // Notifies the tracker that the pref service passed upon construction is
  // about to go away. This must be called from the UI thread.
  void DetachFromPrefService();

 private:
  // NotificationObserver implementation:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // Install a new configuration. This is invoked on the IO thread to update
  // the internal state after handling a pref change on the UI thread. |valid|
  // indicates whether there is a preference proxy configuration defined, in
  // which case this configuration is given by |config|. |config| is ignored if
  // |valid| is false.
  void InstallProxyConfig(const net::ProxyConfig& config, bool valid);

  // Creates a proxy configuration from proxy-related preferences. Configuration
  // is stored in |config| and the return value indicates whether the
  // configuration is valid.
  bool ReadPrefConfig(net::ProxyConfig* config);

  // Configuration as defined by prefs. Only to be accessed from the IO thread
  // (except for construction).
  net::ProxyConfig pref_config_;

  // Whether |pref_config_| is valid. Only accessed from the IO thread.
  bool valid_;

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
  virtual bool GetLatestProxyConfig(net::ProxyConfig* config);
  virtual void OnLazyPoll();

  static void RegisterPrefs(PrefService* user_prefs);

 private:
  // ProxyConfigService::Observer implementation:
  virtual void OnProxyConfigChanged(const net::ProxyConfig& config);

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
