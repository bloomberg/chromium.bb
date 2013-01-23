// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/internet_options_handler.h"

#include <ctype.h>

#include <map>
#include <string>
#include <vector>

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "base/base64.h"
#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/choose_mobile_network_dialog.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/enrollment_dialog_view.h"
#include "chrome/browser/chromeos/mobile_config.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/chromeos/proxy_config_service_impl.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/sim_dialog_delegate.h"
#include "chrome/browser/chromeos/status/network_menu_icon.h"
#include "chrome/browser/net/pref_proxy_config_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/webui/web_ui_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/time_format.h"
#include "chromeos/network/network_ip_config.h"
#include "chromeos/network/network_util.h"
#include "chromeos/network/onc/onc_constants.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "grit/ash_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/display.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/screen.h"
#include "ui/views/widget/widget.h"

namespace {

// Keys for the network description dictionary passed to the web ui. Make sure
// to keep the strings in sync with what the JavaScript side uses.
const char kNetworkInfoKeyActivationState[] = "activation_state";
const char kNetworkInfoKeyConnectable[] = "connectable";
const char kNetworkInfoKeyConnected[] = "connected";
const char kNetworkInfoKeyConnecting[] = "connecting";
const char kNetworkInfoKeyIconURL[] = "iconURL";
const char kNetworkInfoKeyNetworkName[] = "networkName";
const char kNetworkInfoKeyNetworkStatus[] = "networkStatus";
const char kNetworkInfoKeyNetworkType[] = "networkType";
const char kNetworkInfoKeyRemembered[] = "remembered";
const char kNetworkInfoKeyServicePath[] = "servicePath";
const char kNetworkInfoKeyPolicyManaged[] = "policyManaged";

// These are keys for getting IP information from the web ui.
const char kIpConfigAddress[] = "address";
const char kIpConfigPrefixLength[] = "prefixLength";
const char kIpConfigNetmask[] = "netmask";
const char kIpConfigGateway[] = "gateway";
const char kIpConfigNameServers[] = "nameServers";
const char kIpConfigAutoConfig[] = "ipAutoConfig";

// These are types of name server selections from the web ui.
const char kNameServerTypeAutomatic[] = "automatic";
const char kNameServerTypeGoogle[] = "google";
const char kNameServerTypeUser[] = "user";

// These are dictionary names used to send data to the web ui.
const char kDictionaryIpConfig[] = "ipconfig";
const char kDictionaryStaticIp[] = "staticIP";
const char kDictionarySavedIp[] = "savedIP";

// Google public name servers (DNS).
const char kGoogleNameServers[] = "8.8.4.4,8.8.8.8";

// Functions we call in JavaScript.
const char kRefreshNetworkDataFunction[] =
    "options.network.NetworkList.refreshNetworkData";
const char kSetDefaultNetworkIconsFunction[] =
    "options.network.NetworkList.setDefaultNetworkIcons";
const char kShowDetailedInfoFunction[] =
    "options.internet.DetailsInternetPage.showDetailedInfo";
const char kUpdateConnectionDataFunction[] =
    "options.internet.DetailsInternetPage.updateConnectionData";
const char kUpdateCarrierFunction[] =
    "options.internet.DetailsInternetPage.updateCarrier";
const char kUpdateSecurityTabFunction[] =
    "options.internet.DetailsInternetPage.updateSecurityTab";

// These are used to register message handlers with JavaScript.
const char kBuyDataPlanMessage[] = "buyDataPlan";
const char kChangePinMessage[] = "changePin";
const char kDisableCellularMessage[] = "disableCellular";
const char kDisableWifiMessage[] = "disableWifi";
const char kDisableWimaxMessage[] = "disableWimax";
const char kEnableCellularMessage[] = "enableCellular";
const char kEnableWifiMessage[] = "enableWifi";
const char kEnableWimaxMessage[] = "enableWimax";
const char kNetworkCommandMessage[] = "networkCommand";
const char kRefreshNetworksMessage[] = "refreshNetworks";
const char kSetApnMessage[] = "setApn";
const char kSetAutoConnectMessage[] = "setAutoConnect";
const char kSetCarrierMessage[] = "setCarrier";
const char kSetIPConfigMessage[] = "setIPConfig";
const char kSetPreferNetworkMessage[] = "setPreferNetwork";
const char kSetServerHostname[] = "setServerHostname";
const char kSetSimCardLockMessage[] = "setSimCardLock";
const char kShowMorePlanInfoMessage[] = "showMorePlanInfo";

// These are strings used to communicate with JavaScript.
const char kTagAccessLocked[] = "accessLocked";
const char kTagActivate[] = "activate";
const char kTagActivated[] = "activated";
const char kTagActivationState[] = "activationState";
const char kTagAddConnection[] = "add";
const char kTagAirplaneMode[] = "airplaneMode";
const char kTagApn[] = "apn";
const char kTagAutoConnect[] = "autoConnect";
const char kTagBssid[] = "bssid";
const char kTagCarrierSelectFlag[] = "showCarrierSelect";
const char kTagCarrierUrl[] = "carrierUrl";
const char kTagCellular[] = "cellular";
const char kTagCellularAvailable[] = "cellularAvailable";
const char kTagCellularBusy[] = "cellularBusy";
const char kTagCellularEnabled[] = "cellularEnabled";
const char kTagCellularSupportsScan[] = "cellularSupportsScan";
const char kTagConnect[] = "connect";
const char kTagConnected[] = "connected";
const char kTagConnecting[] = "connecting";
const char kTagConnectionState[] = "connectionState";
const char kTagControlledBy[] = "controlledBy";
const char kTagDataRemaining[] = "dataRemaining";
const char kTagDeviceConnected[] = "deviceConnected";
const char kTagDisableConnectButton[] = "disableConnectButton";
const char kTagDisconnect[] = "disconnect";
const char kTagEncryption[] = "encryption";
const char kTagErrorState[] = "errorState";
const char kTagEsn[] = "esn";
const char kTagFirmwareRevision[] = "firmwareRevision";
const char kTagForget[] = "forget";
const char kTagFrequency[] = "frequency";
const char kTagGsm[] = "gsm";
const char kTagHardwareAddress[] = "hardwareAddress";
const char kTagHardwareRevision[] = "hardwareRevision";
const char kTagIdentity[] = "identity";
const char kTagImei[] = "imei";
const char kTagImsi[] = "imsi";
const char kTagLanguage[] = "language";
const char kTagLastGoodApn[] = "lastGoodApn";
const char kTagLocalizedName[] = "localizedName";
const char kTagManufacturer[] = "manufacturer";
const char kTagMdn[] = "mdn";
const char kTagMeid[] = "meid";
const char kTagMin[] = "min";
const char kTagModelId[] = "modelId";
const char kTagName[] = "name";
const char kTagNameServersGoogle[] = "nameServersGoogle";
const char kTagNameServerType[] = "nameServerType";
const char kTagNetworkId[] = "networkId";
const char kTagNetworkName[] = "networkName";
const char kTagNetworkTechnology[] = "networkTechnology";
const char kTagOperatorCode[] = "operatorCode";
const char kTagOperatorName[] = "operatorName";
const char kTagOptions[] = "options";
const char kTagPassword[] = "password";
const char kTagPolicy[] = "policy";
const char kTagPreferred[] = "preferred";
const char kTagPrlVersion[] = "prlVersion";
const char kTagProvider_type[] = "provider_type";
const char kTagProviderApnList[] = "providerApnList";
const char kTagRecommended[] = "recommended";
const char kTagRecommendedValue[] = "recommendedValue";
const char kTagRemembered[] = "remembered";
const char kTagRememberedList[] = "rememberedList";
const char kTagRestrictedPool[] = "restrictedPool";
const char kTagRoamingState[] = "roamingState";
const char kTagServerHostname[] = "serverHostname";
const char kTagService_name[] = "service_name";
const char kTagCarriers[] = "carriers";
const char kTagCurrentCarrierIndex[] = "currentCarrierIndex";
const char kTagServiceName[] = "serviceName";
const char kTagServicePath[] = "servicePath";
const char kTagShared[] = "shared";
const char kTagShowActivateButton[] = "showActivateButton";
const char kTagShowBuyButton[] = "showBuyButton";
const char kTagShowPreferred[] = "showPreferred";
const char kTagShowProxy[] = "showProxy";
const char kTagShowStaticIPConfig[] = "showStaticIPConfig";
const char kTagShowViewAccountButton[] = "showViewAccountButton";
const char kTagSimCardLockEnabled[] = "simCardLockEnabled";
const char kTagSsid[] = "ssid";
const char kTagStrength[] = "strength";
const char kTagSupportUrl[] = "supportUrl";
const char kTagTrue[] = "true";
const char kTagType[] = "type";
const char kTagUsername[] = "username";
const char kTagValue[] = "value";
const char kTagVpn[] = "vpn";
const char kTagVpnList[] = "vpnList";
const char kTagWarning[] = "warning";
const char kTagWifi[] = "wifi";
const char kTagWifiAvailable[] = "wifiAvailable";
const char kTagWifiBusy[] = "wifiBusy";
const char kTagWifiEnabled[] = "wifiEnabled";
const char kTagWimaxAvailable[] = "wimaxAvailable";
const char kTagWimaxBusy[] = "wimaxBusy";
const char kTagWimaxEnabled[] = "wimaxEnabled";
const char kTagWiredList[] = "wiredList";
const char kTagWirelessList[] = "wirelessList";
const char kToggleAirplaneModeMessage[] = "toggleAirplaneMode";

// A helper class for building network information dictionaries to be sent to
// the webui code.
class NetworkInfoDictionary {
 public:
  // Initializes the dictionary with default values.
  explicit NetworkInfoDictionary(ui::ScaleFactor icon_scale_factor);

