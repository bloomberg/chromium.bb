// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/networking_private/networking_private_event_router.h"

#include "base/json/json_writer.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_state_handler_observer.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_translator.h"
#include "chromeos/network/portal_detector/network_portal_detector.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/onc/onc_constants.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/api/networking_private/networking_private_api.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/api/networking_private.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using chromeos::NetworkHandler;
using chromeos::NetworkPortalDetector;
using chromeos::NetworkState;
using chromeos::NetworkStateHandler;

namespace extensions {

class NetworkingPrivateEventRouterImpl
    : public NetworkingPrivateEventRouter,
      public chromeos::NetworkStateHandlerObserver,
      public NetworkPortalDetector::Observer {
 public:
  explicit NetworkingPrivateEventRouterImpl(content::BrowserContext* context);
  ~NetworkingPrivateEventRouterImpl() override;

 protected:
  // KeyedService overrides:
  void Shutdown() override;

  // EventRouter::Observer overrides:
  void OnListenerAdded(const EventListenerInfo& details) override;
  void OnListenerRemoved(const EventListenerInfo& details) override;

  // NetworkStateHandlerObserver overrides:
  void NetworkListChanged() override;
  void NetworkPropertiesUpdated(const NetworkState* network) override;

  // NetworkPortalDetector::Observer overrides:
  void OnPortalDetectionCompleted(
      const NetworkState* network,
      const NetworkPortalDetector::CaptivePortalState& state) override;

 private:
  // Decide if we should listen for network changes or not. If there are any
  // JavaScript listeners registered for the onNetworkChanged event, then we
  // want to register for change notification from the network state handler.
  // Otherwise, we want to unregister and not be listening to network changes.
  void StartOrStopListeningForNetworkChanges();

  content::BrowserContext* context_;
  bool listening_;

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateEventRouterImpl);
};

NetworkingPrivateEventRouterImpl::NetworkingPrivateEventRouterImpl(
    content::BrowserContext* context)
    : context_(context), listening_(false) {
  // Register with the event router so we know when renderers are listening to
  // our events. We first check and see if there *is* an event router, because
  // some unit tests try to create all context services, but don't initialize
  // the event router first.
  EventRouter* event_router = EventRouter::Get(context_);
  if (event_router) {
    event_router->RegisterObserver(
        this, core_api::networking_private::OnNetworksChanged::kEventName);
    event_router->RegisterObserver(
        this, core_api::networking_private::OnNetworkListChanged::kEventName);
    event_router->RegisterObserver(
        this,
        core_api::networking_private::OnPortalDetectionCompleted::kEventName);
    StartOrStopListeningForNetworkChanges();
  }
}

NetworkingPrivateEventRouterImpl::~NetworkingPrivateEventRouterImpl() {
  DCHECK(!listening_);
}

void NetworkingPrivateEventRouterImpl::Shutdown() {
  // Unregister with the event router. We first check and see if there *is* an
  // event router, because some unit tests try to shutdown all context services,
  // but didn't initialize the event router first.
  EventRouter* event_router = EventRouter::Get(context_);
  if (event_router)
    event_router->UnregisterObserver(this);

  if (listening_) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this,
                                                                   FROM_HERE);
  }
  listening_ = false;
}

void NetworkingPrivateEventRouterImpl::OnListenerAdded(
    const EventListenerInfo& details) {
  // Start listening to events from the network state handler.
  StartOrStopListeningForNetworkChanges();
}

void NetworkingPrivateEventRouterImpl::OnListenerRemoved(
    const EventListenerInfo& details) {
  // Stop listening to events from the network state handler if there are no
  // more listeners.
  StartOrStopListeningForNetworkChanges();
}

void NetworkingPrivateEventRouterImpl::StartOrStopListeningForNetworkChanges() {
  EventRouter* event_router = EventRouter::Get(context_);
  bool should_listen =
      event_router->HasEventListener(
          core_api::networking_private::OnNetworksChanged::kEventName) ||
      event_router->HasEventListener(
          core_api::networking_private::OnNetworkListChanged::kEventName) ||
      event_router->HasEventListener(
          core_api::networking_private::OnPortalDetectionCompleted::kEventName);

  if (should_listen && !listening_) {
    NetworkHandler::Get()->network_state_handler()->AddObserver(this,
                                                                FROM_HERE);
    if (chromeos::NetworkPortalDetector::IsInitialized())
      NetworkPortalDetector::Get()->AddObserver(this);
  } else if (!should_listen && listening_) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(this,
                                                                   FROM_HERE);
    if (chromeos::NetworkPortalDetector::IsInitialized())
      NetworkPortalDetector::Get()->RemoveObserver(this);
  }
  listening_ = should_listen;
}

