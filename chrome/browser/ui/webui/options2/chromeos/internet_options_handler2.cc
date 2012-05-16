// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options2/chromeos/internet_options_handler2.h"

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
#include "chrome/browser/chromeos/cros/onc_constants.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/chromeos/enrollment_dialog_view.h"
#include "chrome/browser/chromeos/mobile_config.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/browser/chromeos/proxy_config_service_impl.h"
#include "chrome/browser/chromeos/sim_dialog_delegate.h"
#include "chrome/browser/chromeos/status/network_menu_icon.h"
#include "chrome/browser/net/pref_proxy_config_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/web_ui_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/time_format.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_ui.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/views/widget/widget.h"

namespace {

static const char kOtherNetworksFakePath[] = "?";

// Keys for the network description dictionary passed to the web ui. Make sure
// to keep the strings in sync with what the Javascript side uses.
const char kNetworkInfoKeyActivationState[] = "activation_state";
const char kNetworkInfoKeyConnectable[] = "connectable";
const char kNetworkInfoKeyConnected[] = "connected";
const char kNetworkInfoKeyConnecting[] = "connecting";
const char kNetworkInfoKeyIconURL[] = "iconURL";
const char kNetworkInfoKeyNeedsNewPlan[] = "needs_new_plan";
const char kNetworkInfoKeyNetworkName[] = "networkName";
const char kNetworkInfoKeyNetworkStatus[] = "networkStatus";
const char kNetworkInfoKeyNetworkType[] = "networkType";
const char kNetworkInfoKeyRemembered[] = "remembered";
const char kNetworkInfoKeyServicePath[] = "servicePath";
const char kNetworkInfoKeyPolicyManaged[] = "policyManaged";

// A helper class for building network information dictionaries to be sent to
// the webui code.
class NetworkInfoDictionary {
 public:
  // Initializes the dictionary with default values.
  NetworkInfoDictionary();

  // Copies in service path, connect{ing|ed|able} flags and connection type from
  // the provided network object. Also chooses an appropriate icon based on the
  // network type.
  explicit NetworkInfoDictionary(const chromeos::Network* network);

  // Initializes a remembered network entry, pulling information from the passed
  // network object and the corresponding remembered network object. |network|
  // may be NULL.
  NetworkInfoDictionary(const chromeos::Network* network,
                        const chromeos::Network* remembered);