  // Copies in service path, connect{ing|ed|able} flags and connection type from
  // the provided network object. Also chooses an appropriate icon based on the
  // network type.
  NetworkInfoDictionary(const chromeos::Network* network,
                        ui::ScaleFactor icon_scale_factor);

  // Initializes a remembered network entry, pulling information from the passed
  // network object and the corresponding remembered network object. |network|
  // may be NULL.
  NetworkInfoDictionary(const chromeos::Network* network,
                        const chromeos::Network* remembered,
                        ui::ScaleFactor icon_scale_factor);

  // Setters for filling in information.
  void set_service_path(const std::string& service_path) {
    service_path_ = service_path;
  }
  void set_icon(const gfx::ImageSkia& icon) {
    gfx::ImageSkiaRep image_rep = icon.GetRepresentation(icon_scale_factor_);
    icon_url_ = icon.isNull() ? "" : web_ui_util::GetBitmapDataUrl(
        image_rep.sk_bitmap());
  }
  void set_name(const std::string& name) {
    name_ = name;
  }
  void set_connecting(bool connecting) {
    connecting_ = connecting;
  }
  void set_connected(bool connected) {
    connected_ = connected;
  }
  void set_connectable(bool connectable) {
    connectable_ = connectable;
  }
  void set_connection_type(chromeos::ConnectionType connection_type) {
    connection_type_ = connection_type;
  }
  void set_remembered(bool remembered) {
    remembered_ = remembered;
  }
  void set_shared(bool shared) {
    shared_ = shared;
  }
  void set_activation_state(chromeos::ActivationState activation_state) {
    activation_state_ = activation_state;
  }
  void set_policy_managed(bool policy_managed) {
    policy_managed_ = policy_managed;
  }

  // Builds the DictionaryValue representation from the previously set
  // parameters. Ownership of the returned pointer is transferred to the caller.
  DictionaryValue* BuildDictionary();

 private:
  // Values to be filled into the dictionary.
  std::string service_path_;
  std::string icon_url_;
  std::string name_;
  bool connecting_;
  bool connected_;
  bool connectable_;
  chromeos::ConnectionType connection_type_;
  bool remembered_;
  bool shared_;
  chromeos::ActivationState activation_state_;
  bool policy_managed_;
  ui::ScaleFactor icon_scale_factor_;

  DISALLOW_COPY_AND_ASSIGN(NetworkInfoDictionary);
};

NetworkInfoDictionary::NetworkInfoDictionary(
    ui::ScaleFactor icon_scale_factor)
    : icon_scale_factor_(icon_scale_factor) {
  set_connecting(false);
  set_connected(false);
  set_connectable(false);
  set_remembered(false);
  set_shared(false);
  set_activation_state(chromeos::ACTIVATION_STATE_UNKNOWN);
  set_policy_managed(false);
}

NetworkInfoDictionary::NetworkInfoDictionary(const chromeos::Network* network,
                                             ui::ScaleFactor icon_scale_factor)
    : icon_scale_factor_(icon_scale_factor) {
  set_service_path(network->service_path());
  set_icon(chromeos::NetworkMenuIcon::GetImage(network,
      chromeos::NetworkMenuIcon::COLOR_DARK));
  set_name(network->name());
  set_connecting(network->connecting());
  set_connected(network->connected());
  set_connectable(network->connectable());
  set_connection_type(network->type());
  set_remembered(false);
  set_shared(false);
  set_policy_managed(network->ui_data().is_managed());
}

NetworkInfoDictionary::NetworkInfoDictionary(
    const chromeos::Network* network,
    const chromeos::Network* remembered,
    ui::ScaleFactor icon_scale_factor)
    : icon_scale_factor_(icon_scale_factor) {
  set_service_path(remembered->service_path());
  set_icon(chromeos::NetworkMenuIcon::GetImage(
      network ? network : remembered, chromeos::NetworkMenuIcon::COLOR_DARK));
  set_name(remembered->name());
  set_connecting(network ? network->connecting() : false);
  set_connected(network ? network->connected() : false);
  set_connectable(true);
  set_connection_type(remembered->type());
  set_remembered(true);
  set_shared(remembered->profile_type() == chromeos::PROFILE_SHARED);
  set_policy_managed(remembered->ui_data().is_managed());
}

DictionaryValue* NetworkInfoDictionary::BuildDictionary() {
  std::string status;

  if (remembered_) {
    if (shared_)
      status = l10n_util::GetStringUTF8(IDS_OPTIONS_SETTINGS_SHARED_NETWORK);
  } else {
    // 802.1X networks can be connected but not have saved credentials, and
    // hence be "not configured".  Give preference to the "connected" and
    // "connecting" states.  http://crosbug.com/14459
    int connection_state = IDS_STATUSBAR_NETWORK_DEVICE_DISCONNECTED;
    if (connected_)
      connection_state = IDS_STATUSBAR_NETWORK_DEVICE_CONNECTED;
    else if (connecting_)
      connection_state = IDS_STATUSBAR_NETWORK_DEVICE_CONNECTING;
    else if (!connectable_)
      connection_state = IDS_STATUSBAR_NETWORK_DEVICE_NOT_CONFIGURED;
    status = l10n_util::GetStringUTF8(connection_state);
    if (connection_type_ == chromeos::TYPE_CELLULAR) {
      if (activation_state_ != chromeos::ACTIVATION_STATE_ACTIVATED) {
        status.append(" / ");
        status.append(chromeos::CellularNetwork::ActivationStateToString(
            activation_state_));
      }
    }
  }

  scoped_ptr<DictionaryValue> network_info(new DictionaryValue());
  network_info->SetInteger(kNetworkInfoKeyActivationState,
                           static_cast<int>(activation_state_));
  network_info->SetBoolean(kNetworkInfoKeyConnectable, connectable_);
  network_info->SetBoolean(kNetworkInfoKeyConnected, connected_);
  network_info->SetBoolean(kNetworkInfoKeyConnecting, connecting_);
  network_info->SetString(kNetworkInfoKeyIconURL, icon_url_);
  network_info->SetString(kNetworkInfoKeyNetworkName, name_);
  network_info->SetString(kNetworkInfoKeyNetworkStatus, status);
  network_info->SetInteger(kNetworkInfoKeyNetworkType,
                           static_cast<int>(connection_type_));
  network_info->SetBoolean(kNetworkInfoKeyRemembered, remembered_);
  network_info->SetString(kNetworkInfoKeyServicePath, service_path_);
  network_info->SetBoolean(kNetworkInfoKeyPolicyManaged, policy_managed_);
  return network_info.release();
}

// Pulls IP information out of a shill service properties dictionary. If
// |static_ip| is true, then it fetches "StaticIP.*" properties. If not, then it
// fetches "SavedIP.*" properties. Caller must take ownership of returned
// dictionary.  If non-NULL, |ip_parameters_set| returns a count of the number
// of IP routing parameters that get set.
DictionaryValue* BuildIPInfoDictionary(const DictionaryValue& shill_properties,
                                       bool static_ip,
                                       int* routing_parameters_set) {
  std::string address_key;
  std::string prefix_len_key;
  std::string gateway_key;
  std::string name_servers_key;
  if (static_ip) {
    address_key = shill::kStaticIPAddressProperty;
    prefix_len_key = shill::kStaticIPPrefixlenProperty;
    gateway_key = shill::kStaticIPGatewayProperty;
    name_servers_key = shill::kStaticIPNameServersProperty;
  } else {
    address_key = shill::kSavedIPAddressProperty;
    prefix_len_key = shill::kSavedIPPrefixlenProperty;
    gateway_key = shill::kSavedIPGatewayProperty;
    name_servers_key = shill::kSavedIPNameServersProperty;
  }

  scoped_ptr<DictionaryValue> ip_info_dict(new DictionaryValue);
  std::string address;
  int routing_parameters = 0;
  if (shill_properties.GetStringWithoutPathExpansion(address_key, &address)) {
    ip_info_dict->SetString(kIpConfigAddress, address);
    VLOG(2) << "Found " << address_key << ": " << address;
    routing_parameters++;
  }
  int prefix_len = -1;
  if (shill_properties.GetIntegerWithoutPathExpansion(
      prefix_len_key, &prefix_len)) {
    ip_info_dict->SetInteger(kIpConfigPrefixLength, prefix_len);
    std::string netmask =
        chromeos::network_util::PrefixLengthToNetmask(prefix_len);
    ip_info_dict->SetString(kIpConfigNetmask, netmask);
    VLOG(2) << "Found " << prefix_len_key << ": "
            <<  prefix_len << " (" << netmask << ")";
    routing_parameters++;
  }
  std::string gateway;
  if (shill_properties.GetStringWithoutPathExpansion(gateway_key, &gateway)) {
    ip_info_dict->SetString(kIpConfigGateway, gateway);
    VLOG(2) << "Found " << gateway_key << ": " << gateway;
    routing_parameters++;
  }
  if (routing_parameters_set)
    *routing_parameters_set = routing_parameters;

  std::string name_servers;
  if (shill_properties.GetStringWithoutPathExpansion(
      name_servers_key, &name_servers)) {
    ip_info_dict->SetString(kIpConfigNameServers, name_servers);
    VLOG(2) << "Found " << name_servers_key << ": " << name_servers;
  }

  return ip_info_dict.release();
}

static bool CanForgetNetworkType(int type) {
  return type == chromeos::TYPE_WIFI ||
         type == chromeos::TYPE_WIMAX ||
         type == chromeos::TYPE_VPN;
}

static bool CanAddNetworkType(int type) {
  return type == chromeos::TYPE_WIFI ||
         type == chromeos::TYPE_VPN ||
         type == chromeos::TYPE_CELLULAR;
}

// Decorate pref value as CoreOptionsHandler::CreateValueForPref() does and
// store it under |key| in |settings|. Takes ownership of |value|.
void SetValueDictionary(
    DictionaryValue* settings,
    const char* key,
    base::Value* value,
    const chromeos::NetworkPropertyUIData& ui_data) {
  DictionaryValue* dict = new DictionaryValue();
  // DictionaryValue::Set() takes ownership of |value|.
  dict->Set(kTagValue, value);
  const base::Value* recommended_value = ui_data.default_value();
  if (ui_data.managed())
    dict->SetString(kTagControlledBy, kTagPolicy);
  else if (recommended_value && recommended_value->Equals(value))
    dict->SetString(kTagControlledBy, kTagRecommended);

  if (recommended_value)
    dict->Set(kTagRecommendedValue, recommended_value->DeepCopy());
  settings->Set(key, dict);
}

// Fills |dictionary| with the configuration details of |vpn|. |onc| is required
// for augmenting the policy-managed information.
void PopulateVPNDetails(
    const chromeos::VirtualNetwork* vpn,
    const base::DictionaryValue& onc,
    DictionaryValue* dictionary) {
  dictionary->SetString(kTagService_name, vpn->name());
  bool remembered = (vpn->profile_type() != chromeos::PROFILE_NONE);
  dictionary->SetBoolean(kTagRemembered, remembered);
  dictionary->SetString(kTagProvider_type, vpn->GetProviderTypeString());
  dictionary->SetString(kTagUsername, vpn->username());

  chromeos::NetworkPropertyUIData hostname_ui_data;
  hostname_ui_data.ParseOncProperty(
      vpn->ui_data(), &onc,
      base::StringPrintf("%s.%s",
                         chromeos::onc::network_config::kVPN,
                         chromeos::onc::vpn::kHost));
  SetValueDictionary(dictionary, kTagServerHostname,
                     new base::StringValue(vpn->server_hostname()),
                     hostname_ui_data);
}

// Activate the cellular device pointed to by the service path.
void Activate(std::string service_path) {
  chromeos::Network* network = NULL;
  if (!service_path.empty()) {
    network = chromeos::CrosLibrary::Get()->GetNetworkLibrary()->
        FindNetworkByPath(service_path);
  } else {
    NOTREACHED();
    return;
  }

  if (network->type() != chromeos::TYPE_CELLULAR)
    return;

  chromeos::CellularNetwork* cellular =
      static_cast<chromeos::CellularNetwork*>(network);
  if (cellular->activation_state() != chromeos::ACTIVATION_STATE_ACTIVATED)
    cellular->StartActivation();
}

// Check if the current cellular device can be activated by directly calling
// it's activate function instead of going through the activation process.
// Note: Currently Sprint is the only carrier that uses this.
bool UseDirectActivation() {
  const chromeos::NetworkDevice* device =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary()->FindCellularDevice();
  return device && (device->carrier() == shill::kCarrierSprint);
}

// Given a list of supported carrier's by the device, return the index of
// the carrier the device is currently using.
int FindCurrentCarrierIndex(const base::ListValue* carriers,
                            const chromeos::NetworkDevice* device) {
  DCHECK(carriers);
  DCHECK(device);

  bool gsm = (device->technology_family() == chromeos::TECHNOLOGY_FAMILY_GSM);
  int index = 0;
  for (base::ListValue::const_iterator it = carriers->begin();
       it != carriers->end();
       ++it, ++index) {
    std::string value;
    if ((*it)->GetAsString(&value)) {
      // For GSM devices the device name will be empty, so simply select
      // the Generic UMTS carrier option if present.
      if (gsm && (value == shill::kCarrierGenericUMTS)) {
        return index;
      } else {
        // For other carriers, the service name will match the carrier name.
        if (value == device->carrier())
          return index;
      }
    }
  }

  return -1;
}

}  // namespace

