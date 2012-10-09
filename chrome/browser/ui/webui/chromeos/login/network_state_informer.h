// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_STATE_INFORMER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_STATE_INFORMER_H_

#include <string>

#include "base/cancelable_callback.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/cros/network_constants.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/login/captive_portal_window_proxy.h"
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
      public content::NotificationObserver,
      public CaptivePortalWindowProxyDelegate,
      public base::RefCounted<NetworkStateInformer> {
 public:
  enum State {
    OFFLINE = 0,
    ONLINE,
    CAPTIVE_PORTAL,
    CONNECTING
  };

  class NetworkStateInformerObserver {
   public:
    NetworkStateInformerObserver() {}
    virtual ~NetworkStateInformerObserver() {}

    virtual void UpdateState(State state,
                             const std::string& network_name,
                             const std::string& reason,
                             ConnectionType last_network_type) = 0;
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

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // CaptivePortalWindowProxyDelegate implementation:
  virtual void OnPortalDetected() OVERRIDE;

  // Returns active network's ID. It can be used to uniquely
  // identify the network.
  std::string active_network_id() {
    return last_online_network_id_;
  }

  bool is_online() { return state_ == ONLINE; }
  State state() const { return state_; }
  std::string network_name() const { return network_name_; }
  ConnectionType last_network_type() const { return last_network_type_; }

 private:
  friend class base::RefCounted<NetworkStateInformer>;

  virtual ~NetworkStateInformer();

  bool UpdateState(chromeos::NetworkLibrary* cros);

  void UpdateStateAndNotify();

  void SendStateToObservers(const std::string& reason);

  content::NotificationRegistrar registrar_;
  State state_;
  NetworkStateInformerDelegate* delegate_;
  ObserverList<NetworkStateInformerObserver> observers_;
  ConnectionType last_network_type_;
  std::string network_name_;
  base::CancelableClosure check_state_;
  std::string last_online_network_id_;
  std::string last_connected_network_id_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_STATE_INFORMER_H_
