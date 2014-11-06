// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_STATE_INFORMER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_STATE_INFORMER_H_

#include <map>
#include <string>

#include "base/cancelable_callback.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/login/screens/error_screen_actor.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"

#if !defined(USE_ATHENA)
// TODO(ygorshenin): Figure out what the captive portal should look like
// on athena (crbug.com/430300)
#include "chrome/browser/chromeos/login/ui/captive_portal_window_proxy.h"
#endif

namespace chromeos {

// Class which observes network state changes and calls registered callbacks.
// State is considered changed if connection or the active network has been
// changed. Also, it answers to the requests about current network state.
class NetworkStateInformer
    : public chromeos::NetworkStateHandlerObserver,
      public chromeos::NetworkPortalDetector::Observer,
      public content::NotificationObserver,
#if !defined(USE_ATHENA)
      public CaptivePortalWindowProxyDelegate,
#endif
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

    virtual void UpdateState(ErrorScreenActor::ErrorReason reason) = 0;
    virtual void OnNetworkReady() {}
  };

  NetworkStateInformer();

  void Init();

  // Adds observer to be notified when network state has been changed.
  void AddObserver(NetworkStateInformerObserver* observer);

  // Removes observer.
  void RemoveObserver(NetworkStateInformerObserver* observer);

  // NetworkStateHandlerObserver implementation:
  virtual void DefaultNetworkChanged(const NetworkState* network) override;

  // NetworkPortalDetector::Observer implementation:
  virtual void OnPortalDetectionCompleted(
      const NetworkState* network,
      const NetworkPortalDetector::CaptivePortalState& state) override;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) override;

#if !defined(USE_ATHENA)
  // CaptivePortalWindowProxyDelegate implementation:
  virtual void OnPortalDetected() override;
#endif

  State state() const { return state_; }
  std::string network_path() const { return network_path_; }
  std::string network_type() const { return network_type_; }

  static const char* StatusString(State state);

 private:
  friend class base::RefCounted<NetworkStateInformer>;

  virtual ~NetworkStateInformer();

  bool UpdateState();

  void UpdateStateAndNotify();

  void SendStateToObservers(ErrorScreenActor::ErrorReason reason);

  State state_;
  std::string network_path_;
  std::string network_type_;

  ObserverList<NetworkStateInformerObserver> observers_;
  content::NotificationRegistrar registrar_;

  base::WeakPtrFactory<NetworkStateInformer> weak_ptr_factory_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_NETWORK_STATE_INFORMER_H_
