// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/networking_private/networking_private_event_router.h"

#include "chrome/browser/extensions/api/networking_private/networking_private_api.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_service_client.h"
#include "chrome/browser/extensions/api/networking_private/networking_private_service_client_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/networking_private.h"

namespace extensions {

// This is an event router that will observe listeners to |NetworksChanged| and
// |NetworkListChanged| events.
class NetworkingPrivateEventRouterImpl
    : public NetworkingPrivateEventRouter,
      NetworkingPrivateServiceClient::Observer {
 public:
  explicit NetworkingPrivateEventRouterImpl(Profile* profile);
  virtual ~NetworkingPrivateEventRouterImpl();

 protected:
  // KeyedService overrides:
  virtual void Shutdown() OVERRIDE;

  // EventRouter::Observer overrides:
  virtual void OnListenerAdded(const EventListenerInfo& details) OVERRIDE;
  virtual void OnListenerRemoved(const EventListenerInfo& details) OVERRIDE;

  // NetworkingPrivateServiceClient::Observer overrides:
  virtual void OnNetworksChangedEvent(
      const std::vector<std::string>& network_guids) OVERRIDE;
  virtual void OnNetworkListChangedEvent(
      const std::vector<std::string>& network_guids) OVERRIDE;

 private:
  // Decide if we should listen for network changes or not. If there are any
  // JavaScript listeners registered for the onNetworkChanged event, then we
  // want to register for change notification from the network state handler.
  // Otherwise, we want to unregister and not be listening to network changes.
  void StartOrStopListeningForNetworkChanges();

  Profile* profile_;
  bool listening_;

  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateEventRouterImpl);
};

NetworkingPrivateEventRouterImpl::NetworkingPrivateEventRouterImpl(
    Profile* profile)
    : profile_(profile), listening_(false) {
  // Register with the event router so we know when renderers are listening to
  // our events. We first check and see if there *is* an event router, because
  // some unit tests try to create all profile services, but don't initialize
  // the event router first.
  EventRouter* event_router = EventRouter::Get(profile_);
  if (!event_router)
    return;
  event_router->RegisterObserver(
      this, api::networking_private::OnNetworksChanged::kEventName);
  event_router->RegisterObserver(
      this, api::networking_private::OnNetworkListChanged::kEventName);
  StartOrStopListeningForNetworkChanges();
}

NetworkingPrivateEventRouterImpl::~NetworkingPrivateEventRouterImpl() {
  DCHECK(!listening_);
}

void NetworkingPrivateEventRouterImpl::Shutdown() {
  // Unregister with the event router. We first check and see if there *is* an
  // event router, because some unit tests try to shutdown all profile services,
  // but didn't initialize the event router first.
  EventRouter* event_router = EventRouter::Get(profile_);
  if (event_router)
    event_router->UnregisterObserver(this);

  if (!listening_)
    return;
  listening_ = false;
  NetworkingPrivateServiceClient* process_client =
      NetworkingPrivateServiceClientFactory::GetForBrowserContext(profile_);
  process_client->RemoveObserver(this);
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
  EventRouter* event_router = EventRouter::Get(profile_);
  if (!event_router)
    return;
  bool should_listen =
      event_router->HasEventListener(
          api::networking_private::OnNetworksChanged::kEventName) ||
      event_router->HasEventListener(
          api::networking_private::OnNetworkListChanged::kEventName);

  if (should_listen && !listening_) {
    NetworkingPrivateServiceClient* process_client =
        NetworkingPrivateServiceClientFactory::GetForBrowserContext(profile_);
    process_client->AddObserver(this);
  }

  if (!should_listen && listening_) {
    NetworkingPrivateServiceClient* process_client =
        NetworkingPrivateServiceClientFactory::GetForBrowserContext(profile_);
    process_client->RemoveObserver(this);
  }

  listening_ = should_listen;
}

void NetworkingPrivateEventRouterImpl::OnNetworksChangedEvent(
    const std::vector<std::string>& network_guids) {
  EventRouter* event_router = EventRouter::Get(profile_);
  if (!event_router)
    return;
  scoped_ptr<base::ListValue> args(
      api::networking_private::OnNetworksChanged::Create(network_guids));
  scoped_ptr<extensions::Event> netchanged_event(new extensions::Event(
      api::networking_private::OnNetworksChanged::kEventName, args.Pass()));
  event_router->BroadcastEvent(netchanged_event.Pass());
}

void NetworkingPrivateEventRouterImpl::OnNetworkListChangedEvent(
    const std::vector<std::string>& network_guids) {
  EventRouter* event_router = EventRouter::Get(profile_);
  if (!event_router)
    return;
  scoped_ptr<base::ListValue> args(
      api::networking_private::OnNetworkListChanged::Create(network_guids));
  scoped_ptr<extensions::Event> netlistchanged_event(new extensions::Event(
      api::networking_private::OnNetworkListChanged::kEventName,
      args.Pass()));
  event_router->BroadcastEvent(netlistchanged_event.Pass());
}

NetworkingPrivateEventRouter* NetworkingPrivateEventRouter::Create(
    Profile* profile) {
  return new NetworkingPrivateEventRouterImpl(profile);
}

}  // namespace extensions
