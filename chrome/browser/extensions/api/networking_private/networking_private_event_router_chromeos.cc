// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/networking_private/networking_private_event_router.h"

#include "base/json/json_writer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_api.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/networking_private.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_translator.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "components/onc/onc_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using extensions::EventRouter;
using extensions::ExtensionSystem;
namespace api = extensions::api::networking_private;

namespace chromeos {

NetworkingPrivateEventRouter::NetworkingPrivateEventRouter(Profile* profile)
    : profile_(profile), listening_(false) {
  // Register with the event router so we know when renderers are listening to
  // our events. We first check and see if there *is* an event router, because
  // some unit tests try to create all profile services, but don't initialize
  // the event router first.
  EventRouter* event_router = ExtensionSystem::Get(profile_)->event_router();
  if (event_router) {
    event_router->RegisterObserver(
        this, api::OnNetworksChanged::kEventName);
    event_router->RegisterObserver(
        this, api::OnNetworkListChanged::kEventName);
    StartOrStopListeningForNetworkChanges();
  }
}

NetworkingPrivateEventRouter::~NetworkingPrivateEventRouter() {
  DCHECK(!listening_);
}

void NetworkingPrivateEventRouter::Shutdown() {
  // Unregister with the event router. We first check and see if there *is* an
  // event router, because some unit tests try to shutdown all profile services,
  // but didn't initialize the event router first.
  EventRouter* event_router = ExtensionSystem::Get(profile_)->event_router();
  if (event_router)
    event_router->UnregisterObserver(this);

  if (listening_) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(
        this, FROM_HERE);
  }
  listening_ = false;
}

void NetworkingPrivateEventRouter::OnListenerAdded(
    const extensions::EventListenerInfo& details) {
  // Start listening to events from the network state handler.
  StartOrStopListeningForNetworkChanges();
}

void NetworkingPrivateEventRouter::OnListenerRemoved(
    const extensions::EventListenerInfo& details) {
  // Stop listening to events from the network state handler if there are no
  // more listeners.
  StartOrStopListeningForNetworkChanges();
}

void NetworkingPrivateEventRouter::StartOrStopListeningForNetworkChanges() {
  EventRouter* event_router = ExtensionSystem::Get(profile_)->event_router();
  bool should_listen =
      event_router->HasEventListener(api::OnNetworksChanged::kEventName) ||
      event_router->HasEventListener(api::OnNetworkListChanged::kEventName);

  if (should_listen && !listening_) {
    NetworkHandler::Get()->network_state_handler()->AddObserver(
        this, FROM_HERE);
  } else if (!should_listen && listening_) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(
        this, FROM_HERE);
  }
  listening_ = should_listen;
}

void NetworkingPrivateEventRouter::NetworkListChanged() {
  EventRouter* event_router = ExtensionSystem::Get(profile_)->event_router();
  NetworkStateHandler::NetworkStateList networks;
  NetworkHandler::Get()->network_state_handler()->GetNetworkList(&networks);
  if (!event_router->HasEventListener(api::OnNetworkListChanged::kEventName)) {
    // TODO(stevenjb): Remove logging once crbug.com/256881 is fixed
    // (or at least reduce to LOG_DEBUG). Same with NET_LOG events below.
    NET_LOG_EVENT("NetworkingPrivate.NetworkListChanged: No Listeners", "");
    return;
  }

  NET_LOG_EVENT("NetworkingPrivate.NetworkListChanged", "");

  std::vector<std::string> changes;
  for (NetworkStateHandler::NetworkStateList::const_iterator iter =
           networks.begin(); iter != networks.end(); ++iter) {
    // TODO(gspencer): Currently the "GUID" is actually the service path. Fix
    // this to be the real GUID once we're using
    // ManagedNetworkConfigurationManager.
    changes.push_back((*iter)->path());
  }

  scoped_ptr<base::ListValue> args(api::OnNetworkListChanged::Create(changes));
  scoped_ptr<extensions::Event> extension_event(new extensions::Event(
      api::OnNetworkListChanged::kEventName, args.Pass()));
  event_router->BroadcastEvent(extension_event.Pass());
}

void NetworkingPrivateEventRouter::NetworkPropertiesUpdated(
    const NetworkState* network) {
  EventRouter* event_router = ExtensionSystem::Get(profile_)->event_router();
  if (!event_router->HasEventListener(api::OnNetworksChanged::kEventName)) {
    NET_LOG_EVENT("NetworkingPrivate.NetworkPropertiesUpdated: No Listeners",
                  network->path());
    return;
  }
  NET_LOG_EVENT("NetworkingPrivate.NetworkPropertiesUpdated",
                network->path());
  scoped_ptr<base::ListValue> args(api::OnNetworksChanged::Create(
      std::vector<std::string>(1, network->path())));
  scoped_ptr<extensions::Event> extension_event(
      new extensions::Event(api::OnNetworksChanged::kEventName, args.Pass()));
  event_router->BroadcastEvent(extension_event.Pass());
}

}  // namespace chromeos

