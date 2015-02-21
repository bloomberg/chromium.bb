// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "extensions/browser/api/networking_config/networking_config_service.h"
#include "extensions/common/api/networking_config.h"

namespace extensions {

namespace {

bool IsValidNonEmptyHexString(const std::string& input) {
  size_t count = input.size();
  if (count == 0 || (count % 2) != 0)
    return false;
  for (const char& c : input)
    if (!IsHexDigit<char>(c))
      return false;
  return true;
}

}  // namespace

NetworkingConfigService::AuthenticationResult::AuthenticationResult()
    : authentication_state(NetworkingConfigService::NOTRY) {
}

NetworkingConfigService::AuthenticationResult::AuthenticationResult(
    ExtensionId extension_id,
    std::string guid,
    AuthenticationState authentication_state)
    : extension_id(extension_id),
      guid(guid),
      authentication_state(authentication_state) {
}

NetworkingConfigService::NetworkingConfigService(
    content::BrowserContext* browser_context,
    scoped_ptr<EventDelegate> event_delegate,
    ExtensionRegistry* extension_registry)
    : browser_context_(browser_context),
      registry_observer_(this),
      event_delegate_(event_delegate.Pass()) {
  registry_observer_.Add(extension_registry);
}

NetworkingConfigService::~NetworkingConfigService() {
}

void NetworkingConfigService::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  UnregisterExtension(extension->id());
}

std::string NetworkingConfigService::LookupExtensionIdForHexSsid(
    std::string hex_ssid) const {
  // Transform hex_ssid to uppercase.
  transform(hex_ssid.begin(), hex_ssid.end(), hex_ssid.begin(), toupper);

  const auto it = hex_ssid_to_extension_id_.find(hex_ssid);
  if (it == hex_ssid_to_extension_id_.end())
    return std::string();
  return it->second;
}

bool NetworkingConfigService::IsRegisteredForCaptivePortalEvent(
    std::string extension_id) const {
  return event_delegate_->HasExtensionRegisteredForEvent(extension_id);
}

bool NetworkingConfigService::RegisterHexSsid(std::string hex_ssid,
                                              const std::string& extension_id) {
  if (!IsValidNonEmptyHexString(hex_ssid)) {
    LOG(ERROR) << "\'" << hex_ssid << "\' is not a valid hex encoded string.";
    return false;
  }

  // Transform hex_ssid to uppercase.
  transform(hex_ssid.begin(), hex_ssid.end(), hex_ssid.begin(), toupper);

  return hex_ssid_to_extension_id_.insert(make_pair(hex_ssid, extension_id))
      .second;
}

void NetworkingConfigService::UnregisterExtension(
    const std::string& extension_id) {
  for (auto it = hex_ssid_to_extension_id_.begin();
       it != hex_ssid_to_extension_id_.end();) {
    if (it->second == extension_id)
      hex_ssid_to_extension_id_.erase(it++);
    else
      ++it;
  }
}

const NetworkingConfigService::AuthenticationResult&
NetworkingConfigService::GetAuthenticationResult() const {
  return authentication_result_;
}

void NetworkingConfigService::ResetAuthenticationResult() {
  authentication_result_ = AuthenticationResult();
}

void NetworkingConfigService::SetAuthenticationResult(
    const AuthenticationResult& authentication_result) {
  authentication_result_ = authentication_result;
}

bool NetworkingConfigService::DispatchPortalDetectedEvent(
    std::string extension_id,
    std::string guid) {
  EventRouter* eventRouter = EventRouter::Get(browser_context_);
  const chromeos::NetworkState* network = chromeos::NetworkHandler::Get()
                                              ->network_state_handler()
                                              ->GetNetworkStateFromGuid(guid);
  if (!network)
    return false;

  // Populate the NetworkInfo object.
  extensions::core_api::networking_config::NetworkInfo network_info;
  network_info.type =
      extensions::core_api::networking_config::NETWORK_TYPE_WIFI;
  const std::vector<uint8_t>& raw_ssid = network->raw_ssid();
  std::string hex_ssid =
      base::HexEncode(vector_as_array(&raw_ssid), raw_ssid.size());
  network_info.hex_ssid = make_scoped_ptr(new std::string(hex_ssid));
  network_info.ssid = make_scoped_ptr(new std::string(network->name()));
  network_info.guid = make_scoped_ptr(new std::string(network->guid()));
  scoped_ptr<base::ListValue> results =
      extensions::core_api::networking_config::OnCaptivePortalDetected::Create(
          network_info);
  scoped_ptr<Event> event(new Event(extensions::core_api::networking_config::
                                        OnCaptivePortalDetected::kEventName,
                                    results.Pass()));
  eventRouter->DispatchEventToExtension(extension_id, event.Pass());
  return true;
}

}  // namespace extensions
