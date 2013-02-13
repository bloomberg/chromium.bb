// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_NETWORKING_PRIVATE_EVENT_ROUTER_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_NETWORKING_PRIVATE_EVENT_ROUTER_H_

#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chromeos/network/network_state_handler_observer.h"

class Profile;

namespace chromeos {

// This is a factory class used by the ProfileDependencyManager to instantiate
// the event router that will forward events from the NetworkStateHandler to the
// JavaScript Networking API.
class NetworkingPrivateEventRouter : public ProfileKeyedService,
                                     public extensions::EventRouter::Observer,
                                     public NetworkStateHandlerObserver {
 public:
  explicit NetworkingPrivateEventRouter(Profile* profile);
  virtual ~NetworkingPrivateEventRouter();

 protected:
  // ProfileKeyedService overrides:
  virtual void Shutdown() OVERRIDE;

  // EventRouter::Observer overrides:
  virtual void OnListenerAdded(
      const extensions::EventListenerInfo& details) OVERRIDE;
  virtual void OnListenerRemoved(
      const extensions::EventListenerInfo& details) OVERRIDE;

  // NetworkStateHandlerObserver overrides:
  virtual void NetworkListChanged() OVERRIDE;
  virtual void NetworkPropertiesUpdated(const NetworkState* network) OVERRIDE;

 private:
  // Decide if we should listen for network changes or not. If there are any
  // JavaScript listeners registered for the onNetworkChanged event, then we
  // want to register for change notification from the network state handler.
  // Otherwise, we want to unregister and not be listening to network changes.
  void StartOrStopListeningForNetworkChanges();

  Profile* profile_;
  bool listening_;

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateEventRouter);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_NETWORKING_PRIVATE_EVENT_ROUTER_H_