namespace options {

InternetOptionsHandler::InternetOptionsHandler()
  : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  registrar_.Add(this, chrome::NOTIFICATION_REQUIRE_PIN_SETTING_CHANGE_ENDED,
      content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_ENTER_PIN_ENDED,
      content::NotificationService::AllSources());
  cros_ = chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  if (cros_) {
    cros_->AddNetworkManagerObserver(this);
    MonitorNetworks();
  }
}

InternetOptionsHandler::~InternetOptionsHandler() {
  if (cros_) {
    cros_->RemoveNetworkManagerObserver(this);
    cros_->RemoveObserverForAllNetworks(this);
  }
}

void InternetOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {

    // Main settings page.

    { "ethernetTitle", IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET },
    { "wifiTitle", IDS_OPTIONS_SETTINGS_SECTION_TITLE_WIFI_NETWORK },
    // TODO(zelidrag): Change details title to Wimax once we get strings.
    { "wimaxTitle", IDS_OPTIONS_SETTINGS_SECTION_TITLE_CELLULAR_NETWORK },
    { "cellularTitle", IDS_OPTIONS_SETTINGS_SECTION_TITLE_CELLULAR_NETWORK },
    { "vpnTitle", IDS_OPTIONS_SETTINGS_SECTION_TITLE_PRIVATE_NETWORK },
    { "airplaneModeTitle", IDS_OPTIONS_SETTINGS_SECTION_TITLE_AIRPLANE_MODE },
    { "airplaneModeLabel", IDS_OPTIONS_SETTINGS_NETWORK_AIRPLANE_MODE_LABEL },
    { "networkNotConnected", IDS_OPTIONS_SETTINGS_NETWORK_NOT_CONNECTED },
    { "networkConnected", IDS_CHROMEOS_NETWORK_STATE_READY },
    { "joinOtherNetwork", IDS_OPTIONS_SETTINGS_NETWORK_OTHER },
    { "networkOffline", IDS_OPTIONS_SETTINGS_NETWORK_OFFLINE },
    { "networkDisabled", IDS_OPTIONS_SETTINGS_NETWORK_DISABLED },
    { "networkOnline", IDS_OPTIONS_SETTINGS_NETWORK_ONLINE },
    { "networkOptions", IDS_OPTIONS_SETTINGS_NETWORK_OPTIONS },
    { "turnOffWifi", IDS_OPTIONS_SETTINGS_NETWORK_DISABLE_WIFI },
    { "turnOffCellular", IDS_OPTIONS_SETTINGS_NETWORK_DISABLE_CELLULAR },
    { "disconnectNetwork", IDS_OPTIONS_SETTINGS_DISCONNECT },
    { "preferredNetworks", IDS_OPTIONS_SETTINGS_PREFERRED_NETWORKS_LABEL },
    { "preferredNetworksPage", IDS_OPTIONS_SETTINGS_PREFERRED_NETWORKS_TITLE },
    { "useSharedProxies", IDS_OPTIONS_SETTINGS_USE_SHARED_PROXIES },
    { "addConnectionTitle",
      IDS_OPTIONS_SETTINGS_SECTION_TITLE_ADD_CONNECTION },
    { "addConnectionWifi", IDS_OPTIONS_SETTINGS_ADD_CONNECTION_WIFI },
    { "addConnectionVPN", IDS_STATUSBAR_NETWORK_ADD_VPN },
    { "otherCellularNetworks", IDS_OPTIONS_SETTINGS_OTHER_CELLULAR_NETWORKS },
    { "enableDataRoaming", IDS_OPTIONS_SETTINGS_ENABLE_DATA_ROAMING },
    { "disableDataRoaming", IDS_OPTIONS_SETTINGS_DISABLE_DATA_ROAMING },
    { "dataRoamingDisableToggleTooltip",
      IDS_OPTIONS_SETTINGS_TOGGLE_DATA_ROAMING_RESTRICTION },
    { "activateNetwork", IDS_STATUSBAR_NETWORK_DEVICE_ACTIVATE },

    // Internet details dialog.

    { "changeProxyButton",
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CHANGE_PROXY_BUTTON },
    { "managedNetwork", IDS_OPTIONS_SETTINGS_MANAGED_NETWORK },
    { "wifiNetworkTabLabel", IDS_OPTIONS_SETTINGS_INTERNET_TAB_CONNECTION },
    { "vpnTabLabel", IDS_OPTIONS_SETTINGS_INTERNET_TAB_VPN },
    { "cellularConnTabLabel", IDS_OPTIONS_SETTINGS_INTERNET_TAB_CONNECTION },
    { "cellularDeviceTabLabel", IDS_OPTIONS_SETTINGS_INTERNET_TAB_DEVICE },
    { "networkTabLabel", IDS_OPTIONS_SETTINGS_INTERNET_TAB_NETWORK },
    { "securityTabLabel", IDS_OPTIONS_SETTINGS_INTERNET_TAB_SECURITY },
    { "proxyTabLabel", IDS_OPTIONS_SETTINGS_INTERNET_TAB_PROXY },
    { "connectionState", IDS_OPTIONS_SETTINGS_INTERNET_CONNECTION_STATE },
    { "inetAddress", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_ADDRESS },
    { "inetNetmask", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SUBNETMASK },
    { "inetGateway", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_GATEWAY },
    { "inetNameServers", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_DNSSERVER },
    { "ipAutomaticConfiguration",
        IDS_OPTIONS_SETTINGS_INTERNET_IP_AUTOMATIC_CONFIGURATION },
    { "automaticNameServers",
        IDS_OPTIONS_SETTINGS_INTERNET_AUTOMATIC_NAME_SERVERS },
    { "userNameServer1", IDS_OPTIONS_SETTINGS_INTERNET_USER_NAME_SERVER_1 },
    { "userNameServer2", IDS_OPTIONS_SETTINGS_INTERNET_USER_NAME_SERVER_2 },
    { "userNameServer3", IDS_OPTIONS_SETTINGS_INTERNET_USER_NAME_SERVER_3 },
    { "userNameServer4", IDS_OPTIONS_SETTINGS_INTERNET_USER_NAME_SERVER_4 },
    { "googleNameServers", IDS_OPTIONS_SETTINGS_INTERNET_GOOGLE_NAME_SERVERS },
    { "userNameServers", IDS_OPTIONS_SETTINGS_INTERNET_USER_NAME_SERVERS },
    { "hardwareAddress",
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_HARDWARE_ADDRESS },
    { "detailsInternetDismiss", IDS_CLOSE },
    { "activateButton", IDS_OPTIONS_SETTINGS_ACTIVATE },
    { "buyplanButton", IDS_OPTIONS_SETTINGS_BUY_PLAN },
    { "connectButton", IDS_OPTIONS_SETTINGS_CONNECT },
    { "disconnectButton", IDS_OPTIONS_SETTINGS_DISCONNECT },
    { "viewAccountButton", IDS_STATUSBAR_NETWORK_VIEW_ACCOUNT },

    // TODO(zelidrag): Change details title to Wimax once we get strings.
    { "wimaxConnTabLabel", IDS_OPTIONS_SETTINGS_INTERNET_TAB_CONNECTION },

    // Wifi Tab.

    { "accessLockedMsg", IDS_STATUSBAR_NETWORK_LOCKED },
    { "inetSsid", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_NETWORK_ID },
    { "inetBssid", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_NETWORK_BSSID },
    { "inetEncryption",
      IDS_OPTIONS_SETTIGNS_INTERNET_OPTIONS_NETWORK_ENCRYPTION },
    { "inetFrequency",
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_NETWORK_FREQUENCY },
    { "inetFrequencyFormat",
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_NETWORK_FREQUENCY_MHZ },
    { "inetSignalStrength",
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_NETWORK_STRENGTH },
    { "inetSignalStrengthFormat",
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_NETWORK_STRENGTH_PERCENTAGE },
    { "inetPassProtected",
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_NET_PROTECTED },
    { "inetNetworkShared",
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_NETWORK_SHARED },
    { "inetPreferredNetwork",
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PREFER_NETWORK },
    { "inetAutoConnectNetwork",
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_AUTO_CONNECT },
    { "inetLogin", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_LOGIN },
    { "inetShowPass", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SHOWPASSWORD },
    { "inetPassPrompt", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PASSWORD },
    { "inetSsidPrompt", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SSID },
    { "inetStatus", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_STATUS_TITLE },
    { "inetConnect", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CONNECT_TITLE },

    // VPN Tab.

    { "inetServiceName",
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_VPN_SERVICE_NAME },
    { "inetServerHostname",
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_VPN_SERVER_HOSTNAME },
    { "inetProviderType",
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_VPN_PROVIDER_TYPE },
    { "inetUsername", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_VPN_USERNAME },

    // Cellular Tab.

    { "serviceName", IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_SERVICE_NAME },
    { "networkTechnology",
      IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_NETWORK_TECHNOLOGY },
    { "operatorName", IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_OPERATOR },
    { "operatorCode", IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_OPERATOR_CODE },
    { "activationState",
      IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_ACTIVATION_STATE },
    { "roamingState", IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_ROAMING_STATE },
    { "restrictedPool",
      IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_RESTRICTED_POOL },
    { "errorState", IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_ERROR_STATE },
    { "manufacturer", IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_MANUFACTURER },
    { "modelId", IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_MODEL_ID },
    { "firmwareRevision",
      IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_FIRMWARE_REVISION },
    { "hardwareRevision",
      IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_HARDWARE_REVISION },
    { "prlVersion", IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_PRL_VERSION },
    { "cellularApnLabel", IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_APN },
    { "cellularApnOther", IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_APN_OTHER },
    { "cellularApnUsername",
      IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_APN_USERNAME },
    { "cellularApnPassword",
      IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_APN_PASSWORD },
    { "cellularApnUseDefault",
      IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_APN_CLEAR },
    { "cellularApnSet", IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_APN_SET },
    { "cellularApnCancel", IDS_CANCEL },

    // Security Tab.

    { "accessSecurityTabLink",
      IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_ACCESS_SECURITY_TAB },
    { "lockSimCard", IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_LOCK_SIM_CARD },
    { "changePinButton",
      IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_CHANGE_PIN_BUTTON },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));

  std::string owner;
  chromeos::CrosSettings::Get()->GetString(chromeos::kDeviceOwner, &owner);
  localized_strings->SetString("ownerUserId", UTF8ToUTF16(owner));

  DictionaryValue* network_dictionary = new DictionaryValue;
  FillNetworkInfo(network_dictionary);
  localized_strings->Set("networkData", network_dictionary);
}

