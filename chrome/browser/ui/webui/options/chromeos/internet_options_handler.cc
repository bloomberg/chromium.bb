// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/internet_options_handler.h"

#include <ctype.h>

#include <map>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/enrollment_dialog_view.h"
#include "chrome/browser/chromeos/mobile_config.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/sim_dialog_delegate.h"
#include "chrome/browser/chromeos/ui/choose_mobile_network_dialog.h"
#include "chrome/browser/chromeos/ui/mobile_config_ui.h"
#include "chrome/browser/chromeos/ui_proxy_config_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/mobile_setup_dialog.h"
#include "chrome/browser/ui/webui/options/chromeos/internet_options_handler_strings.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "components/onc/onc_constants.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/api/vpn_provider/vpn_service.h"
#include "extensions/browser/api/vpn_provider/vpn_service_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/chromeos/network/network_connect.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"

namespace chromeos {
namespace options {

namespace {

// Keys for the initial "localized" dictionary values.
const char kLoggedInAsOwnerKey[] = "loggedInAsOwner";
const char kShowCarrierSelectKey[] = "showCarrierSelect";

// Functions we call in JavaScript.
const char kSetVPNProvidersFunction[] = "options.VPNProviders.setProviders";

// JS methods to show additional UI.
const char kShowMorePlanInfoMessage[] = "showMorePlanInfo";
const char kSimOperationMessage[] = "simOperation";

// TODO(stevenjb): Deprecate these and integrate with settings Web UI.
const char kAddVPNConnectionMessage[] = "addVPNConnection";
const char kAddNonVPNConnectionMessage[] = "addNonVPNConnection";
const char kConfigureNetworkMessage[] = "configureNetwork";

const char kLoadVPNProviders[] = "loadVPNProviders";

// These are strings used to communicate with JavaScript.
const char kTagSimOpChangePin[] = "changePin";
const char kTagSimOpConfigure[] = "configure";
const char kTagSimOpSetLocked[] = "setLocked";
const char kTagSimOpSetUnlocked[] = "setUnlocked";
const char kTagSimOpUnlock[] = "unlock";
const char kTagVPNProviderName[] = "name";
const char kTagVPNProviderExtensionID[] = "extensionID";

const NetworkState* GetNetworkState(const std::string& service_path) {
  return NetworkHandler::Get()->network_state_handler()->
      GetNetworkState(service_path);
}

std::string ServicePathFromGuid(const std::string& guid) {
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->GetNetworkStateFromGuid(
          guid);
  return network ? network->path() : "";
}

bool IsVPNProvider(const extensions::Extension* extension) {
  return extension->permissions_data()->HasAPIPermission(
      extensions::APIPermission::kVpnProvider);
}

Profile* GetProfileForPrimaryUser() {
  return chromeos::ProfileHelper::Get()->GetProfileByUser(
      user_manager::UserManager::Get()->GetPrimaryUser());
}

extensions::ExtensionRegistry* GetExtensionRegistryForPrimaryUser() {
  return extensions::ExtensionRegistry::Get(GetProfileForPrimaryUser());
}

scoped_ptr<base::DictionaryValue> BuildVPNProviderDictionary(
    const std::string& name,
    const std::string& third_party_provider_extension_id) {
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  dict->SetString(kTagVPNProviderName, name);
  if (!third_party_provider_extension_id.empty()) {
    dict->SetString(kTagVPNProviderExtensionID,
                    third_party_provider_extension_id);
  }
  return dict.Pass();
}

}  // namespace

InternetOptionsHandler::InternetOptionsHandler()
    : weak_factory_(this) {
  GetExtensionRegistryForPrimaryUser()->AddObserver(this);
}

InternetOptionsHandler::~InternetOptionsHandler() {
  GetExtensionRegistryForPrimaryUser()->RemoveObserver(this);
}

void InternetOptionsHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  internet_options_strings::RegisterLocalizedStrings(localized_strings);

  // TODO(stevenjb): Find a better way to populate initial data before
  // InitializePage() gets called.
  std::string owner;
  chromeos::CrosSettings::Get()->GetString(chromeos::kDeviceOwner, &owner);
  bool logged_in_as_owner = LoginState::Get()->GetLoggedInUserType() ==
                            LoginState::LOGGED_IN_USER_OWNER;
  localized_strings->SetBoolean(kLoggedInAsOwnerKey, logged_in_as_owner);
  // TODO(anujsharma): Remove kShowCarrierSelectKey, as it is not
  // required anymore.
  localized_strings->SetBoolean(kShowCarrierSelectKey, false);
}

void InternetOptionsHandler::InitializePage() {
  UpdateVPNProviders();
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
  web_ui()->RegisterMessageCallback(
      kLoadVPNProviders,
      base::Bind(&InternetOptionsHandler::LoadVPNProvidersCallback,
                 base::Unretained(this)));
}

void InternetOptionsHandler::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  if (IsVPNProvider(extension))
    UpdateVPNProviders();
}

void InternetOptionsHandler::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UnloadedExtensionInfo::Reason reason) {
  if (IsVPNProvider(extension))
    UpdateVPNProviders();
}

void InternetOptionsHandler::OnShutdown(
    extensions::ExtensionRegistry* registry) {
  registry->RemoveObserver(this);
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
  std::string service_path = ServicePathFromGuid(guid);
  if (!service_path.empty())
    ui::NetworkConnect::Get()->ShowMobileSetup(service_path);
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

void InternetOptionsHandler::UpdateVPNProviders() {
  extensions::ExtensionRegistry* const registry =
      GetExtensionRegistryForPrimaryUser();

  base::ListValue vpn_providers;
  const extensions::ExtensionSet& extensions = registry->enabled_extensions();
  for (const auto& extension : extensions) {
    if (IsVPNProvider(extension.get())) {
      vpn_providers.Append(BuildVPNProviderDictionary(
                               extension->name(), extension->id()).release());
    }
  }
  // Add the built-in OpenVPN/L2TP provider.
  vpn_providers.Append(
      BuildVPNProviderDictionary(
          l10n_util::GetStringUTF8(IDS_NETWORK_VPN_BUILT_IN_PROVIDER),
          std::string() /* third_party_provider_extension_id */).release());
  web_ui()->CallJavascriptFunction(kSetVPNProvidersFunction, vpn_providers);
}

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

  const std::string service_path = ServicePathFromGuid(guid);
  if (service_path.empty())
    return;

  const NetworkState* network = GetNetworkState(service_path);
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
      enrollment::CreateDialog(service_path, GetNativeWindow())) {
    return;
  }

  NetworkConfigView::Show(service_path, GetNativeWindow());
}

void InternetOptionsHandler::LoadVPNProvidersCallback(
    const base::ListValue* args) {
  UpdateVPNProviders();
}

}  // namespace options
}  // namespace chromeos
