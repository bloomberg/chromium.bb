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
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/mobile_config.h"
#include "chrome/browser/chromeos/net/onc_utils.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/chromeos/options/network_property_ui_data.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/sim_dialog_delegate.h"
#include "chrome/browser/chromeos/ui/choose_mobile_network_dialog.h"
#include "chrome/browser/chromeos/ui/mobile_config_ui.h"
#include "chrome/browser/chromeos/ui_proxy_config_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/mobile_setup_dialog.h"
#include "chrome/browser/ui/webui/options/chromeos/internet_options_handler_strings.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/login/login_state.h"
#include "chromeos/network/device_state.h"
#include "chromeos/network/managed_network_configuration_handler.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_device_handler.h"
#include "chromeos/network/network_event_log.h"
#include "chromeos/network/network_ip_config.h"
#include "chromeos/network/network_profile.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_util.h"
#include "chromeos/network/onc/onc_signature.h"
#include "chromeos/network/onc/onc_translation_tables.h"
#include "chromeos/network/onc/onc_translator.h"
#include "chromeos/network/onc/onc_utils.h"
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
const char kNetworkDataKey[] = "networkData";

// Keys for the network description dictionary passed to the web ui. Make sure
// to keep the strings in sync with what the JavaScript side uses.
const char kNetworkInfoKeyPolicyManaged[] = "policyManaged";

// Functions we call in JavaScript.
const char kSetVPNProvidersFunction[] = "options.VPNProviders.setProviders";
const char kRefreshNetworkDataFunction[] =
    "options.network.NetworkList.refreshNetworkData";
const char kUpdateConnectionDataFunction[] =
    "options.internet.DetailsInternetPage.updateConnectionData";
const char kUpdateCarrierFunction[] =
    "options.internet.DetailsInternetPage.updateCarrier";

// Setter methods called from JS that still need to be converted to match
// networkingPrivate methods.
const char kSetCarrierMessage[] = "setCarrier";
const char kShowMorePlanInfoMessage[] = "showMorePlanInfo";
const char kSimOperationMessage[] = "simOperation";

// TODO(stevenjb): Replace these with the matching networkingPrivate methods.
// crbug.com/279351.
const char kStartConnectMessage[] = "startConnect";

// TODO(stevenjb): Deprecate this once we handle events in the JS.
const char kSetNetworkGuidMessage[] = "setNetworkGuid";

// TODO(stevenjb): Add these to networkingPrivate.
const char kRemoveNetworkMessage[] = "removeNetwork";

// TODO(stevenjb): Deprecate these and integrate with settings Web UI.
const char kAddVPNConnectionMessage[] = "addVPNConnection";
const char kAddNonVPNConnectionMessage[] = "addNonVPNConnection";
const char kConfigureNetworkMessage[] = "configureNetwork";
const char kActivateNetworkMessage[] = "activateNetwork";

const char kLoadVPNProviders[] = "loadVPNProviders";

// These are strings used to communicate with JavaScript.
const char kTagCellularAvailable[] = "cellularAvailable";
const char kTagCellularEnabled[] = "cellularEnabled";
const char kTagCellularSimAbsent[] = "cellularSimAbsent";
const char kTagCellularSimLockType[] = "cellularSimLockType";
const char kTagCellularSupportsScan[] = "cellularSupportsScan";
const char kTagRememberedList[] = "rememberedList";
const char kTagSimOpChangePin[] = "changePin";
const char kTagSimOpConfigure[] = "configure";
const char kTagSimOpSetLocked[] = "setLocked";
const char kTagSimOpSetUnlocked[] = "setUnlocked";
const char kTagSimOpUnlock[] = "unlock";
const char kTagVPNProviderName[] = "name";
const char kTagVPNProviderExtensionID[] = "extensionID";
const char kTagVpnList[] = "vpnList";
const char kTagWifiAvailable[] = "wifiAvailable";
const char kTagWifiEnabled[] = "wifiEnabled";
const char kTagWimaxAvailable[] = "wimaxAvailable";
const char kTagWimaxEnabled[] = "wimaxEnabled";
const char kTagWiredList[] = "wiredList";
const char kTagWirelessList[] = "wirelessList";

