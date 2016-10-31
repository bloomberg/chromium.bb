// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/internet_options_handler.h"

#include <ctype.h>

#include <map>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/values.h"
#include "chrome/browser/chromeos/enrollment_dialog_view.h"
#include "chrome/browser/chromeos/mobile_config.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/sim_dialog_delegate.h"
#include "chrome/browser/chromeos/ui/choose_mobile_network_dialog.h"
#include "chrome/browser/chromeos/ui/mobile_config_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/mobile_setup_dialog.h"
#include "chrome/browser/ui/webui/options/chromeos/internet_options_handler_strings.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_connect.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/onc/onc_constants.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/api/vpn_provider/vpn_service.h"
#include "extensions/browser/api/vpn_provider/vpn_service_factory.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {
namespace options {

namespace {

const char kAllowOnlyPolicyNetworksToConnect[] =
    "allowOnlyPolicyNetworksToConnect";

// Keys for the initial "localized" dictionary values.
const char kLoggedInAsOwnerKey[] = "loggedInAsOwner";

// JS methods to show additional UI.
const char kShowMorePlanInfoMessage[] = "showMorePlanInfo";
const char kSimOperationMessage[] = "simOperation";

// TODO(stevenjb): Deprecate these and integrate with settings Web UI.
const char kAddVPNConnectionMessage[] = "addVPNConnection";
const char kAddNonVPNConnectionMessage[] = "addNonVPNConnection";
const char kConfigureNetworkMessage[] = "configureNetwork";

// These are strings used to communicate with JavaScript.
const char kTagSimOpChangePin[] = "changePin";
const char kTagSimOpConfigure[] = "configure";
const char kTagSimOpSetLocked[] = "setLocked";
const char kTagSimOpSetUnlocked[] = "setUnlocked";
const char kTagSimOpUnlock[] = "unlock";

Profile* GetProfileForPrimaryUser() {
  return chromeos::ProfileHelper::Get()->GetProfileByUser(
      user_manager::UserManager::Get()->GetPrimaryUser());
}

}  // namespace

InternetOptionsHandler::InternetOptionsHandler()
    : weak_factory_(this) {
}

InternetOptionsHandler::~InternetOptionsHandler() {
}

void InternetOptionsHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  internet_options_strings::RegisterLocalizedStrings(localized_strings);

  // TODO(stevenjb): Find a better way to populate initial data.
  std::string owner;
  chromeos::CrosSettings::Get()->GetString(chromeos::kDeviceOwner, &owner);
  bool logged_in_as_owner = LoginState::Get()->GetLoggedInUserType() ==
                            LoginState::LOGGED_IN_USER_OWNER;
  localized_strings->SetBoolean(kLoggedInAsOwnerKey, logged_in_as_owner);

  // TODO(fqj/stevenjb): Make this a property of networkingPrivate
  const base::DictionaryValue* global_network_config =
      NetworkHandler::Get()
          ->managed_network_configuration_handler()
          ->GetGlobalConfigFromPolicy(
              std::string() /* no user hash, device policy*/);
  if (global_network_config) {
    bool only_policy_connect = false;
    global_network_config->GetBooleanWithoutPathExpansion(
        ::onc::global_network_config::kAllowOnlyPolicyNetworksToConnect,
        &only_policy_connect);
    localized_strings->SetBoolean(kAllowOnlyPolicyNetworksToConnect,
                                  only_policy_connect);
  }
}

void InternetOptionsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(kAddVPNConnectionMessage,
      base::Bind(&InternetOptionsHandler::AddVPNConnection,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kAddNonVPNConnectionMessage,
      base::Bind(&InternetOptionsHandler::AddNonVPNConnection,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kConfigureNetworkMessage,
      base::Bind(&InternetOptionsHandler::ConfigureNetwork,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kShowMorePlanInfoMessage,
      base::Bind(&InternetOptionsHandler::ShowMorePlanInfoCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kSimOperationMessage,
      base::Bind(&InternetOptionsHandler::SimOperationCallback,
                 base::Unretained(this)));
}

void InternetOptionsHandler::ShowMorePlanInfoCallback(
    const base::ListValue* args) {
  if (!web_ui())
    return;
  std::string guid;
  if (args->GetSize() != 1 || !args->GetString(0, &guid)) {
    NOTREACHED();
    return;
  }
  NetworkConnect::Get()->ShowMobileSetup(guid);
}

void InternetOptionsHandler::SimOperationCallback(const base::ListValue* args) {
  std::string operation;
  if (args->GetSize() != 1 || !args->GetString(0, &operation)) {
    NOTREACHED();
    return;
  }
  if (operation == kTagSimOpConfigure) {
    mobile_config_ui::DisplayConfigDialog();
    return;
  }
  // 1. Bring up SIM unlock dialog, pass new RequirePin setting in URL.
  // 2. Dialog will ask for current PIN in any case.
  // 3. If card is locked it will first call PIN unlock operation
  // 4. Then it will call Set RequirePin, passing the same PIN.
  // 5. The dialog may change device properties, in which case
  //    DevicePropertiesUpdated() will get called which will update the UI.
  SimDialogDelegate::SimDialogMode mode;
  if (operation == kTagSimOpSetLocked) {
    mode = SimDialogDelegate::SIM_DIALOG_SET_LOCK_ON;
  } else if (operation == kTagSimOpSetUnlocked) {
    mode = SimDialogDelegate::SIM_DIALOG_SET_LOCK_OFF;
  } else if (operation == kTagSimOpUnlock) {
    mode = SimDialogDelegate::SIM_DIALOG_UNLOCK;
  } else if (operation == kTagSimOpChangePin) {
    mode = SimDialogDelegate::SIM_DIALOG_CHANGE_PIN;
  } else {
    NOTREACHED();
    return;
  }
  SimDialogDelegate::ShowDialog(GetNativeWindow(), mode);
}

////////////////////////////////////////////////////////////////////////////////

gfx::NativeWindow InternetOptionsHandler::GetNativeWindow() const {
  return web_ui()->GetWebContents()->GetTopLevelNativeWindow();
}

const PrefService* InternetOptionsHandler::GetPrefs() const {
  return Profile::FromWebUI(web_ui())->GetPrefs();
}


void InternetOptionsHandler::AddVPNConnection(const base::ListValue* args) {
  if (args->empty()) {
    // Show the "add network" dialog for the built-in OpenVPN/L2TP provider.
    NetworkConfigView::ShowForType(shill::kTypeVPN, GetNativeWindow());
    return;
  }

  std::string extension_id;
  if (args->GetSize() != 1 || !args->GetString(0, &extension_id)) {
    NOTREACHED();
    return;
  }

  // Request that the third-party VPN provider identified by |provider_id|
  // show its "add network" dialog.
  chromeos::VpnServiceFactory::GetForBrowserContext(
      GetProfileForPrimaryUser())->SendShowAddDialogToExtension(extension_id);
}

void InternetOptionsHandler::AddNonVPNConnection(const base::ListValue* args) {
  std::string onc_type;
  if (args->GetSize() != 1 || !args->GetString(0, &onc_type)) {
    NOTREACHED();
    return;
  }
  if (onc_type == ::onc::network_type::kWiFi) {
    NetworkConfigView::ShowForType(shill::kTypeWifi, GetNativeWindow());
  } else if (onc_type == ::onc::network_type::kCellular) {
    ChooseMobileNetworkDialog::ShowDialog(GetNativeWindow());
  } else {
    LOG(ERROR) << "Unsupported type for AddConnection";
  }
}

void InternetOptionsHandler::ConfigureNetwork(const base::ListValue* args) {
  std::string guid;
  if (args->GetSize() < 1 || !args->GetString(0, &guid)) {
    NOTREACHED();
    return;
  }
  bool force_show = false;
  if (args->GetSize() >= 2)
    args->GetBoolean(1, &force_show);

  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
          guid);
  if (!network)
    return;

  if (network->type() == shill::kTypeVPN &&
      network->vpn_provider_type() == shill::kProviderThirdPartyVpn) {
    // Request that the third-party VPN provider used by the |network| show a
    // configuration dialog for it.
    VpnServiceFactory::GetForBrowserContext(GetProfileForPrimaryUser())
        ->SendShowConfigureDialogToExtension(
            network->third_party_vpn_provider_extension_id(), network->name());
    return;
  }

  // If a network is not connectable, show the enrollment dialog if available.
  if (!force_show && !network->connectable() &&
      enrollment::CreateEnrollmentDialog(guid, GetNativeWindow())) {
    return;
  }

  NetworkConfigView::ShowForNetworkId(guid, GetNativeWindow());
}

}  // namespace options
}  // namespace chromeos
