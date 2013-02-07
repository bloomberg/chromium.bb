// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/networking_private_event_router.h"

#include "base/json/json_writer.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/extensions/networking_private_api.h"
#include "chrome/browser/extensions/event_names.h"
#include "chrome/browser/extensions/event_router_forwarder.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/common/extensions/api/networking_private.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/onc/onc_constants.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_translator.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

using extensions::ExtensionSystem;
namespace api = extensions::api::networking_private;

namespace chromeos {

namespace {

// Translates the current connection state of the network into the ONC
// equivalent.
std::string GetConnectionState(const NetworkState* state) {
  if (state->IsConnectedState())
    return onc::connection_state::kConnected;
  else if (state->IsConnectingState())
    return onc::connection_state::kConnecting;
  else
    return onc::connection_state::kNotConnected;
}

// Translate from the Shill network type to the ONC network type.
std::string GetConnectionType(const std::string& shill_type) {
  base::DictionaryValue shill_type_dict;
  shill_type_dict.SetStringWithoutPathExpansion(flimflam::kTypeProperty,
                                                shill_type);
  scoped_ptr<base::DictionaryValue> onc_type_dict =
      onc::TranslateShillServiceToONCPart(
          shill_type_dict,
          &onc::kNetworkConfigurationSignature);
  std::string onc_type;
  if (onc_type_dict->GetString(onc::network_config::kType, &onc_type))
    return onc_type;
  return std::string();
}

}  // namespace

NetworkingPrivateEventRouter::NetworkingPrivateEventRouter(Profile* profile)
    : profile_(profile), listening_(false) {
  // Register with the event router so we know when renderers are listening to
  // our events. We first check and see if there *is* an event router, because
  // some unit tests try to create all profile services, but don't initialize
  // the event router first.
  extensions::EventRouter* event_router =
      ExtensionSystem::Get(profile_)->event_router();
  if (event_router) {
    event_router->RegisterObserver(
        this, extensions::event_names::kOnNetworkChanged);
    StartOrStopListeningForNetworkChanges();
  }
}

NetworkingPrivateEventRouter::~NetworkingPrivateEventRouter() {
}

void NetworkingPrivateEventRouter::Shutdown() {
  // Unregister with the event router. We first check and see if there *is* an
  // event router, because some unit tests try to shutdown all profile services,
  // but didn't initialize the event router first.
  extensions::EventRouter* event_router =
      ExtensionSystem::Get(profile_)->event_router();
  if (event_router)
    event_router->UnregisterObserver(this);

  if (listening_)
    NetworkStateHandler::Get()->RemoveObserver(this);
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
  bool should_listen = ExtensionSystem::Get(profile_)->event_router()->
      HasEventListener(extensions::event_names::kOnNetworkChanged);

  if (should_listen) {
    if (!listening_)
      NetworkStateHandler::Get()->AddObserver(this);
  } else {
    if (listening_)
      NetworkStateHandler::Get()->RemoveObserver(this);
  }
  listening_ = should_listen;
}

void NetworkingPrivateEventRouter::NetworkListChanged(
    const NetworkStateList& networks) {
  std::vector<linked_ptr<api::NetworkProperties> > changes;
  for (NetworkStateList::const_iterator iter = networks.begin();
      iter != networks.end(); ++iter) {
    api::NetworkProperties* network_properties = new api::NetworkProperties;
    network_properties->additional_properties.SetString(
        onc::network_config::kName, (*iter)->name());
    network_properties->additional_properties.SetString(
        onc::network_config::kGUID, (*iter)->path());
    network_properties->additional_properties.SetString(
        onc::network_config::kType,
        GetConnectionType((*iter)->type()));
    network_properties->additional_properties.SetString(
        onc::network_config::kConnectionState,
        GetConnectionState(*iter));
    changes.push_back(make_linked_ptr(network_properties));
  }

  scoped_ptr<base::ListValue> args(api::OnNetworkChanged::Create(changes));
  scoped_ptr<extensions::Event> extension_event(new extensions::Event(
      extensions::event_names::kOnNetworkChanged, args.Pass()));
  ExtensionSystem::Get(profile_)->event_router()->BroadcastEvent(
      extension_event.Pass());
}

}  // namespace chromeos