void InternetOptionsHandler::InitializePage() {
  DictionaryValue dictionary;
  dictionary.SetString(kTagCellular,
      GetIconDataUrl(IDR_AURA_UBER_TRAY_NETWORK_BARS_DARK));
  dictionary.SetString(kTagWifi,
      GetIconDataUrl(IDR_AURA_UBER_TRAY_NETWORK_ARCS_DARK));
  dictionary.SetString(kTagVpn,
      GetIconDataUrl(IDR_AURA_UBER_TRAY_NETWORK_VPN));
  web_ui()->CallJavascriptFunction(kSetDefaultNetworkIconsFunction,
                                   dictionary);
  cros_->RequestNetworkScan();
}

void InternetOptionsHandler::RegisterMessages() {
  // Setup handlers specific to this panel.
  web_ui()->RegisterMessageCallback(kNetworkCommandMessage,
      base::Bind(&InternetOptionsHandler::NetworkCommandCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kRefreshNetworksMessage,
      base::Bind(&InternetOptionsHandler::RefreshNetworksCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kSetPreferNetworkMessage,
      base::Bind(&InternetOptionsHandler::SetPreferNetworkCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kSetAutoConnectMessage,
      base::Bind(&InternetOptionsHandler::SetAutoConnectCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kSetIPConfigMessage,
      base::Bind(&InternetOptionsHandler::SetIPConfigCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kEnableWifiMessage,
      base::Bind(&InternetOptionsHandler::EnableWifiCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kDisableWifiMessage,
      base::Bind(&InternetOptionsHandler::DisableWifiCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kEnableCellularMessage,
      base::Bind(&InternetOptionsHandler::EnableCellularCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kDisableCellularMessage,
      base::Bind(&InternetOptionsHandler::DisableCellularCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kEnableWimaxMessage,
      base::Bind(&InternetOptionsHandler::EnableWimaxCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kDisableWimaxMessage,
      base::Bind(&InternetOptionsHandler::DisableWimaxCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kBuyDataPlanMessage,
      base::Bind(&InternetOptionsHandler::BuyDataPlanCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kShowMorePlanInfoMessage,
      base::Bind(&InternetOptionsHandler::ShowMorePlanInfoCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kSetApnMessage,
      base::Bind(&InternetOptionsHandler::SetApnCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kSetCarrierMessage,
      base::Bind(&InternetOptionsHandler::SetCarrierCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kSetSimCardLockMessage,
      base::Bind(&InternetOptionsHandler::SetSimCardLockCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kChangePinMessage,
      base::Bind(&InternetOptionsHandler::ChangePinCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kToggleAirplaneModeMessage,
      base::Bind(&InternetOptionsHandler::ToggleAirplaneModeCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(kSetServerHostname,
      base::Bind(&InternetOptionsHandler::SetServerHostnameCallback,
                 base::Unretained(this)));
}

void InternetOptionsHandler::EnableWifiCallback(const ListValue* args) {
  cros_->EnableWifiNetworkDevice(true);
}

void InternetOptionsHandler::DisableWifiCallback(const ListValue* args) {
  cros_->EnableWifiNetworkDevice(false);
}

void InternetOptionsHandler::EnableCellularCallback(const ListValue* args) {
  // TODO(nkostylev): Code duplication, see NetworkMenu::ToggleCellular().
  const chromeos::NetworkDevice* mobile = cros_->FindMobileDevice();
  if (!mobile) {
    LOG(ERROR) << "Didn't find mobile device, it should have been available.";
    cros_->EnableCellularNetworkDevice(true);
  } else if (!mobile->is_sim_locked()) {
    if (mobile->is_sim_absent()) {
      std::string setup_url;
      chromeos::MobileConfig* config = chromeos::MobileConfig::GetInstance();
      if (config->IsReady()) {
        const chromeos::MobileConfig::LocaleConfig* locale_config =
            config->GetLocaleConfig();
        if (locale_config)
          setup_url = locale_config->setup_url();
      }
      if (!setup_url.empty()) {
        chrome::ShowSingletonTab(GetAppropriateBrowser(), GURL(setup_url));
      } else {
        // TODO(nkostylev): Show generic error message. http://crosbug.com/15444
      }
    } else {
      cros_->EnableCellularNetworkDevice(true);
    }
  } else {
    chromeos::SimDialogDelegate::ShowDialog(GetNativeWindow(),
        chromeos::SimDialogDelegate::SIM_DIALOG_UNLOCK);
  }
}

void InternetOptionsHandler::DisableCellularCallback(const ListValue* args) {
  cros_->EnableCellularNetworkDevice(false);
}

void InternetOptionsHandler::EnableWimaxCallback(const ListValue* args) {
  cros_->EnableWimaxNetworkDevice(true);
}

void InternetOptionsHandler::DisableWimaxCallback(const ListValue* args) {
  cros_->EnableWimaxNetworkDevice(false);
}

void InternetOptionsHandler::ShowMorePlanInfoCallback(const ListValue* args) {
  if (!web_ui())
    return;

  const chromeos::CellularNetwork* cellular = cros_->cellular_network();
  if (!cellular)
    return;

  web_ui()->GetWebContents()->OpenURL(content::OpenURLParams(
      cellular->GetAccountInfoUrl(), content::Referrer(),
      NEW_FOREGROUND_TAB,
      content::PAGE_TRANSITION_LINK, false));
}

void InternetOptionsHandler::BuyDataPlanCallback(const ListValue* args) {
  if (!web_ui())
    return;

  std::string service_path;
  if (args->GetSize() != 1 || !args->GetString(0, &service_path)) {
    NOTREACHED();
    return;
  }
  ash::Shell::GetInstance()->delegate()->OpenMobileSetup(service_path);
}

void InternetOptionsHandler::SetApnCallback(const ListValue* args) {
  std::string service_path;
  std::string apn;
  std::string username;
  std::string password;
  if (args->GetSize() != 4 ||
      !args->GetString(0, &service_path) ||
      !args->GetString(1, &apn) ||
      !args->GetString(2, &username) ||
      !args->GetString(3, &password)) {
    NOTREACHED();
    return;
  }

  chromeos::CellularNetwork* network =
        cros_->FindCellularNetworkByPath(service_path);
  if (network) {
    network->SetApn(chromeos::CellularApn(
        apn, network->apn().network_id, username, password));
  }
}

void InternetOptionsHandler::CarrierStatusCallback(
    const std::string& service_path,
    chromeos::NetworkMethodErrorType error,
    const std::string& error_message) {
  if ((error == chromeos::NETWORK_METHOD_ERROR_NONE) &&
      UseDirectActivation()) {
    Activate(service_path);
    UpdateConnectionData(cros_->FindNetworkByPath(service_path));
  }

  UpdateCarrier();
}


void InternetOptionsHandler::SetCarrierCallback(const ListValue* args) {
  std::string service_path;
  std::string carrier;
  if (args->GetSize() != 2 ||
      !args->GetString(0, &service_path) ||
      !args->GetString(1, &carrier)) {
    NOTREACHED();
    return;
  }

  chromeos::NetworkLibrary* cros_net =
      chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  if (cros_net) {
    cros_net->SetCarrier(
        carrier,
        base::Bind(&InternetOptionsHandler::CarrierStatusCallback,
                   weak_factory_.GetWeakPtr()));
  }
}

void InternetOptionsHandler::SetSimCardLockCallback(const ListValue* args) {
  bool require_pin_new_value;
  if (!args->GetBoolean(0, &require_pin_new_value)) {
    NOTREACHED();
    return;
  }
  // 1. Bring up SIM unlock dialog, pass new RequirePin setting in URL.
  // 2. Dialog will ask for current PIN in any case.
  // 3. If card is locked it will first call PIN unlock operation
  // 4. Then it will call Set RequirePin, passing the same PIN.
  // 5. We'll get notified by REQUIRE_PIN_SETTING_CHANGE_ENDED notification.
  chromeos::SimDialogDelegate::SimDialogMode mode;
  if (require_pin_new_value)
    mode = chromeos::SimDialogDelegate::SIM_DIALOG_SET_LOCK_ON;
  else
    mode = chromeos::SimDialogDelegate::SIM_DIALOG_SET_LOCK_OFF;
  chromeos::SimDialogDelegate::ShowDialog(GetNativeWindow(), mode);
}

void InternetOptionsHandler::ChangePinCallback(const ListValue* args) {
  chromeos::SimDialogDelegate::ShowDialog(GetNativeWindow(),
      chromeos::SimDialogDelegate::SIM_DIALOG_CHANGE_PIN);
}

void InternetOptionsHandler::RefreshNetworksCallback(const ListValue* args) {
  cros_->RequestNetworkScan();
}

std::string InternetOptionsHandler::GetIconDataUrl(int resource_id) const {
  gfx::ImageSkia* icon =
      ResourceBundle::GetSharedInstance().GetImageSkiaNamed(resource_id);
  gfx::ImageSkiaRep image_rep = icon->GetRepresentation(
      web_ui()->GetDeviceScaleFactor());
  return web_ui_util::GetBitmapDataUrl(image_rep.sk_bitmap());
}

void InternetOptionsHandler::RefreshNetworkData() {
  DictionaryValue dictionary;
  FillNetworkInfo(&dictionary);
  web_ui()->CallJavascriptFunction(
      kRefreshNetworkDataFunction, dictionary);
}

void InternetOptionsHandler::UpdateConnectionData(
    const chromeos::Network* network) {
  DictionaryValue dictionary;
  PopulateConnectionDetails(network, &dictionary);
  web_ui()->CallJavascriptFunction(
      kUpdateConnectionDataFunction, dictionary);
}

void InternetOptionsHandler::UpdateCarrier() {
  web_ui()->CallJavascriptFunction(kUpdateCarrierFunction);
}

void InternetOptionsHandler::OnNetworkManagerChanged(
    chromeos::NetworkLibrary* cros) {
  if (!web_ui())
    return;
  MonitorNetworks();
  RefreshNetworkData();
}

void InternetOptionsHandler::OnNetworkChanged(
    chromeos::NetworkLibrary* cros,
    const chromeos::Network* network) {
  if (web_ui()) {
    RefreshNetworkData();
    UpdateConnectionData(network);
  }
}

// Monitor wireless networks for changes. It is only necessary
// to set up individual observers for the cellular networks
// (if any) and for the connected Wi-Fi network (if any). The
// only change we are interested in for Wi-Fi networks is signal
// strength. For non-connected Wi-Fi networks, all information is
// reported via scan results, which trigger network manager
// updates. Only the connected Wi-Fi network has changes reported
// via service property updates.
void InternetOptionsHandler::MonitorNetworks() {
  cros_->RemoveObserverForAllNetworks(this);
  const chromeos::WifiNetwork* wifi_network = cros_->wifi_network();
  if (wifi_network)
    cros_->AddNetworkObserver(wifi_network->service_path(), this);

  // Always monitor all mobile networks, if any, so that changes
  // in network technology, roaming status, and signal strength
  // will be shown.
  const chromeos::WimaxNetworkVector& wimax_networks =
      cros_->wimax_networks();
  for (size_t i = 0; i < wimax_networks.size(); ++i) {
    chromeos::WimaxNetwork* wimax_network = wimax_networks[i];
    cros_->AddNetworkObserver(wimax_network->service_path(), this);
  }
  const chromeos::CellularNetworkVector& cell_networks =
      cros_->cellular_networks();
  for (size_t i = 0; i < cell_networks.size(); ++i) {
    chromeos::CellularNetwork* cell_network = cell_networks[i];
    cros_->AddNetworkObserver(cell_network->service_path(), this);
  }
  const chromeos::VirtualNetwork* virtual_network = cros_->virtual_network();
  if (virtual_network)
    cros_->AddNetworkObserver(virtual_network->service_path(), this);
}

void InternetOptionsHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  OptionsPageUIHandler::Observe(type, source, details);
  if (type == chrome::NOTIFICATION_REQUIRE_PIN_SETTING_CHANGE_ENDED) {
    base::FundamentalValue require_pin(*content::Details<bool>(details).ptr());
    web_ui()->CallJavascriptFunction(
        kUpdateSecurityTabFunction, require_pin);
  } else if (type == chrome::NOTIFICATION_ENTER_PIN_ENDED) {
    // We make an assumption (which is valid for now) that the SIM
    // unlock dialog is put up only when the user is trying to enable
    // mobile data.
    bool cancelled = *content::Details<bool>(details).ptr();
    if (cancelled)
      RefreshNetworkData();
    // The case in which the correct PIN was entered and the SIM is
    // now unlocked is handled in NetworkMenuButton.
  }
}

void InternetOptionsHandler::SetServerHostnameCallback(const ListValue* args) {
  std::string service_path;
  std::string server_hostname;

  if (args->GetSize() < 2 ||
      !args->GetString(0, &service_path) ||
      !args->GetString(1, &server_hostname)) {
    NOTREACHED();
    return;
  }

  chromeos::VirtualNetwork* vpn = cros_->FindVirtualNetworkByPath(service_path);
  if (!vpn)
    return;

  if (server_hostname != vpn->server_hostname())
    vpn->SetServerHostname(server_hostname);
}

void InternetOptionsHandler::SetPreferNetworkCallback(const ListValue* args) {
  std::string service_path;
  std::string prefer_network_str;

  if (args->GetSize() < 2 ||
      !args->GetString(0, &service_path) ||
      !args->GetString(1, &prefer_network_str)) {
    NOTREACHED();
    return;
  }

  chromeos::Network* network = cros_->FindNetworkByPath(service_path);
  if (!network)
    return;

  bool prefer_network = prefer_network_str == kTagTrue;
  if (prefer_network != network->preferred())
    network->SetPreferred(prefer_network);
}

void InternetOptionsHandler::SetAutoConnectCallback(const ListValue* args) {
  std::string service_path;
  std::string auto_connect_str;

  if (args->GetSize() < 2 ||
      !args->GetString(0, &service_path) ||
      !args->GetString(1, &auto_connect_str)) {
    NOTREACHED();
    return;
  }

  chromeos::Network* network = cros_->FindNetworkByPath(service_path);
  if (!network)
    return;

  bool auto_connect = auto_connect_str == kTagTrue;
  if (auto_connect != network->auto_connect())
    network->SetAutoConnect(auto_connect);
}

void InternetOptionsHandler::SetIPConfigCallback(const ListValue* args) {
  std::string service_path;
  bool dhcp_for_ip;
  std::string address;
  std::string netmask;
  std::string gateway;
  std::string name_server_type;
  std::string name_servers;

  if (args->GetSize() < 7 ||
      !args->GetString(0, &service_path) ||
      !args->GetBoolean(1, &dhcp_for_ip) ||
      !args->GetString(2, &address) ||
      !args->GetString(3, &netmask) ||
      !args->GetString(4, &gateway) ||
      !args->GetString(5, &name_server_type) ||
      !args->GetString(6, &name_servers)) {
    NOTREACHED();
    return;
  }

  int dhcp_usage_mask = 0;
  if (dhcp_for_ip) {
    dhcp_usage_mask = (chromeos::NetworkLibrary::USE_DHCP_ADDRESS |
                       chromeos::NetworkLibrary::USE_DHCP_NETMASK |
                       chromeos::NetworkLibrary::USE_DHCP_GATEWAY);
  }
  if (name_server_type == kNameServerTypeAutomatic) {
    dhcp_usage_mask |= chromeos::NetworkLibrary::USE_DHCP_NAME_SERVERS;
    name_servers.clear();
  } else if (name_server_type == kNameServerTypeGoogle) {
    name_servers = kGoogleNameServers;
  }

  cros_->SetIPParameters(service_path,
                         address,
                         netmask,
                         gateway,
                         name_servers,
                         dhcp_usage_mask);
}

void InternetOptionsHandler::PopulateDictionaryDetailsCallback(
    const std::string& service_path,
    const base::DictionaryValue* shill_properties) {
  if (!shill_properties)
    return;
  chromeos::Network* network = cros_->FindNetworkByPath(service_path);
  if (!network)
    return;
  // Have to copy the properties because the object will go out of scope when
  // this function call completes (it's owned by the calling function).
  base::DictionaryValue* shill_props_copy = shill_properties->DeepCopy();
  cros_->GetIPConfigs(
      network->device_path(),
      chromeos::NetworkLibrary::FORMAT_COLON_SEPARATED_HEX,
      base::Bind(&InternetOptionsHandler::PopulateIPConfigsCallback,
                 weak_factory_.GetWeakPtr(),
                 service_path,
                 base::Owned(shill_props_copy)));
}

void InternetOptionsHandler::PopulateIPConfigsCallback(
    const std::string& service_path,
    base::DictionaryValue* shill_properties,
    const chromeos::NetworkIPConfigVector& ipconfigs,
    const std::string& hardware_address) {
  if (!shill_properties)
    return;
  if (VLOG_IS_ON(2)) {
    std::string properties_json;
    base::JSONWriter::WriteWithOptions(shill_properties,
                                       base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                       &properties_json);
    VLOG(2) << "Shill Properties: " << std::endl << properties_json;
  }
  chromeos::Network* network = cros_->FindNetworkByPath(service_path);
  if (!network)
    return;

  Profile::FromWebUI(web_ui())->GetProxyConfigTracker()->UISetCurrentNetwork(
      service_path);

  const chromeos::NetworkUIData& ui_data = network->ui_data();
  const chromeos::NetworkPropertyUIData property_ui_data(ui_data);
  const base::DictionaryValue* onc =
      cros_->FindOncForNetwork(network->unique_id());

  base::DictionaryValue dictionary;
  if (!hardware_address.empty())
    dictionary.SetString(kTagHardwareAddress, hardware_address);

  // The DHCP IPConfig contains the values that are actually in use at the
  // moment, even if some are overridden by static IP values.
  scoped_ptr<DictionaryValue> ipconfig_dhcp(new DictionaryValue);
  std::string ipconfig_name_servers;
  for (chromeos::NetworkIPConfigVector::const_iterator it = ipconfigs.begin();
       it != ipconfigs.end(); ++it) {
    const chromeos::NetworkIPConfig& ipconfig = *it;
    if (ipconfig.type == chromeos::IPCONFIG_TYPE_DHCP) {
      ipconfig_dhcp->SetString(kIpConfigAddress, ipconfig.address);
      VLOG(2) << "Found DHCP Address: " << ipconfig.address;
      ipconfig_dhcp->SetString(kIpConfigNetmask, ipconfig.netmask);
      VLOG(2) << "Found DHCP Netmask: " << ipconfig.netmask;
      ipconfig_dhcp->SetString(kIpConfigGateway, ipconfig.gateway);
      VLOG(2) << "Found DHCP Gateway: " << ipconfig.gateway;
      ipconfig_dhcp->SetString(kIpConfigNameServers, ipconfig.name_servers);
      ipconfig_name_servers = ipconfig.name_servers; // save for later
      VLOG(2) << "Found DHCP Name Servers: " << ipconfig.name_servers;
      break;
    }
  }
  SetValueDictionary(&dictionary, kDictionaryIpConfig, ipconfig_dhcp.release(),
                     property_ui_data);

  std::string name_server_type = kNameServerTypeAutomatic;
  if (shill_properties) {
    int automatic_ip_config = 0;
    scoped_ptr<DictionaryValue> static_ip_dict(
        BuildIPInfoDictionary(*shill_properties, true, &automatic_ip_config));
    dictionary.SetBoolean(kIpConfigAutoConfig, automatic_ip_config == 0);
    DCHECK(automatic_ip_config == 3 || automatic_ip_config == 0)
        << "UI doesn't support automatic specification of individual "
        << "static IP parameters.";
    scoped_ptr<DictionaryValue> saved_ip_dict(
        BuildIPInfoDictionary(*shill_properties, false, NULL));
    dictionary.Set(kDictionarySavedIp, saved_ip_dict.release());

    // Determine what kind of name server setting we have by comparing the
    // StaticIP and Google values with the ipconfig values.
    std::string static_ip_nameservers;
    static_ip_dict->GetString(kIpConfigNameServers, &static_ip_nameservers);

    if (!static_ip_nameservers.empty() &&
        static_ip_nameservers == ipconfig_name_servers) {
      name_server_type = kNameServerTypeUser;
    }
    if (ipconfig_name_servers == kGoogleNameServers) {
      name_server_type = kNameServerTypeGoogle;
    }
    SetValueDictionary(&dictionary,
                       kDictionaryStaticIp,
                       static_ip_dict.release(),
                       property_ui_data);
  } else {
    LOG(ERROR) << "Unable to fetch IP configuration for " << service_path;
    // If we were unable to fetch shill_properties for some reason,
    // then just go with some defaults.
    dictionary.SetBoolean(kIpConfigAutoConfig, false);
    dictionary.Set(kDictionarySavedIp, new DictionaryValue);
    SetValueDictionary(&dictionary, kDictionaryStaticIp, new DictionaryValue,
                       property_ui_data);
  }

  chromeos::ConnectionType type = network->type();
  dictionary.SetInteger(kTagType, type);
  dictionary.SetString(kTagServicePath, network->service_path());
  dictionary.SetString(kTagNameServerType, name_server_type);
  dictionary.SetString(kTagNameServersGoogle, kGoogleNameServers);

  // Only show proxy for remembered networks.
  chromeos::NetworkProfileType network_profile = network->profile_type();
  dictionary.SetBoolean(kTagShowProxy,
                        network_profile != chromeos::PROFILE_NONE);

  // Enable static ip config for ethernet. For wifi, enable if flag is set.
  bool staticIPConfig = type == chromeos::TYPE_ETHERNET ||
      (type == chromeos::TYPE_WIFI &&
       CommandLine::ForCurrentProcess()->HasSwitch(
           switches::kEnableStaticIPConfig));
  dictionary.SetBoolean(kTagShowStaticIPConfig, staticIPConfig);

  dictionary.SetBoolean(kTagShowPreferred,
                        network_profile == chromeos::PROFILE_USER);
  SetValueDictionary(&dictionary, kTagPreferred,
                     new base::FundamentalValue(network->preferred()),
                     property_ui_data);

  chromeos::NetworkPropertyUIData auto_connect_ui_data(ui_data);
  if (type == chromeos::TYPE_WIFI) {
    auto_connect_ui_data.ParseOncProperty(
        ui_data, onc,
        base::StringPrintf("%s.%s",
                           chromeos::onc::network_config::kWiFi,
                           chromeos::onc::wifi::kAutoConnect));
  }
  SetValueDictionary(&dictionary, kTagAutoConnect,
                     new base::FundamentalValue(network->auto_connect()),
                     auto_connect_ui_data);

  PopulateConnectionDetails(network, &dictionary);
  web_ui()->CallJavascriptFunction(
      kShowDetailedInfoFunction, dictionary);
}

void InternetOptionsHandler::PopulateConnectionDetails(
    const chromeos::Network* network, DictionaryValue* dictionary) {
  chromeos::ConnectionType type = network->type();
  dictionary->SetBoolean(kTagConnecting, network->connecting());
  dictionary->SetBoolean(kTagConnected, network->connected());
  dictionary->SetString(kTagConnectionState, network->GetStateString());
  dictionary->SetString(kTagNetworkName, network->name());

  if (type == chromeos::TYPE_WIFI) {
    dictionary->SetBoolean(kTagDeviceConnected, cros_->wifi_connected());
    PopulateWifiDetails(static_cast<const chromeos::WifiNetwork*>(network),
                        dictionary);
  } else if (type == chromeos::TYPE_WIMAX) {
    dictionary->SetBoolean(kTagDeviceConnected, cros_->wimax_connected());
    PopulateWimaxDetails(static_cast<const chromeos::WimaxNetwork*>(network),
                         dictionary);
  } else if (type == chromeos::TYPE_CELLULAR) {
    dictionary->SetBoolean(kTagDeviceConnected, cros_->cellular_connected());
    PopulateCellularDetails(
        static_cast<const chromeos::CellularNetwork*>(network),
        dictionary);
  } else if (type == chromeos::TYPE_VPN) {
    dictionary->SetBoolean(kTagDeviceConnected,
                           cros_->virtual_network_connected());
    const base::DictionaryValue* onc =
        cros_->FindOncForNetwork(network->unique_id());
    PopulateVPNDetails(static_cast<const chromeos::VirtualNetwork*>(network),
                       *onc,
                       dictionary);
  } else if (type == chromeos::TYPE_ETHERNET) {
    dictionary->SetBoolean(kTagDeviceConnected, cros_->ethernet_connected());
  }
}

void InternetOptionsHandler::PopulateWifiDetails(
    const chromeos::WifiNetwork* wifi,
    DictionaryValue* dictionary) {
  dictionary->SetString(kTagSsid, wifi->name());
  bool remembered = (wifi->profile_type() != chromeos::PROFILE_NONE);
  dictionary->SetBoolean(kTagRemembered, remembered);
  bool shared = wifi->profile_type() == chromeos::PROFILE_SHARED;
  dictionary->SetBoolean(kTagShared, shared);
  dictionary->SetString(kTagEncryption, wifi->GetEncryptionString());
  dictionary->SetString(kTagBssid, wifi->bssid());
  dictionary->SetInteger(kTagFrequency, wifi->frequency());
  dictionary->SetInteger(kTagStrength, wifi->strength());
}

void InternetOptionsHandler::PopulateWimaxDetails(
    const chromeos::WimaxNetwork* wimax,
    DictionaryValue* dictionary) {
  bool remembered = (wimax->profile_type() != chromeos::PROFILE_NONE);
  dictionary->SetBoolean(kTagRemembered, remembered);
  bool shared = wimax->profile_type() == chromeos::PROFILE_SHARED;
  dictionary->SetBoolean(kTagShared, shared);
  if (wimax->passphrase_required())
    dictionary->SetString(kTagIdentity, wimax->eap_identity());

  dictionary->SetInteger(kTagStrength, wimax->strength());
}

DictionaryValue* InternetOptionsHandler::CreateDictionaryFromCellularApn(
    const chromeos::CellularApn& apn) {
  DictionaryValue* dictionary = new DictionaryValue();
  dictionary->SetString(kTagApn, apn.apn);
  dictionary->SetString(kTagNetworkId, apn.network_id);
  dictionary->SetString(kTagUsername, apn.username);
  dictionary->SetString(kTagPassword, apn.password);
  dictionary->SetString(kTagName, apn.name);
  dictionary->SetString(kTagLocalizedName, apn.localized_name);
  dictionary->SetString(kTagLanguage, apn.language);
  return dictionary;
}

void InternetOptionsHandler::PopulateCellularDetails(
    const chromeos::CellularNetwork* cellular,
    DictionaryValue* dictionary) {
  dictionary->SetBoolean(kTagCarrierSelectFlag,
                         CommandLine::ForCurrentProcess()->HasSwitch(
                             switches::kEnableCarrierSwitching));
  // Cellular network / connection settings.
  dictionary->SetString(kTagServiceName, cellular->name());
  dictionary->SetString(kTagNetworkTechnology,
                        cellular->GetNetworkTechnologyString());
  dictionary->SetString(kTagOperatorName, cellular->operator_name());
  dictionary->SetString(kTagOperatorCode, cellular->operator_code());
  dictionary->SetString(kTagActivationState,
                        cellular->GetActivationStateString());
  dictionary->SetString(kTagRoamingState,
                        cellular->GetRoamingStateString());
  dictionary->SetString(kTagRestrictedPool,
                        cellular->restricted_pool() ?
                        l10n_util::GetStringUTF8(
                            IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL) :
                        l10n_util::GetStringUTF8(
                            IDS_CONFIRM_MESSAGEBOX_NO_BUTTON_LABEL));
  dictionary->SetString(kTagErrorState, cellular->GetErrorString());
  dictionary->SetString(kTagSupportUrl, cellular->payment_url());

  dictionary->Set(kTagApn, CreateDictionaryFromCellularApn(cellular->apn()));
  dictionary->Set(kTagLastGoodApn,
                  CreateDictionaryFromCellularApn(cellular->last_good_apn()));

  // Device settings.
  const chromeos::NetworkDevice* device =
      cros_->FindNetworkDeviceByPath(cellular->device_path());
  if (device) {
    const chromeos::NetworkPropertyUIData cellular_property_ui_data(
        cellular->ui_data());
    dictionary->SetString(kTagManufacturer, device->manufacturer());
    dictionary->SetString(kTagModelId, device->model_id());
    dictionary->SetString(kTagFirmwareRevision, device->firmware_revision());
    dictionary->SetString(kTagHardwareRevision, device->hardware_revision());
    dictionary->SetString(kTagPrlVersion,
                          base::StringPrintf("%u", device->prl_version()));
    dictionary->SetString(kTagMeid, device->meid());
    dictionary->SetString(kTagImei, device->imei());
    dictionary->SetString(kTagMdn, device->mdn());
    dictionary->SetString(kTagImsi, device->imsi());
    dictionary->SetString(kTagEsn, device->esn());
    dictionary->SetString(kTagMin, device->min());
    dictionary->SetBoolean(kTagGsm,
        device->technology_family() == chromeos::TECHNOLOGY_FAMILY_GSM);
    SetValueDictionary(
        dictionary, kTagSimCardLockEnabled,
        new base::FundamentalValue(
            device->sim_pin_required() == chromeos::SIM_PIN_REQUIRED),
        cellular_property_ui_data);

    chromeos::MobileConfig* config = chromeos::MobileConfig::GetInstance();
    if (config->IsReady()) {
      const std::string& carrier_id = cros_->GetCellularHomeCarrierId();
      const chromeos::MobileConfig::Carrier* carrier =
          config->GetCarrier(carrier_id);
      if (carrier && !carrier->top_up_url().empty())
        dictionary->SetString(kTagCarrierUrl, carrier->top_up_url());
    }

    const chromeos::CellularApnList& apn_list = device->provider_apn_list();
    ListValue* apn_list_value = new ListValue();
    for (chromeos::CellularApnList::const_iterator it = apn_list.begin();
         it != apn_list.end(); ++it) {
      apn_list_value->Append(CreateDictionaryFromCellularApn(*it));
    }
    SetValueDictionary(dictionary, kTagProviderApnList, apn_list_value,
                       cellular_property_ui_data);

    if (CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kEnableCarrierSwitching)) {
      base::ListValue* supported_carriers = device->supported_carriers();
      if (supported_carriers) {
        dictionary->Set(kTagCarriers, supported_carriers->DeepCopy());
        dictionary->SetInteger(kTagCurrentCarrierIndex,
                               FindCurrentCarrierIndex(supported_carriers,
                                                       device));
      } else {
        // In case of any error, set the current carrier tag to -1 indicating
        // to the JS code to fallback to a single carrier.
        dictionary->SetInteger(kTagCurrentCarrierIndex, -1);
      }
    }
  }

  SetCellularButtonsVisibility(cellular,
                                dictionary,
                                cros_->GetCellularHomeCarrierId());
}

void InternetOptionsHandler::SetCellularButtonsVisibility(
    const chromeos::CellularNetwork* cellular,
    DictionaryValue* dictionary,
    const std::string& carrier_id) {
  dictionary->SetBoolean(
      kTagDisableConnectButton,
      cellular->activation_state() == chromeos::ACTIVATION_STATE_ACTIVATING ||
          cellular->connecting());

  if (cellular->activation_state() != chromeos::ACTIVATION_STATE_ACTIVATING &&
      cellular->activation_state() != chromeos::ACTIVATION_STATE_ACTIVATED) {
    dictionary->SetBoolean(kTagShowActivateButton, true);
  } else {
    const chromeos::MobileConfig::Carrier* carrier =
        chromeos::MobileConfig::GetInstance()->GetCarrier(carrier_id);
    if (carrier && carrier->show_portal_button()) {
      // This will trigger BuyDataPlanCallback() so that
      // chrome://mobilesetup/ will open carrier specific portal.
      dictionary->SetBoolean(kTagShowViewAccountButton, true);
    }
  }
}

gfx::NativeWindow InternetOptionsHandler::GetNativeWindow() const {
  // TODO(beng): This is an improper direct dependency on Browser. Route this
  // through some sort of delegate.
  Browser* browser =
      chrome::FindBrowserWithWebContents(web_ui()->GetWebContents());
  return browser->window()->GetNativeWindow();
}

Browser* InternetOptionsHandler::GetAppropriateBrowser() {
  return chrome::FindOrCreateTabbedBrowser(
      ProfileManager::GetDefaultProfileOrOffTheRecord(),
      chrome::HOST_DESKTOP_TYPE_ASH);
}

void InternetOptionsHandler::NetworkCommandCallback(const ListValue* args) {
  std::string str_type;
  std::string service_path;
  std::string command;
  if (args->GetSize() != 3 ||
      !args->GetString(0, &str_type) ||
      !args->GetString(1, &service_path) ||
      !args->GetString(2, &command)) {
    NOTREACHED();
    return;
  }

  chromeos::ConnectionType type =
      (chromeos::ConnectionType) atoi(str_type.c_str());

  // Process commands that do not require an existing network.
  if (command == kTagAddConnection) {
    if (CanAddNetworkType(type))
      AddConnection(type);
    return;
  } else if (command == kTagForget) {
    if (CanForgetNetworkType(type))
      cros_->ForgetNetwork(service_path);
    return;
  }

  // Process commands that require an active network.
  chromeos::Network *network = NULL;
  if (!service_path.empty())
    network = cros_->FindNetworkByPath(service_path);

  if (!network) {
    VLOG(2) << "Network command: " << command
            << "Called with unknown service-path: " << service_path;
    return;
  }
  DCHECK_EQ(network->type(), type)
      << "Provided type: " << type << " does not match: " << network->type()
      << " For network: " << service_path;

  if (command == kTagOptions) {
    cros_->RequestNetworkServiceProperties(
        service_path,
        base::Bind(&InternetOptionsHandler::PopulateDictionaryDetailsCallback,
                   weak_factory_.GetWeakPtr()));
  } else if (command == kTagConnect) {
    ConnectToNetwork(network);
  } else if (command == kTagDisconnect && type != chromeos::TYPE_ETHERNET) {
    cros_->DisconnectFromNetwork(network);
  } else if (command == kTagActivate && type == chromeos::TYPE_CELLULAR) {
    if (!UseDirectActivation()) {
      ash::Shell::GetInstance()->delegate()->OpenMobileSetup(
          network->service_path());
    } else {
      Activate(service_path);
      // Update network properties after we start activation. The Activate
      // call is a blocking call, which blocks on finishing the "start" of
      // the activation, hence when we query for network properties after
      // this call is done, we will have the "activating" activation state.
      UpdateConnectionData(network);
    }
  } else {
    VLOG(1) << "Unknown command: " << command;
    NOTREACHED();
  }
}

void InternetOptionsHandler::ToggleAirplaneModeCallback(const ListValue* args) {
  // TODO(kevers): The use of 'offline_mode' is not quite correct.  Update once
  // we have proper back-end support.
  cros_->EnableOfflineMode(!cros_->offline_mode());
}

void InternetOptionsHandler::AddConnection(chromeos::ConnectionType type) {
  switch (type) {
    case chromeos::TYPE_WIFI:
    case chromeos::TYPE_VPN:
      chromeos::NetworkConfigView::ShowForType(type,
                                               GetNativeWindow());
      break;
    case chromeos::TYPE_CELLULAR:
      chromeos::ChooseMobileNetworkDialog::ShowDialog(GetNativeWindow());
      break;
    default:
      NOTREACHED();
  }
}

void InternetOptionsHandler::ConnectToNetwork(chromeos::Network* network) {
  if (network->type() == chromeos::TYPE_CELLULAR) {
    cros_->ConnectToCellularNetwork(
        static_cast<chromeos::CellularNetwork*>(network));
  } else {
    network->SetEnrollmentDelegate(
        chromeos::CreateEnrollmentDelegate(
            GetNativeWindow(),
            network->name(),
            ProfileManager::GetLastUsedProfile()));
    network->AttemptConnection(base::Bind(&InternetOptionsHandler::DoConnect,
                                          weak_factory_.GetWeakPtr(),
                                          network));
  }
}

void InternetOptionsHandler::DoConnect(chromeos::Network* network) {
  if (network->type() == chromeos::TYPE_VPN) {
    chromeos::VirtualNetwork* vpn =
        static_cast<chromeos::VirtualNetwork*>(network);
    if (vpn->NeedMoreInfoToConnect()) {
      chromeos::NetworkConfigView::Show(network, GetNativeWindow());
    } else {
      cros_->ConnectToVirtualNetwork(vpn);
      // Connection failures are responsible for updating the UI, including
      // reopening dialogs.
    }
  } else if (network->type() == chromeos::TYPE_WIFI) {
    chromeos::WifiNetwork* wifi = static_cast<chromeos::WifiNetwork*>(network);
    if (wifi->IsPassphraseRequired()) {
      // Show the connection UI if we require a passphrase.
      chromeos::NetworkConfigView::Show(wifi, GetNativeWindow());
    } else {
      cros_->ConnectToWifiNetwork(wifi);
      // Connection failures are responsible for updating the UI, including
      // reopening dialogs.
    }
  } else if (network->type() == chromeos::TYPE_WIMAX) {
    chromeos::WimaxNetwork* wimax =
        static_cast<chromeos::WimaxNetwork*>(network);
    if (wimax->passphrase_required()) {
      // Show the connection UI if we require a passphrase.
      // TODO(stevenjb): Implement WiMAX connection UI.
      chromeos::NetworkConfigView::Show(wimax, GetNativeWindow());
    } else {
      cros_->ConnectToWimaxNetwork(wimax);
      // Connection failures are responsible for updating the UI, including
      // reopening dialogs.
    }
  }
}

ListValue* InternetOptionsHandler::GetWiredList() {
  ListValue* list = new ListValue();

  // If ethernet is not enabled, then don't add anything.
  if (cros_->ethernet_enabled()) {
    const chromeos::EthernetNetwork* ethernet_network =
        cros_->ethernet_network();
    if (ethernet_network) {
      NetworkInfoDictionary network_dict(ethernet_network,
                                         web_ui()->GetDeviceScaleFactor());
      network_dict.set_name(
          l10n_util::GetStringUTF8(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET)),
      list->Append(network_dict.BuildDictionary());
    }
  }
  return list;
}

ListValue* InternetOptionsHandler::GetWirelessList() {
  ListValue* list = new ListValue();

  const chromeos::WifiNetworkVector& wifi_networks = cros_->wifi_networks();
  for (chromeos::WifiNetworkVector::const_iterator it =
      wifi_networks.begin(); it != wifi_networks.end(); ++it) {
    NetworkInfoDictionary network_dict(*it, web_ui()->GetDeviceScaleFactor());
    network_dict.set_connectable(cros_->CanConnectToNetwork(*it));
    list->Append(network_dict.BuildDictionary());
  }

  const chromeos::WimaxNetworkVector& wimax_networks = cros_->wimax_networks();
  for (chromeos::WimaxNetworkVector::const_iterator it =
      wimax_networks.begin(); it != wimax_networks.end(); ++it) {
    NetworkInfoDictionary network_dict(*it, web_ui()->GetDeviceScaleFactor());
    network_dict.set_connectable(cros_->CanConnectToNetwork(*it));
    list->Append(network_dict.BuildDictionary());
  }

  const chromeos::CellularNetworkVector cellular_networks =
      cros_->cellular_networks();
  for (chromeos::CellularNetworkVector::const_iterator it =
      cellular_networks.begin(); it != cellular_networks.end(); ++it) {
    NetworkInfoDictionary network_dict(*it, web_ui()->GetDeviceScaleFactor());
    network_dict.set_connectable(cros_->CanConnectToNetwork(*it));
    network_dict.set_activation_state((*it)->activation_state());
    list->Append(network_dict.BuildDictionary());
  }

  return list;
}

ListValue* InternetOptionsHandler::GetVPNList() {
  ListValue* list = new ListValue();

  const chromeos::VirtualNetworkVector& virtual_networks =
      cros_->virtual_networks();
  for (chromeos::VirtualNetworkVector::const_iterator it =
      virtual_networks.begin(); it != virtual_networks.end(); ++it) {
    NetworkInfoDictionary network_dict(*it, web_ui()->GetDeviceScaleFactor());
    network_dict.set_connectable(cros_->CanConnectToNetwork(*it));
    list->Append(network_dict.BuildDictionary());
  }

  return list;
}

ListValue* InternetOptionsHandler::GetRememberedList() {
  ListValue* list = new ListValue();

  for (chromeos::WifiNetworkVector::const_iterator rit =
           cros_->remembered_wifi_networks().begin();
       rit != cros_->remembered_wifi_networks().end(); ++rit) {
    chromeos::WifiNetwork* remembered = *rit;
    chromeos::WifiNetwork* wifi = static_cast<chromeos::WifiNetwork*>(
        cros_->FindNetworkByUniqueId(remembered->unique_id()));

    NetworkInfoDictionary network_dict(wifi,
                                       remembered,
                                       web_ui()->GetDeviceScaleFactor());
    list->Append(network_dict.BuildDictionary());
  }

  for (chromeos::VirtualNetworkVector::const_iterator rit =
           cros_->remembered_virtual_networks().begin();
       rit != cros_->remembered_virtual_networks().end(); ++rit) {
    chromeos::VirtualNetwork* remembered = *rit;
    chromeos::VirtualNetwork* vpn = static_cast<chromeos::VirtualNetwork*>(
        cros_->FindNetworkByUniqueId(remembered->unique_id()));

    NetworkInfoDictionary network_dict(vpn,
                                       remembered,
                                       web_ui()->GetDeviceScaleFactor());
    list->Append(network_dict.BuildDictionary());
  }

  return list;
}

void InternetOptionsHandler::FillNetworkInfo(DictionaryValue* dictionary) {
  dictionary->SetBoolean(kTagAccessLocked, cros_->IsLocked());
  dictionary->Set(kTagWiredList, GetWiredList());
  dictionary->Set(kTagWirelessList, GetWirelessList());
  dictionary->Set(kTagVpnList, GetVPNList());
  dictionary->Set(kTagRememberedList, GetRememberedList());
  dictionary->SetBoolean(kTagWifiAvailable, cros_->wifi_available());
  dictionary->SetBoolean(kTagWifiBusy, cros_->wifi_busy());
  dictionary->SetBoolean(kTagWifiEnabled, cros_->wifi_enabled());
  dictionary->SetBoolean(kTagCellularAvailable, cros_->cellular_available());
  dictionary->SetBoolean(kTagCellularBusy, cros_->cellular_busy());
  dictionary->SetBoolean(kTagCellularEnabled, cros_->cellular_enabled());

  const chromeos::NetworkDevice* cellular_device = cros_->FindCellularDevice();
  dictionary->SetBoolean(
      kTagCellularSupportsScan,
      cellular_device && cellular_device->support_network_scan());

  dictionary->SetBoolean(kTagWimaxEnabled, cros_->wimax_enabled());
  dictionary->SetBoolean(kTagWimaxAvailable, cros_->wimax_available());
  dictionary->SetBoolean(kTagWimaxBusy, cros_->wimax_busy());
  // TODO(kevers): The use of 'offline_mode' is not quite correct.  Update once
  // we have proper back-end support.
  dictionary->SetBoolean(kTagAirplaneMode, cros_->offline_mode());
}

}  // namespace options
