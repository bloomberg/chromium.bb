// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/internet_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/ui/choose_mobile_network_dialog.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/onc/onc_constants.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/api/vpn_provider/vpn_service.h"
#include "extensions/browser/api/vpn_provider/vpn_service_factory.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

namespace {

const char kAddNetworkMessage[] = "addNetwork";
const char kConfigureNetworkMessage[] = "configureNetwork";

std::string ServicePathFromGuid(const std::string& guid) {
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
          guid);
  return network ? network->path() : "";
}

Profile* GetProfileForPrimaryUser() {
  return ProfileHelper::Get()->GetProfileByUser(
      user_manager::UserManager::Get()->GetPrimaryUser());
}

}  // namespace

namespace settings {

InternetHandler::InternetHandler() {}

InternetHandler::~InternetHandler() {}

void InternetHandler::RegisterMessages() {
  // TODO(stevenjb): Eliminate once network configuration UI is integrated
  // into settings.
  web_ui()->RegisterMessageCallback(
      kAddNetworkMessage,
      base::Bind(&InternetHandler::AddNetwork, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kConfigureNetworkMessage,
      base::Bind(&InternetHandler::ConfigureNetwork, base::Unretained(this)));
}

void InternetHandler::OnJavascriptAllowed() {}

void InternetHandler::OnJavascriptDisallowed() {}

void InternetHandler::AddNetwork(const base::ListValue* args) {
  std::string onc_type;
  if (args->GetSize() < 1 || !args->GetString(0, &onc_type)) {
    NOTREACHED() << "Invalid args for: " << kAddNetworkMessage;
    return;
  }

  if (onc_type == ::onc::network_type::kVPN) {
    std::string extension_id;
    if (args->GetSize() >= 2)
      args->GetString(1, &extension_id);
    if (extension_id.empty()) {
      // Show the "add network" dialog for the built-in OpenVPN/L2TP provider.
      NetworkConfigView::ShowForType(shill::kTypeVPN, GetNativeWindow());
      return;
    }
    // Request that the third-party VPN provider identified by |provider_id|
    // show its "add network" dialog.
    VpnServiceFactory::GetForBrowserContext(GetProfileForPrimaryUser())
        ->SendShowAddDialogToExtension(extension_id);
  } else if (onc_type == ::onc::network_type::kWiFi) {
    NetworkConfigView::ShowForType(shill::kTypeWifi, GetNativeWindow());
  } else if (onc_type == ::onc::network_type::kCellular) {
    ChooseMobileNetworkDialog::ShowDialog(GetNativeWindow());
  } else {
    LOG(ERROR) << "Unsupported type for: " << kAddNetworkMessage;
  }
}

void InternetHandler::ConfigureNetwork(const base::ListValue* args) {
  std::string guid;
  if (args->GetSize() < 1 || !args->GetString(0, &guid)) {
    NOTREACHED() << "Invalid args for: " << kConfigureNetworkMessage;
    return;
  }

  const std::string service_path = ServicePathFromGuid(guid);
  if (service_path.empty()) {
    LOG(ERROR) << "Network not found: " << guid;
    return;
  }

  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkState(
          service_path);
  if (!network) {
    LOG(ERROR) << "Network not found with service_path: " << service_path;
    return;
  }

  if (network->type() == shill::kTypeVPN &&
      network->vpn_provider_type() == shill::kProviderThirdPartyVpn) {
    // Request that the third-party VPN provider used by the |network| show a
    // configuration dialog for it.
    VpnServiceFactory::GetForBrowserContext(GetProfileForPrimaryUser())
        ->SendShowConfigureDialogToExtension(
            network->third_party_vpn_provider_extension_id(), network->name());
    return;
  }

  NetworkConfigView::ShowForNetworkId(network->guid(), GetNativeWindow());
}

gfx::NativeWindow InternetHandler::GetNativeWindow() const {
  return web_ui()->GetWebContents()->GetTopLevelNativeWindow();
}

}  // namespace settings
}  // namespace chromeos