// Pseudo-ONC chrome specific properties appended to the ONC dictionary.
const char kTagShowViewAccountButton[] = "showViewAccountButton";

void ShillError(const std::string& function,
                const std::string& error_name,
                scoped_ptr<base::DictionaryValue> error_data) {
  // UpdateConnectionData may send requests for stale services; ignore
  // these errors.
  if (function == "UpdateConnectionData" &&
      error_name == network_handler::kDBusFailedError)
    return;
  NET_LOG_ERROR("Shill Error from InternetOptionsHandler: " + error_name,
                function);
}

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

// Builds a dictionary with network information for the NetworkList on the
// settings page. Ownership of the returned pointer is transferred to the
// caller. TODO(stevenjb): Replace with calls to networkingPrivate.getNetworks.
base::DictionaryValue* BuildNetworkDictionary(
    const NetworkState* network,
    const PrefService* profile_prefs) {
  scoped_ptr<base::DictionaryValue> network_info =
      network_util::TranslateNetworkStateToONC(network);

  bool has_policy = onc::HasPolicyForNetwork(
      profile_prefs, g_browser_process->local_state(), *network);
  network_info->SetBoolean(kNetworkInfoKeyPolicyManaged, has_policy);

  return network_info.release();
}

bool ShowViewAccountButton(const NetworkState* cellular) {
  if (cellular->activation_state() != shill::kActivationStateActivating &&
      cellular->activation_state() != shill::kActivationStateActivated)
    return false;

  const DeviceState* device =
      NetworkHandler::Get()->network_state_handler()->GetDeviceState(
          cellular->device_path());

  // If no online payment URL was provided by shill, Check to see if the
  // MobileConfig carrier indicates that "View Account" should be shown.
  if (cellular->payment_url().empty()) {
    if (!device || !MobileConfig::GetInstance()->IsReady())
      return false;
    const MobileConfig::Carrier* carrier =
        MobileConfig::GetInstance()->GetCarrier(device->home_provider_id());
    if (!carrier || !carrier->show_portal_button())
      return false;
  }

  if (!cellular->IsConnectedState()) {
    // Disconnected LTE networks should show the button if we are online and
    // the device's MDN is set. This is to enable users to update their plan
    // if they are out of credits.
    if (!NetworkHandler::Get()->network_state_handler()->DefaultNetwork())
      return false;
    const std::string& technology = cellular->network_technology();
    if (technology != shill::kNetworkTechnologyLte &&
        technology != shill::kNetworkTechnologyLteAdvanced)
      return false;
    std::string mdn;
    if (device) {
      device->properties().GetStringWithoutPathExpansion(shill::kMdnProperty,
                                                         &mdn);
    }
    if (mdn.empty())
      return false;
  }

  return true;
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
  NetworkHandler::Get()->network_state_handler()->AddObserver(this, FROM_HERE);
}

InternetOptionsHandler::~InternetOptionsHandler() {
  if (NetworkHandler::IsInitialized()) {
    NetworkHandler::Get()->network_state_handler()->RemoveObserver(
        this, FROM_HERE);
  }
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
  localized_strings->SetBoolean(
      kShowCarrierSelectKey, base::CommandLine::ForCurrentProcess()->HasSwitch(
                                 chromeos::switches::kEnableCarrierSwitching));

  base::DictionaryValue* network_dictionary = new base::DictionaryValue;
  FillNetworkInfo(network_dictionary);
  localized_strings->Set(kNetworkDataKey, network_dictionary);
}

void InternetOptionsHandler::InitializePage() {
  UpdateVPNProviders();
  NetworkHandler::Get()->network_state_handler()->RequestScan();
  RefreshNetworkData();
}

void InternetOptionsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(kAddVPNConnectionMessage,
      base::Bind(&InternetOptionsHandler::AddVPNConnection,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kAddNonVPNConnectionMessage,
      base::Bind(&InternetOptionsHandler::AddNonVPNConnection,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kRemoveNetworkMessage,
      base::Bind(&InternetOptionsHandler::RemoveNetwork,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kConfigureNetworkMessage,
      base::Bind(&InternetOptionsHandler::ConfigureNetwork,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kActivateNetworkMessage,
      base::Bind(&InternetOptionsHandler::ActivateNetwork,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kShowMorePlanInfoMessage,
      base::Bind(&InternetOptionsHandler::ShowMorePlanInfoCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kSetCarrierMessage,
      base::Bind(&InternetOptionsHandler::SetCarrierCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kSimOperationMessage,
      base::Bind(&InternetOptionsHandler::SimOperationCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kLoadVPNProviders,
      base::Bind(&InternetOptionsHandler::LoadVPNProvidersCallback,
                 base::Unretained(this)));

  web_ui()->RegisterMessageCallback(kSetNetworkGuidMessage,
      base::Bind(&InternetOptionsHandler::SetNetworkGuidCallback,
                 base::Unretained(this)));

  // networkingPrivate methods
  web_ui()->RegisterMessageCallback(kStartConnectMessage,
      base::Bind(&InternetOptionsHandler::StartConnectCallback,
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

void InternetOptionsHandler::CarrierStatusCallback() {
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  const DeviceState* device =
      handler->GetDeviceStateByType(NetworkTypePattern::Cellular());
  if (device && (device->carrier() == shill::kCarrierSprint)) {
    const NetworkState* network =
        handler->FirstNetworkByType(NetworkTypePattern::Cellular());
    if (network && network->guid() == details_guid_) {
      ui::NetworkConnect::Get()->ActivateCellular(network->path());
      UpdateConnectionData(network->path());
    }
  }
  UpdateCarrier();
}

void InternetOptionsHandler::SetCarrierCallback(const base::ListValue* args) {
  std::string carrier;
  if (args->GetSize() != 1 || !args->GetString(1, &carrier)) {
    NOTREACHED();
    return;
  }
  const DeviceState* device = NetworkHandler::Get()->network_state_handler()->
      GetDeviceStateByType(NetworkTypePattern::Cellular());
  if (!device) {
    LOG(WARNING) << "SetCarrierCallback with no cellular device.";
    return;
  }
  NetworkHandler::Get()->network_device_handler()->SetCarrier(
      device->path(),
      carrier,
      base::Bind(&InternetOptionsHandler::CarrierStatusCallback,
                 weak_factory_.GetWeakPtr()),
      base::Bind(&ShillError, "SetCarrierCallback"));
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
// TODO(stevenjb): Deprecate this once events are handled in the JS.

void InternetOptionsHandler::SetNetworkGuidCallback(
    const base::ListValue* args) {
  std::string guid;
  if (args->GetSize() != 1 || !args->GetString(0, &guid)) {
    NOTREACHED();
    return;
  }
  details_guid_ = guid;
}


////////////////////////////////////////////////////////////////////////////////
// networkingPrivate implementation methods. TODO(stevenjb): Use the
// networkingPrivate API directly in the settings JS and deprecate these
// methods. crbug.com/279351.

void InternetOptionsHandler::StartConnectCallback(const base::ListValue* args) {
  std::string guid;
  if (!args->GetString(0, &guid)) {
    NOTREACHED();
    return;
  }
  std::string service_path = ServicePathFromGuid(guid);
  if (!service_path.empty())
    ui::NetworkConnect::Get()->ConnectToNetwork(service_path);
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

void InternetOptionsHandler::RefreshNetworkData() {
  base::DictionaryValue dictionary;
  FillNetworkInfo(&dictionary);
  web_ui()->CallJavascriptFunction(kRefreshNetworkDataFunction, dictionary);
}

void InternetOptionsHandler::UpdateConnectionData(
    const std::string& service_path) {
  NetworkHandler::Get()
      ->managed_network_configuration_handler()
      ->GetManagedProperties(
          LoginState::Get()->primary_user_hash(), service_path,
          base::Bind(&InternetOptionsHandler::GetManagedPropertiesResult,
                     weak_factory_.GetWeakPtr(), kUpdateConnectionDataFunction),
          base::Bind(&ShillError, "UpdateConnectionData"));
}

void InternetOptionsHandler::GetManagedPropertiesResult(
    const std::string& js_callback_function,
    const std::string& service_path,
    const base::DictionaryValue& onc_properties) {
  scoped_ptr<base::DictionaryValue> dictionary(onc_properties.DeepCopy());
  const NetworkState* network = GetNetworkState(service_path);
  if (network) {
    // Add additional non-ONC cellular properties to inform the UI.
    if (network->type() == shill::kTypeCellular) {
      dictionary->SetBoolean(kTagShowViewAccountButton,
                             ShowViewAccountButton(network));
    }
  }
  web_ui()->CallJavascriptFunction(js_callback_function, *dictionary);
}

void InternetOptionsHandler::UpdateCarrier() {
  web_ui()->CallJavascriptFunction(kUpdateCarrierFunction);
}

void InternetOptionsHandler::DeviceListChanged() {
  if (!web_ui())
    return;
  RefreshNetworkData();
}

void InternetOptionsHandler::NetworkListChanged() {
  if (!web_ui())
    return;
  RefreshNetworkData();
}

void InternetOptionsHandler::NetworkConnectionStateChanged(
    const NetworkState* network) {
  if (!web_ui())
    return;
  if (network->guid() == details_guid_)
    UpdateConnectionData(network->path());
}

void InternetOptionsHandler::NetworkPropertiesUpdated(
    const NetworkState* network) {
  if (!web_ui())
    return;
  RefreshNetworkData();
  if (network->guid() == details_guid_)
    UpdateConnectionData(network->path());
}

void InternetOptionsHandler::DevicePropertiesUpdated(
    const DeviceState* device) {
  if (!web_ui())
    return;
  if (device->type() != shill::kTypeCellular)
    return;
  const NetworkState* network =
      NetworkHandler::Get()->network_state_handler()->FirstNetworkByType(
          NetworkTypePattern::Cellular());
  if (network && network->path() == details_guid_)
    UpdateConnectionData(network->path());
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
  if (args->GetSize() != 1 || !args->GetString(0, &guid)) {
    NOTREACHED();
    return;
  }
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

  NetworkConfigView::Show(service_path, GetNativeWindow());
}

void InternetOptionsHandler::ActivateNetwork(const base::ListValue* args) {
  std::string guid;
  if (args->GetSize() != 1 || !args->GetString(0, &guid)) {
    NOTREACHED();
    return;
  }
  std::string service_path = ServicePathFromGuid(guid);
  if (!service_path.empty())
    ui::NetworkConnect::Get()->ActivateCellular(service_path);
}

void InternetOptionsHandler::RemoveNetwork(const base::ListValue* args) {
  std::string guid;
  if (args->GetSize() != 1 || !args->GetString(0, &guid)) {
    NOTREACHED();
    return;
  }
  std::string service_path = ServicePathFromGuid(guid);
  if (service_path.empty())
    return;
  NetworkHandler::Get()
      ->managed_network_configuration_handler()
      ->RemoveConfiguration(service_path, base::Bind(&base::DoNothing),
                            base::Bind(&ShillError, "RemoveNetwork"));
}

void InternetOptionsHandler::LoadVPNProvidersCallback(
    const base::ListValue* args) {
  UpdateVPNProviders();
}

base::ListValue* InternetOptionsHandler::GetWiredList() {
  base::ListValue* list = new base::ListValue();
  const NetworkState* network = NetworkHandler::Get()->network_state_handler()->
      FirstNetworkByType(NetworkTypePattern::Ethernet());
  if (!network)
    return list;
  list->Append(BuildNetworkDictionary(network, GetPrefs()));
  return list;
}

base::ListValue* InternetOptionsHandler::GetWirelessList() {
  base::ListValue* list = new base::ListValue();

  NetworkStateHandler::NetworkStateList networks;
  NetworkHandler::Get()->network_state_handler()->GetVisibleNetworkListByType(
      NetworkTypePattern::Wireless(), &networks);
  for (NetworkStateHandler::NetworkStateList::const_iterator iter =
           networks.begin(); iter != networks.end(); ++iter) {
    list->Append(BuildNetworkDictionary(*iter, GetPrefs()));
  }

  return list;
}

base::ListValue* InternetOptionsHandler::GetVPNList() {
  base::ListValue* list = new base::ListValue();

  NetworkStateHandler::NetworkStateList networks;
  NetworkHandler::Get()->network_state_handler()->GetVisibleNetworkListByType(
      NetworkTypePattern::VPN(), &networks);
  for (NetworkStateHandler::NetworkStateList::const_iterator iter =
           networks.begin(); iter != networks.end(); ++iter) {
    list->Append(BuildNetworkDictionary(*iter, GetPrefs()));
  }

  return list;
}

base::ListValue* InternetOptionsHandler::GetRememberedList() {
  base::ListValue* list = new base::ListValue();

  NetworkStateHandler::NetworkStateList networks;
  NetworkHandler::Get()->network_state_handler()->GetNetworkListByType(
      NetworkTypePattern::Default(),
      true /* configured_only */,
      false /* visible_only */,
      0 /* no limit */,
      &networks);
  for (NetworkStateHandler::NetworkStateList::const_iterator iter =
           networks.begin(); iter != networks.end(); ++iter) {
    const NetworkState* network = *iter;
    if (network->type() != shill::kTypeWifi &&
        network->type() != shill::kTypeVPN) {
      continue;
    }
    list->Append(BuildNetworkDictionary(network, GetPrefs()));
  }

  return list;
}

void InternetOptionsHandler::FillNetworkInfo(
    base::DictionaryValue* dictionary) {
  NetworkStateHandler* handler = NetworkHandler::Get()->network_state_handler();
  dictionary->Set(kTagWiredList, GetWiredList());
  dictionary->Set(kTagWirelessList, GetWirelessList());
  dictionary->Set(kTagVpnList, GetVPNList());
  dictionary->Set(kTagRememberedList, GetRememberedList());

  dictionary->SetBoolean(
      kTagWifiAvailable,
      handler->IsTechnologyAvailable(NetworkTypePattern::WiFi()));
  dictionary->SetBoolean(
      kTagWifiEnabled,
      handler->IsTechnologyEnabled(NetworkTypePattern::WiFi()));

  const DeviceState* cellular =
      handler->GetDeviceStateByType(NetworkTypePattern::Mobile());
  dictionary->SetBoolean(
      kTagCellularAvailable,
      handler->IsTechnologyAvailable(NetworkTypePattern::Mobile()));
  dictionary->SetBoolean(
      kTagCellularEnabled,
      handler->IsTechnologyEnabled(NetworkTypePattern::Mobile()));
  dictionary->SetBoolean(kTagCellularSupportsScan,
                         cellular && cellular->support_network_scan());
  dictionary->SetBoolean(kTagCellularSimAbsent,
                         cellular && cellular->IsSimAbsent());
  dictionary->SetString(kTagCellularSimLockType,
                        cellular ? cellular->sim_lock_type() : "");

  dictionary->SetBoolean(
      kTagWimaxAvailable,
      handler->IsTechnologyAvailable(NetworkTypePattern::Wimax()));
  dictionary->SetBoolean(
      kTagWimaxEnabled,
      handler->IsTechnologyEnabled(NetworkTypePattern::Wimax()));
}

}  // namespace options
}  // namespace chromeos