  // Setters for filling in information.
  void set_service_path(const std::string& service_path) {
    service_path_ = service_path;
  }
  void set_icon(const SkBitmap& icon) {
    icon_url_ = icon.isNull() ? "" : web_ui_util::GetImageDataUrl(icon);
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
  void set_needs_new_plan(bool needs_new_plan) {
    needs_new_plan_ = needs_new_plan;
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
  bool needs_new_plan_;
  bool policy_managed_;

  DISALLOW_COPY_AND_ASSIGN(NetworkInfoDictionary);
};

NetworkInfoDictionary::NetworkInfoDictionary() {
  set_connecting(false);
  set_connected(false);
  set_connectable(false);
  set_remembered(false);
  set_shared(false);
  set_activation_state(chromeos::ACTIVATION_STATE_UNKNOWN);
  set_needs_new_plan(false);
  set_policy_managed(false);
}

NetworkInfoDictionary::NetworkInfoDictionary(const chromeos::Network* network) {
  set_service_path(network->service_path());
  set_icon(chromeos::NetworkMenuIcon::GetBitmap(network,
      chromeos::NetworkMenuIcon::COLOR_DARK));
  set_name(network->name());
  set_connecting(network->connecting());
  set_connected(network->connected());
  set_connectable(network->connectable());
  set_connection_type(network->type());
  set_remembered(false);
  set_shared(false);
  set_needs_new_plan(false);
  set_policy_managed(network->ui_data().is_managed());
}

NetworkInfoDictionary::NetworkInfoDictionary(
    const chromeos::Network* network,
    const chromeos::Network* remembered) {
  set_service_path(remembered->service_path());
  set_icon(chromeos::NetworkMenuIcon::GetBitmap(
      network ? network : remembered, chromeos::NetworkMenuIcon::COLOR_DARK));
  set_name(remembered->name());
  set_connecting(network ? network->connecting() : false);
  set_connected(network ? network->connected() : false);
  set_connectable(true);
  set_connection_type(remembered->type());
  set_remembered(true);
  set_shared(remembered->profile_type() == chromeos::PROFILE_SHARED);
  set_needs_new_plan(false);
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
      if (needs_new_plan_) {
        status = l10n_util::GetStringUTF8(IDS_OPTIONS_SETTINGS_NO_PLAN_LABEL);
      } else if (activation_state_ != chromeos::ACTIVATION_STATE_ACTIVATED) {
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
  network_info->SetBoolean(kNetworkInfoKeyNeedsNewPlan, needs_new_plan_);
  network_info->SetString(kNetworkInfoKeyNetworkName, name_);
  network_info->SetString(kNetworkInfoKeyNetworkStatus, status);
  network_info->SetInteger(kNetworkInfoKeyNetworkType,
                           static_cast<int>(connection_type_));
  network_info->SetBoolean(kNetworkInfoKeyRemembered, remembered_);
  network_info->SetString(kNetworkInfoKeyServicePath, service_path_);
  network_info->SetBoolean(kNetworkInfoKeyPolicyManaged, policy_managed_);
  return network_info.release();
}

}  // namespace

namespace options2 {

InternetOptionsHandler::InternetOptionsHandler()
  : ALLOW_THIS_IN_INITIALIZER_LIST(weak_factory_(this)) {
  registrar_.Add(this, chrome::NOTIFICATION_REQUIRE_PIN_SETTING_CHANGE_ENDED,
      content::NotificationService::AllSources());
  registrar_.Add(this, chrome::NOTIFICATION_ENTER_PIN_ENDED,
      content::NotificationService::AllSources());
  cros_ = chromeos::CrosLibrary::Get()->GetNetworkLibrary();
  if (cros_) {
    cros_->AddNetworkManagerObserver(this);
    cros_->AddCellularDataPlanObserver(this);
    MonitorNetworks();
  }
}

InternetOptionsHandler::~InternetOptionsHandler() {
  if (cros_) {
    cros_->RemoveNetworkManagerObserver(this);
    cros_->RemoveCellularDataPlanObserver(this);
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
    { "cellularPlanTabLabel", IDS_OPTIONS_SETTINGS_INTERNET_TAB_PLAN },
    { "cellularConnTabLabel", IDS_OPTIONS_SETTINGS_INTERNET_TAB_CONNECTION },
    { "cellularDeviceTabLabel", IDS_OPTIONS_SETTINGS_INTERNET_TAB_DEVICE },
    { "networkTabLabel", IDS_OPTIONS_SETTINGS_INTERNET_TAB_NETWORK },
    { "securityTabLabel", IDS_OPTIONS_SETTINGS_INTERNET_TAB_SECURITY },
    { "proxyTabLabel", IDS_OPTIONS_SETTINGS_INTERNET_TAB_PROXY },
    { "useDHCP", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_USE_DHCP },
    { "useStaticIP", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_USE_STATIC_IP },
    { "connectionState", IDS_OPTIONS_SETTINGS_INTERNET_CONNECTION_STATE },
    { "inetAddress", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_ADDRESS },
    { "inetSubnetAddress", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SUBNETMASK },
    { "inetGateway", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_GATEWAY },
    { "inetDns", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_DNSSERVER },
    { "hardwareAddress",
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_HARDWARE_ADDRESS },
    { "detailsInternetDismiss", IDS_CLOSE },
    { "activateButton", IDS_OPTIONS_SETTINGS_ACTIVATE },
    { "buyplanButton", IDS_OPTIONS_SETTINGS_BUY_PLAN },
    { "connectButton", IDS_OPTIONS_SETTINGS_CONNECT },
    { "disconnectButton", IDS_OPTIONS_SETTINGS_DISCONNECT },
    { "viewAccountButton", IDS_STATUSBAR_NETWORK_VIEW_ACCOUNT },

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

    // Plan Tab.

    { "planName", IDS_OPTIONS_SETTINGS_INTERNET_CELL_PLAN_NAME },
    { "planLoading", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_LOADING_PLAN },
    { "noPlansFound", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_NO_PLANS_FOUND },
    { "purchaseMore", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PURCHASE_MORE },
    { "dataRemaining", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_DATA_REMAINING },
    { "planExpires", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_EXPIRES },
    { "showPlanNotifications",
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SHOW_MOBILE_NOTIFICATION },
    { "autoconnectCellular",
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_AUTO_CONNECT }
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
  cros_->RequestNetworkScan();
}

void InternetOptionsHandler::RegisterMessages() {
  // Setup handlers specific to this panel.
  web_ui()->RegisterMessageCallback("networkCommand",
      base::Bind(&InternetOptionsHandler::NetworkCommandCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("refreshNetworks",
      base::Bind(&InternetOptionsHandler::RefreshNetworksCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("refreshCellularPlan",
      base::Bind(&InternetOptionsHandler::RefreshCellularPlanCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("setPreferNetwork",
      base::Bind(&InternetOptionsHandler::SetPreferNetworkCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("setAutoConnect",
      base::Bind(&InternetOptionsHandler::SetAutoConnectCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("setIPConfig",
      base::Bind(&InternetOptionsHandler::SetIPConfigCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("enableWifi",
      base::Bind(&InternetOptionsHandler::EnableWifiCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("disableWifi",
      base::Bind(&InternetOptionsHandler::DisableWifiCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("enableCellular",
      base::Bind(&InternetOptionsHandler::EnableCellularCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("disableCellular",
      base::Bind(&InternetOptionsHandler::DisableCellularCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("buyDataPlan",
      base::Bind(&InternetOptionsHandler::BuyDataPlanCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("showMorePlanInfo",
      base::Bind(&InternetOptionsHandler::ShowMorePlanInfoCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("setApn",
      base::Bind(&InternetOptionsHandler::SetApnCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("setSimCardLock",
      base::Bind(&InternetOptionsHandler::SetSimCardLockCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("changePin",
      base::Bind(&InternetOptionsHandler::ChangePinCallback,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("toggleAirplaneMode",
      base::Bind(&InternetOptionsHandler::ToggleAirplaneModeCallback,
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
  const chromeos::NetworkDevice* cellular = cros_->FindCellularDevice();
  if (!cellular) {
    LOG(ERROR) << "Didn't find cellular device, it should have been available.";
    cros_->EnableCellularNetworkDevice(true);
  } else if (!cellular->is_sim_locked()) {
    if (cellular->is_sim_absent()) {
      std::string setup_url;
      chromeos::MobileConfig* config = chromeos::MobileConfig::GetInstance();
      if (config->IsReady()) {
        const chromeos::MobileConfig::LocaleConfig* locale_config =
            config->GetLocaleConfig();
        if (locale_config)
          setup_url = locale_config->setup_url();
      }
      if (!setup_url.empty()) {
        GetAppropriateBrowser()->ShowSingletonTab(GURL(setup_url));
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

void InternetOptionsHandler::ShowMorePlanInfoCallback(const ListValue* args) {
  if (!web_ui())
    return;
  Browser* browser = BrowserList::FindBrowserWithFeature(
      Profile::FromWebUI(web_ui()), Browser::FEATURE_TABSTRIP);
  if (!browser)
    return;

  const chromeos::CellularNetwork* cellular = cros_->cellular_network();
  if (!cellular)
    return;

  browser->OpenURL(content::OpenURLParams(
      cellular->GetAccountInfoUrl(), content::Referrer(),
      NEW_FOREGROUND_TAB,
      content::PAGE_TRANSITION_LINK, false));
}

void InternetOptionsHandler::BuyDataPlanCallback(const ListValue* args) {
  if (!web_ui())
    return;
  ash::Shell::GetInstance()->delegate()->OpenMobileSetup();
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

void InternetOptionsHandler::RefreshNetworkData() {
  DictionaryValue dictionary;
  FillNetworkInfo(&dictionary);
  web_ui()->CallJavascriptFunction(
      "options.network.NetworkList.refreshNetworkData", dictionary);
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
  if (web_ui())
    RefreshNetworkData();
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
  // Always monitor the cellular networks, if any, so that changes
  // in network technology, roaming status, and signal strength
  // will be shown.
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

void InternetOptionsHandler::OnCellularDataPlanChanged(
    chromeos::NetworkLibrary* cros) {
  if (!web_ui())
    return;
  const chromeos::CellularNetwork* cellular = cros_->cellular_network();
  if (!cellular)
    return;
  const chromeos::CellularDataPlanVector* plans =
      cros_->GetDataPlans(cellular->service_path());
  DictionaryValue connection_plans;
  ListValue* plan_list = new ListValue();
  if (plans) {
    for (chromeos::CellularDataPlanVector::const_iterator iter = plans->begin();
         iter != plans->end(); ++iter) {
      plan_list->Append(CellularDataPlanToDictionary(*iter));
    }
  }
  connection_plans.SetString("servicePath", cellular->service_path());
  connection_plans.SetBoolean("needsPlan", cellular->needs_new_plan());
  connection_plans.SetBoolean("activated",
      cellular->activation_state() == chromeos::ACTIVATION_STATE_ACTIVATED);
  connection_plans.Set("plans", plan_list);
  SetActivationButtonVisibility(cellular,
                                &connection_plans,
                                cros_->GetCellularHomeCarrierId());
  web_ui()->CallJavascriptFunction(
      "options.internet.DetailsInternetPage.updateCellularPlans",
      connection_plans);
}


void InternetOptionsHandler::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  OptionsPageUIHandler::Observe(type, source, details);
  if (type == chrome::NOTIFICATION_REQUIRE_PIN_SETTING_CHANGE_ENDED) {
    base::FundamentalValue require_pin(*content::Details<bool>(details).ptr());
    web_ui()->CallJavascriptFunction(
        "options.internet.DetailsInternetPage.updateSecurityTab", require_pin);
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

DictionaryValue* InternetOptionsHandler::CellularDataPlanToDictionary(
    const chromeos::CellularDataPlan* plan) {
  DictionaryValue* plan_dict = new DictionaryValue();
  plan_dict->SetInteger("planType", plan->plan_type);
  plan_dict->SetString("name", plan->plan_name);
  plan_dict->SetString("planSummary", plan->GetPlanDesciption());
  plan_dict->SetString("dataRemaining", plan->GetDataRemainingDesciption());
  plan_dict->SetString("planExpires", plan->GetPlanExpiration());
  plan_dict->SetString("warning", plan->GetRemainingWarning());
  return plan_dict;
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

  bool prefer_network = prefer_network_str == "true";
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

  bool auto_connect = auto_connect_str == "true";
  if (auto_connect != network->auto_connect())
    network->SetAutoConnect(auto_connect);
}

void InternetOptionsHandler::SetIPConfigCallback(const ListValue* args) {
  std::string service_path;
  std::string dhcp_str;
  std::string address;
  std::string netmask;
  std::string gateway;
  std::string name_servers;

  if (args->GetSize() < 6 ||
      !args->GetString(0, &service_path) ||
      !args->GetString(1, &dhcp_str) ||
      !args->GetString(2, &address) ||
      !args->GetString(3, &netmask) ||
      !args->GetString(4, &gateway) ||
      !args->GetString(5, &name_servers)) {
    NOTREACHED();
    return;
  }

  chromeos::Network* network = cros_->FindNetworkByPath(service_path);
  if (!network)
    return;

  cros_->SetIPConfig(chromeos::NetworkIPConfig(network->device_path(),
      dhcp_str == "true" ? chromeos::IPCONFIG_TYPE_DHCP :
                           chromeos::IPCONFIG_TYPE_IPV4,
      address, netmask, gateway, name_servers));
}

void InternetOptionsHandler::PopulateDictionaryDetails(
    const chromeos::Network* network) {
  DCHECK(network);

  if (web_ui()) {
    Profile::FromWebUI(web_ui())->GetProxyConfigTracker()->UISetCurrentNetwork(
        network->service_path());
  }

  const chromeos::NetworkUIData& ui_data = network->ui_data();
  const base::DictionaryValue* onc =
      cros_->FindOncForNetwork(network->unique_id());

  DictionaryValue dictionary;
  std::string hardware_address;
  chromeos::NetworkIPConfigVector ipconfigs = cros_->GetIPConfigs(
      network->device_path(), &hardware_address,
      chromeos::NetworkLibrary::FORMAT_COLON_SEPARATED_HEX);
  if (!hardware_address.empty())
    dictionary.SetString("hardwareAddress", hardware_address);

  scoped_ptr<DictionaryValue> ipconfig_dhcp;
  scoped_ptr<DictionaryValue> ipconfig_static;
  for (chromeos::NetworkIPConfigVector::const_iterator it = ipconfigs.begin();
       it != ipconfigs.end(); ++it) {
    const chromeos::NetworkIPConfig& ipconfig = *it;
    scoped_ptr<DictionaryValue> ipconfig_dict(new DictionaryValue());
    ipconfig_dict->SetString("address", ipconfig.address);
    ipconfig_dict->SetString("subnetAddress", ipconfig.netmask);
    ipconfig_dict->SetString("gateway", ipconfig.gateway);
    ipconfig_dict->SetString("dns", ipconfig.name_servers);
    if (ipconfig.type == chromeos::IPCONFIG_TYPE_DHCP)
      ipconfig_dhcp.reset(ipconfig_dict.release());
    else if (ipconfig.type == chromeos::IPCONFIG_TYPE_IPV4)
      ipconfig_static.reset(ipconfig_dict.release());
  }

  chromeos::NetworkPropertyUIData ipconfig_dhcp_ui_data(ui_data);
  SetValueDictionary(&dictionary, "ipconfigDHCP", ipconfig_dhcp.release(),
                     ipconfig_dhcp_ui_data);
  chromeos::NetworkPropertyUIData ipconfig_static_ui_data(ui_data);
  SetValueDictionary(&dictionary, "ipconfigStatic", ipconfig_static.release(),
                     ipconfig_static_ui_data);

  chromeos::ConnectionType type = network->type();
  dictionary.SetInteger("type", type);
  dictionary.SetString("servicePath", network->service_path());
  dictionary.SetBoolean("connecting", network->connecting());
  dictionary.SetBoolean("connected", network->connected());
  dictionary.SetString("connectionState", network->GetStateString());
  dictionary.SetString("networkName", network->name());

  // Only show proxy for remembered networks.
  chromeos::NetworkProfileType network_profile = network->profile_type();
  dictionary.SetBoolean("showProxy", network_profile != chromeos::PROFILE_NONE);

  // Enable static ip config for ethernet. For wifi, enable if flag is set.
  bool staticIPConfig = type == chromeos::TYPE_ETHERNET ||
      (type == chromeos::TYPE_WIFI &&
       CommandLine::ForCurrentProcess()->HasSwitch(
           switches::kEnableStaticIPConfig));
  dictionary.SetBoolean("showStaticIPConfig", staticIPConfig);

  chromeos::NetworkPropertyUIData preferred_ui_data(ui_data);
  if (network_profile == chromeos::PROFILE_USER) {
    dictionary.SetBoolean("showPreferred", true);
    SetValueDictionary(&dictionary, "preferred",
                       Value::CreateBooleanValue(network->preferred()),
                       preferred_ui_data);
  } else {
    dictionary.SetBoolean("showPreferred", false);
    SetValueDictionary(&dictionary, "preferred",
                       Value::CreateBooleanValue(network->preferred()),
                       preferred_ui_data);
  }
  chromeos::NetworkPropertyUIData auto_connect_ui_data(ui_data);
  if (type == chromeos::TYPE_WIFI)
    auto_connect_ui_data.ParseOncProperty(
        ui_data, onc,
        base::StringPrintf("%s.%s",
                           chromeos::onc::kWiFi,
                           chromeos::onc::wifi::kAutoConnect));
  SetValueDictionary(&dictionary, "autoConnect",
                     Value::CreateBooleanValue(network->auto_connect()),
                     auto_connect_ui_data);

  if (type == chromeos::TYPE_WIFI) {
    dictionary.SetBoolean("deviceConnected", cros_->wifi_connected());
    const chromeos::WifiNetwork* wifi =
        cros_->FindWifiNetworkByPath(network->service_path());
    if (!wifi) {
      LOG(WARNING) << "Cannot find network " << network->service_path();
    } else {
      PopulateWifiDetails(wifi, &dictionary);
    }
  } else if (type == chromeos::TYPE_CELLULAR) {
    dictionary.SetBoolean("deviceConnected", cros_->cellular_connected());
    const chromeos::CellularNetwork* cellular =
        cros_->FindCellularNetworkByPath(network->service_path());
    if (!cellular) {
      LOG(WARNING) << "Cannot find network " << network->service_path();
    } else {
      PopulateCellularDetails(cellular, &dictionary);
    }
  } else if (type == chromeos::TYPE_VPN) {
    dictionary.SetBoolean("deviceConnected",
                          cros_->virtual_network_connected());
    const chromeos::VirtualNetwork* vpn =
        cros_->FindVirtualNetworkByPath(network->service_path());
    if (!vpn) {
      LOG(WARNING) << "Cannot find network " << network->service_path();
    } else {
      PopulateVPNDetails(vpn, &dictionary);
    }
  } else if (type == chromeos::TYPE_ETHERNET) {
    dictionary.SetBoolean("deviceConnected", cros_->ethernet_connected());
  }

  web_ui()->CallJavascriptFunction(
      "options.internet.DetailsInternetPage.showDetailedInfo", dictionary);
}

void InternetOptionsHandler::PopulateWifiDetails(
    const chromeos::WifiNetwork* wifi,
    DictionaryValue* dictionary) {
  dictionary->SetString("ssid", wifi->name());
  bool remembered = (wifi->profile_type() != chromeos::PROFILE_NONE);
  dictionary->SetBoolean("remembered", remembered);
  bool shared = wifi->profile_type() == chromeos::PROFILE_SHARED;
  dictionary->SetBoolean("shared", shared);
  dictionary->SetString("encryption", wifi->GetEncryptionString());
  dictionary->SetString("bssid", wifi->bssid());
  dictionary->SetInteger("frequency", wifi->frequency());
  dictionary->SetInteger("strength", wifi->strength());
}

DictionaryValue* InternetOptionsHandler::CreateDictionaryFromCellularApn(
    const chromeos::CellularApn& apn) {
  DictionaryValue* dictionary = new DictionaryValue();
  dictionary->SetString("apn", apn.apn);
  dictionary->SetString("networkId", apn.network_id);
  dictionary->SetString("username", apn.username);
  dictionary->SetString("password", apn.password);
  dictionary->SetString("name", apn.name);
  dictionary->SetString("localizedName", apn.localized_name);
  dictionary->SetString("language", apn.language);
  return dictionary;
}

void InternetOptionsHandler::PopulateCellularDetails(
    const chromeos::CellularNetwork* cellular,
    DictionaryValue* dictionary) {
  // Cellular network / connection settings.
  dictionary->SetString("serviceName", cellular->name());
  dictionary->SetString("networkTechnology",
                        cellular->GetNetworkTechnologyString());
  dictionary->SetString("operatorName", cellular->operator_name());
  dictionary->SetString("operatorCode", cellular->operator_code());
  dictionary->SetString("activationState",
                        cellular->GetActivationStateString());
  dictionary->SetString("roamingState",
                        cellular->GetRoamingStateString());
  dictionary->SetString("restrictedPool",
                        cellular->restricted_pool() ?
                        l10n_util::GetStringUTF8(
                            IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL) :
                        l10n_util::GetStringUTF8(
                            IDS_CONFIRM_MESSAGEBOX_NO_BUTTON_LABEL));
  dictionary->SetString("errorState", cellular->GetErrorString());
  dictionary->SetString("supportUrl", cellular->payment_url());
  dictionary->SetBoolean("needsPlan", cellular->needs_new_plan());

  dictionary->Set("apn", CreateDictionaryFromCellularApn(cellular->apn()));
  dictionary->Set("lastGoodApn",
                  CreateDictionaryFromCellularApn(cellular->last_good_apn()));

  // Device settings.
  const chromeos::NetworkDevice* device =
      cros_->FindNetworkDeviceByPath(cellular->device_path());
  if (device) {
    chromeos::NetworkPropertyUIData cellular_propety_ui_data(
        cellular->ui_data());
    dictionary->SetString("manufacturer", device->manufacturer());
    dictionary->SetString("modelId", device->model_id());
    dictionary->SetString("firmwareRevision", device->firmware_revision());
    dictionary->SetString("hardwareRevision", device->hardware_revision());
    dictionary->SetString("prlVersion",
                          base::StringPrintf("%u", device->prl_version()));
    dictionary->SetString("meid", device->meid());
    dictionary->SetString("imei", device->imei());
    dictionary->SetString("mdn", device->mdn());
    dictionary->SetString("imsi", device->imsi());
    dictionary->SetString("esn", device->esn());
    dictionary->SetString("min", device->min());
    dictionary->SetBoolean("gsm",
        device->technology_family() == chromeos::TECHNOLOGY_FAMILY_GSM);
    SetValueDictionary(
        dictionary, "simCardLockEnabled",
        Value::CreateBooleanValue(
            device->sim_pin_required() == chromeos::SIM_PIN_REQUIRED),
        cellular_propety_ui_data);

    chromeos::MobileConfig* config = chromeos::MobileConfig::GetInstance();
    if (config->IsReady()) {
      const std::string& carrier_id = cros_->GetCellularHomeCarrierId();
      const chromeos::MobileConfig::Carrier* carrier =
          config->GetCarrier(carrier_id);
      if (carrier && !carrier->top_up_url().empty())
        dictionary->SetString("carrierUrl", carrier->top_up_url());
    }

    const chromeos::CellularApnList& apn_list = device->provider_apn_list();
    ListValue* apn_list_value = new ListValue();
    for (chromeos::CellularApnList::const_iterator it = apn_list.begin();
         it != apn_list.end(); ++it) {
      apn_list_value->Append(CreateDictionaryFromCellularApn(*it));
    }
    SetValueDictionary(dictionary, "providerApnList", apn_list_value,
                       cellular_propety_ui_data);
  }

  SetActivationButtonVisibility(cellular,
                                dictionary,
                                cros_->GetCellularHomeCarrierId());
}

void InternetOptionsHandler::PopulateVPNDetails(
    const chromeos::VirtualNetwork* vpn,
    DictionaryValue* dictionary) {
  dictionary->SetString("service_name", vpn->name());
  bool remembered = (vpn->profile_type() != chromeos::PROFILE_NONE);
  dictionary->SetBoolean("remembered", remembered);
  dictionary->SetString("server_hostname", vpn->server_hostname());
  dictionary->SetString("provider_type", vpn->GetProviderTypeString());
  dictionary->SetString("username", vpn->username());
}

void InternetOptionsHandler::SetActivationButtonVisibility(
    const chromeos::CellularNetwork* cellular,
    DictionaryValue* dictionary,
    const std::string& carrier_id) {
  if (cellular->needs_new_plan()) {
    dictionary->SetBoolean("showBuyButton", true);
  } else if (cellular->activation_state() !=
                 chromeos::ACTIVATION_STATE_ACTIVATING &&
             cellular->activation_state() !=
                 chromeos::ACTIVATION_STATE_ACTIVATED) {
    dictionary->SetBoolean("showActivateButton", true);
  } else {
    const chromeos::MobileConfig::Carrier* carrier =
        chromeos::MobileConfig::GetInstance()->GetCarrier(carrier_id);
    if (carrier && carrier->show_portal_button()) {
      // This will trigger BuyDataPlanCallback() so that
      // chrome://mobilesetup/ will open carrier specific portal.
      dictionary->SetBoolean("showViewAccountButton", true);
    }
  }
}

gfx::NativeWindow InternetOptionsHandler::GetNativeWindow() const {
  // TODO(beng): This is an improper direct dependency on Browser. Route this
  // through some sort of delegate.
  Browser* browser =
      BrowserList::FindBrowserWithProfile(Profile::FromWebUI(web_ui()));
  return browser->window()->GetNativeHandle();
}

Browser* InternetOptionsHandler::GetAppropriateBrowser() {
  return Browser::GetOrCreateTabbedBrowser(
      ProfileManager::GetDefaultProfileOrOffTheRecord());
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

  int type = atoi(str_type.c_str());
  if (type == chromeos::TYPE_ETHERNET) {
    const chromeos::EthernetNetwork* ether = cros_->ethernet_network();
    if (ether)
      PopulateDictionaryDetails(ether);
  } else if (type == chromeos::TYPE_WIFI) {
    HandleWifiButtonClick(service_path, command);
  } else if (type == chromeos::TYPE_CELLULAR) {
    HandleCellularButtonClick(service_path, command);
  } else if (type == chromeos::TYPE_VPN) {
    HandleVPNButtonClick(service_path, command);
  } else {
    NOTREACHED();
  }
}

void InternetOptionsHandler::ToggleAirplaneModeCallback(const ListValue* args) {
  // TODO(kevers): The use of 'offline_mode' is not quite correct.  Update once
  // we have proper back-end support.
  cros_->EnableOfflineMode(!cros_->offline_mode());
}

void InternetOptionsHandler::HandleWifiButtonClick(
    const std::string& service_path,
    const std::string& command) {
  chromeos::WifiNetwork* wifi = NULL;
  if (command == "forget") {
    cros_->ForgetNetwork(service_path);
  } else if (service_path == kOtherNetworksFakePath) {
    // Other wifi networks.
    chromeos::NetworkConfigView::ShowForType(
        chromeos::TYPE_WIFI, GetNativeWindow());
  } else if ((wifi = cros_->FindWifiNetworkByPath(service_path))) {
    if (command == "connect") {
      wifi->SetEnrollmentDelegate(
          chromeos::CreateEnrollmentDelegate(GetNativeWindow(), wifi->name(),
              ProfileManager::GetLastUsedProfile()));
      wifi->AttemptConnection(base::Bind(&InternetOptionsHandler::DoConnect,
                                         weak_factory_.GetWeakPtr(),
                                         wifi));
    } else if (command == "disconnect") {
      cros_->DisconnectFromNetwork(wifi);
    } else if (command == "options") {
      PopulateDictionaryDetails(wifi);
    }
  }
}

void InternetOptionsHandler::HandleCellularButtonClick(
    const std::string& service_path,
    const std::string& command) {
  chromeos::CellularNetwork* cellular = NULL;
  if (service_path == kOtherNetworksFakePath) {
    chromeos::ChooseMobileNetworkDialog::ShowDialog(GetNativeWindow());
  } else if ((cellular = cros_->FindCellularNetworkByPath(service_path))) {
    if (command == "connect") {
      cros_->ConnectToCellularNetwork(cellular);
    } else if (command == "disconnect") {
      cros_->DisconnectFromNetwork(cellular);
    } else if (command == "activate") {
      ash::Shell::GetInstance()->delegate()->OpenMobileSetup();
    } else if (command == "options") {
      PopulateDictionaryDetails(cellular);
    }
  }
}

void InternetOptionsHandler::HandleVPNButtonClick(
    const std::string& service_path,
    const std::string& command) {
  chromeos::VirtualNetwork* network = NULL;
  if (command == "forget") {
    cros_->ForgetNetwork(service_path);
  } else if (service_path == kOtherNetworksFakePath) {
    // TODO(altimofeev): verify if service_path in condition is correct.
    // Other VPN networks.
    chromeos::NetworkConfigView::ShowForType(
        chromeos::TYPE_VPN, GetNativeWindow());
  } else if ((network = cros_->FindVirtualNetworkByPath(service_path))) {
    if (command == "connect") {
      network->SetEnrollmentDelegate(
          chromeos::CreateEnrollmentDelegate(
              GetNativeWindow(),
              network->name(),
              ProfileManager::GetLastUsedProfile()));
      network->AttemptConnection(base::Bind(&InternetOptionsHandler::DoConnect,
                                            weak_factory_.GetWeakPtr(),
                                            network));
    } else if (command == "disconnect") {
      cros_->DisconnectFromNetwork(network);
    } else if (command == "options") {
      PopulateDictionaryDetails(network);
    }
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
  }
  if (network->type() == chromeos::TYPE_WIFI) {
    chromeos::WifiNetwork* wifi = static_cast<chromeos::WifiNetwork*>(network);
    if (wifi->IsPassphraseRequired()) {
      // Show the connection UI if we require a passphrase.
      chromeos::NetworkConfigView::Show(wifi, GetNativeWindow());
    } else {
      cros_->ConnectToWifiNetwork(wifi);
      // Connection failures are responsible for updating the UI, including
      // reopening dialogs.
    }
  }
}

void InternetOptionsHandler::RefreshCellularPlanCallback(
    const ListValue* args) {
  std::string service_path;
  if (args->GetSize() != 1 ||
      !args->GetString(0, &service_path)) {
    NOTREACHED();
    return;
  }
  const chromeos::CellularNetwork* cellular =
      cros_->FindCellularNetworkByPath(service_path);
  if (cellular)
    cellular->RefreshDataPlansIfNeeded();
}

ListValue* InternetOptionsHandler::GetWiredList() {
  ListValue* list = new ListValue();

  // If ethernet is not enabled, then don't add anything.
  if (cros_->ethernet_enabled()) {
    const chromeos::EthernetNetwork* ethernet_network =
        cros_->ethernet_network();
    if (ethernet_network) {
      NetworkInfoDictionary network_dict(ethernet_network);
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
    NetworkInfoDictionary network_dict(*it);
    network_dict.set_connectable(cros_->CanConnectToNetwork(*it));
    list->Append(network_dict.BuildDictionary());
  }

  const chromeos::CellularNetworkVector cellular_networks =
      cros_->cellular_networks();
  for (chromeos::CellularNetworkVector::const_iterator it =
      cellular_networks.begin(); it != cellular_networks.end(); ++it) {
    NetworkInfoDictionary network_dict(*it);
    network_dict.set_connectable(cros_->CanConnectToNetwork(*it));
    network_dict.set_activation_state((*it)->activation_state());
    network_dict.set_needs_new_plan(
        (*it)->SupportsDataPlan() && (*it)->restricted_pool());
    list->Append(network_dict.BuildDictionary());
  }

  const chromeos::NetworkDevice* cellular_device = cros_->FindCellularDevice();
  if (cellular_device && cellular_device->support_network_scan() &&
      cros_->cellular_enabled()) {
    NetworkInfoDictionary network_dict;
    network_dict.set_service_path(kOtherNetworksFakePath);
    network_dict.set_icon(
        chromeos::NetworkMenuIcon::GetDisconnectedBitmap(
            chromeos::NetworkMenuIcon::BARS,
            chromeos::NetworkMenuIcon::COLOR_DARK));
    network_dict.set_name(
        l10n_util::GetStringUTF8(IDS_OPTIONS_SETTINGS_OTHER_CELLULAR_NETWORKS));
    network_dict.set_connectable(true);
    network_dict.set_connection_type(chromeos::TYPE_CELLULAR);
    network_dict.set_activation_state(chromeos::ACTIVATION_STATE_ACTIVATED);
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
    NetworkInfoDictionary network_dict(*it);
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

    NetworkInfoDictionary network_dict(wifi, remembered);
    list->Append(network_dict.BuildDictionary());
  }

  for (chromeos::VirtualNetworkVector::const_iterator rit =
           cros_->remembered_virtual_networks().begin();
       rit != cros_->remembered_virtual_networks().end(); ++rit) {
    chromeos::VirtualNetwork* remembered = *rit;
    chromeos::VirtualNetwork* vpn = static_cast<chromeos::VirtualNetwork*>(
        cros_->FindNetworkByUniqueId(remembered->unique_id()));

    NetworkInfoDictionary network_dict(vpn, remembered);
    list->Append(network_dict.BuildDictionary());
  }

  return list;
}

void InternetOptionsHandler::FillNetworkInfo(DictionaryValue* dictionary) {
  dictionary->SetBoolean("accessLocked", cros_->IsLocked());
  dictionary->Set("wiredList", GetWiredList());
  dictionary->Set("wirelessList", GetWirelessList());
  dictionary->Set("vpnList", GetVPNList());
  dictionary->Set("rememberedList", GetRememberedList());
  dictionary->SetBoolean("wifiAvailable", cros_->wifi_available());
  dictionary->SetBoolean("wifiBusy", cros_->wifi_busy());
  dictionary->SetBoolean("wifiEnabled", cros_->wifi_enabled());
  dictionary->SetBoolean("cellularAvailable", cros_->cellular_available());
  dictionary->SetBoolean("cellularBusy", cros_->cellular_busy());
  dictionary->SetBoolean("cellularEnabled", cros_->cellular_enabled());
  // TODO(kevers): The use of 'offline_mode' is not quite correct.  Update once
  // we have proper back-end support.
  dictionary->SetBoolean("airplaneMode", cros_->offline_mode());
}

void InternetOptionsHandler::SetValueDictionary(
    DictionaryValue* settings,
    const char* key,
    base::Value* value,
    const chromeos::NetworkPropertyUIData& ui_data) {
  DictionaryValue* value_dict = new DictionaryValue();
  // DictionaryValue::Set() takes ownership of |value|.
  if (value)
    value_dict->Set("value", value);
  const base::Value* default_value = ui_data.default_value();
  if (default_value)
    value_dict->Set("default", default_value->DeepCopy());
  if (ui_data.managed())
    value_dict->SetString("controlledBy", "policy");
  else if (ui_data.recommended())
    value_dict->SetString("controlledBy", "recommended");
  settings->Set(key, value_dict);
}

}  // namespace options2