void NetworkingPrivateEventRouterImpl::NetworkListChanged() {
  EventRouter* event_router = EventRouter::Get(context_);
  NetworkStateHandler::NetworkStateList networks;
  NetworkHandler::Get()->network_state_handler()->GetVisibleNetworkList(
      &networks);
  if (!event_router->HasEventListener(
          core_api::networking_private::OnNetworkListChanged::kEventName)) {
    // TODO(stevenjb): Remove logging once crbug.com/256881 is fixed
    // (or at least reduce to LOG_DEBUG). Same with NET_LOG events below.
    NET_LOG_EVENT("NetworkingPrivate.NetworkListChanged: No Listeners", "");
    return;
  }

  NET_LOG_EVENT("NetworkingPrivate.NetworkListChanged", "");

  std::vector<std::string> changes;
  for (NetworkStateHandler::NetworkStateList::const_iterator iter =
           networks.begin();
       iter != networks.end(); ++iter) {
    changes.push_back((*iter)->guid());
  }

  scoped_ptr<base::ListValue> args(
      core_api::networking_private::OnNetworkListChanged::Create(changes));
  scoped_ptr<Event> extension_event(
      new Event(core_api::networking_private::OnNetworkListChanged::kEventName,
                args.Pass()));
  event_router->BroadcastEvent(extension_event.Pass());
}

void NetworkingPrivateEventRouterImpl::NetworkPropertiesUpdated(
    const NetworkState* network) {
  EventRouter* event_router = EventRouter::Get(context_);
  if (!event_router->HasEventListener(
          core_api::networking_private::OnNetworksChanged::kEventName)) {
    NET_LOG_EVENT("NetworkingPrivate.NetworkPropertiesUpdated: No Listeners",
                  network->path());
    return;
  }
  NET_LOG_EVENT("NetworkingPrivate.NetworkPropertiesUpdated", network->path());
  scoped_ptr<base::ListValue> args(
      core_api::networking_private::OnNetworksChanged::Create(
          std::vector<std::string>(1, network->guid())));
  scoped_ptr<Event> extension_event(
      new Event(core_api::networking_private::OnNetworksChanged::kEventName,
                args.Pass()));
  event_router->BroadcastEvent(extension_event.Pass());
}

void NetworkingPrivateEventRouterImpl::OnPortalDetectionCompleted(
    const NetworkState* network,
    const NetworkPortalDetector::CaptivePortalState& state) {
  const std::string path = network ? network->guid() : std::string();

  EventRouter* event_router = EventRouter::Get(context_);
  if (!event_router->HasEventListener(
          core_api::networking_private::OnPortalDetectionCompleted::
              kEventName)) {
    NET_LOG_EVENT("NetworkingPrivate.OnPortalDetectionCompleted: No Listeners",
                  path);
    return;
  }
  NET_LOG_EVENT("NetworkingPrivate.OnPortalDetectionCompleted", path);

  core_api::networking_private::CaptivePortalStatus status =
      core_api::networking_private::CAPTIVE_PORTAL_STATUS_UNKNOWN;
  switch (state.status) {
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_UNKNOWN:
      status = core_api::networking_private::CAPTIVE_PORTAL_STATUS_UNKNOWN;
      break;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE:
      status = core_api::networking_private::CAPTIVE_PORTAL_STATUS_OFFLINE;
      break;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE:
      status = core_api::networking_private::CAPTIVE_PORTAL_STATUS_ONLINE;
      break;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PORTAL:
      status = core_api::networking_private::CAPTIVE_PORTAL_STATUS_PORTAL;
      break;
    case NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED:
      status =
          core_api::networking_private::CAPTIVE_PORTAL_STATUS_PROXYAUTHREQUIRED;
      break;
    default:
      NOTREACHED();
      break;
  }

  scoped_ptr<base::ListValue> args(
      core_api::networking_private::OnPortalDetectionCompleted::Create(path,
                                                                       status));
  scoped_ptr<Event> extension_event(new Event(
      core_api::networking_private::OnPortalDetectionCompleted::kEventName,
      args.Pass()));
  event_router->BroadcastEvent(extension_event.Pass());
}

NetworkingPrivateEventRouter* NetworkingPrivateEventRouter::Create(
    content::BrowserContext* context) {
  return new NetworkingPrivateEventRouterImpl(context);
}

}  // namespace extensions
