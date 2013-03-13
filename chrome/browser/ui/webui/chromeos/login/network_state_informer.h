// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_STATE_INFORMER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_STATE_INFORMER_H_

#include <map>
#include <string>

#include "base/cancelable_callback.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/captive_portal_window_proxy.h"
#include "chrome/browser/chromeos/net/network_portal_detector.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"

namespace chromeos {

class NetworkStateInformerDelegate {
 public:
  NetworkStateInformerDelegate() {}
  virtual ~NetworkStateInformerDelegate() {}

  // Called when network is connected.
  virtual void OnNetworkReady() = 0;
};

// Class which observes network state changes and calls registered callbacks.
// State is considered changed if connection or the active network has been
// changed. Also, it answers to the requests about current network state.
class NetworkStateInformer
    : public chromeos::NetworkLibrary::NetworkManagerObserver,
      public chromeos::NetworkPortalDetector::Observer,
      public content::NotificationObserver,
      public CaptivePortalWindowProxyDelegate,
      public base::RefCounted<NetworkStateInformer> {
 public:
  enum State {
    OFFLINE = 0,
    ONLINE,
    CAPTIVE_PORTAL,
    CONNECTING,
    PROXY_AUTH_REQUIRED,
    UNKNOWN
  };

  class NetworkStateInformerObserver {
   public:
    NetworkStateInformerObserver() {}
    virtual ~NetworkStateInformerObserver() {}

    virtual void UpdateState(State state,
                             const std::string& service_path,
                             ConnectionType connection_type,
                             const std::string& reason) = 0;
  };

  NetworkStateInformer();

  void Init();

  void SetDelegate(NetworkStateInformerDelegate* delegate);

  // Adds observer to be notified when network state has been changed.
  void AddObserver(NetworkStateInformerObserver* observer);

  // Removes observer.
  void RemoveObserver(NetworkStateInformerObserver* observer);

  // NetworkLibrary::NetworkManagerObserver implementation:
  virtual void OnNetworkManagerChanged(chromeos::NetworkLibrary* cros) OVERRIDE;

  // NetworkPortalDetector::Observer implementation:
  virtual void OnPortalStateChanged(
      const Network* network,
      const NetworkPortalDetector::CaptivePortalState& state) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // CaptivePortalWindowProxyDelegate implementation:
  virtual void OnPortalDetected() OVERRIDE;

  // Returns active network's service path. It can be used to uniquely
  // identify the network.
  std::string active_network_service_path() {
    return last_online_service_path_;
  }

  bool is_online() { return state_ == ONLINE; }
  State state() const { return state_; }
  std::string last_network_service_path() const {
    return last_network_service_path_;
  }
  ConnectionType last_network_type() const { return last_network_type_; }

 private:
  struct ProxyState {
    ProxyState() : configured(false) {
    }

    ProxyState(const std::string& proxy_config, bool configured)
        : proxy_config(proxy_config),
          configured(configured) {
    }

    std::string proxy_config;
    bool configured;
  };

  typedef std::map<std::string, ProxyState> ProxyStateMap;

  friend class base::RefCounted<NetworkStateInformer>;

  virtual ~NetworkStateInformer();

  bool UpdateState(chromeos::NetworkLibrary* cros);

  void UpdateStateAndNotify();

  void SendStateToObservers(const std::string& reason);

  State GetNetworkState(const Network* network);
  bool IsRestrictedPool(const Network* network);
  bool IsProxyConfigured(const Network* network);
  bool ProxyAuthRequired(const Network* network);

  content::NotificationRegistrar registrar_;
  State state_;
  NetworkStateInformerDelegate* delegate_;
  ObserverList<NetworkStateInformerObserver> observers_;
  std::string last_online_service_path_;
  std::string last_connected_service_path_;
  std::string last_network_service_path_;
  ConnectionType last_network_type_;
  base::CancelableClosure check_state_;

  // Caches proxy state for active networks.
  ProxyStateMap proxy_state_map_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_STATE_INFORMER_H_
