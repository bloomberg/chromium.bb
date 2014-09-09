// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/internet_options_handler_strings.h"

#include "base/macros.h"
#include "base/values.h"
#include "chrome/grit/generated_resources.h"
#include "grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace internet_options_strings {

namespace {

struct StringResource {
  const char* name;
  int id;
};

StringResource kStringResources[] = {
    // Main settings page.
    {"ethernetName", IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET},
    {"ethernetTitle", IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET},
    {"wifiTitle", IDS_OPTIONS_SETTINGS_SECTION_TITLE_WIFI_NETWORK},
    {"wimaxTitle", IDS_OPTIONS_SETTINGS_SECTION_TITLE_WIMAX_NETWORK},
    {"cellularTitle", IDS_OPTIONS_SETTINGS_SECTION_TITLE_CELLULAR_NETWORK},
    {"vpnTitle", IDS_OPTIONS_SETTINGS_SECTION_TITLE_PRIVATE_NETWORK},
    {"joinOtherNetwork", IDS_OPTIONS_SETTINGS_NETWORK_OTHER},
    {"networkDisabled", IDS_OPTIONS_SETTINGS_NETWORK_DISABLED},
    {"turnOffWifi", IDS_OPTIONS_SETTINGS_NETWORK_DISABLE_WIFI},
    {"turnOffWimax", IDS_OPTIONS_SETTINGS_NETWORK_DISABLE_WIMAX},
    {"turnOffCellular", IDS_OPTIONS_SETTINGS_NETWORK_DISABLE_CELLULAR},
    {"disconnectNetwork", IDS_OPTIONS_SETTINGS_DISCONNECT},
    {"preferredNetworks", IDS_OPTIONS_SETTINGS_PREFERRED_NETWORKS_LABEL},
    {"preferredNetworksPage", IDS_OPTIONS_SETTINGS_PREFERRED_NETWORKS_TITLE},
    {"useSharedProxies", IDS_OPTIONS_SETTINGS_USE_SHARED_PROXIES},
    { "addConnectionTitle",
      IDS_OPTIONS_SETTINGS_SECTION_TITLE_ADD_CONNECTION },
    {"addConnectionWifi", IDS_OPTIONS_SETTINGS_ADD_CONNECTION_WIFI},
    {"addConnectionVPN", IDS_STATUSBAR_NETWORK_ADD_VPN},
    {"otherCellularNetworks", IDS_OPTIONS_SETTINGS_OTHER_CELLULAR_NETWORKS},
    {"enableDataRoaming", IDS_OPTIONS_SETTINGS_ENABLE_DATA_ROAMING},
    {"disableDataRoaming", IDS_OPTIONS_SETTINGS_DISABLE_DATA_ROAMING},
    {"dataRoamingDisableToggleTooltip",
     IDS_OPTIONS_SETTINGS_TOGGLE_DATA_ROAMING_RESTRICTION},

    // ONC network states. Format is 'Onc' + key + value.
    // Note: '.' must be replaced with '-', e.g. VPN.Type -> OncVPN-Type
    {"OncCellular-ActivationStateActivated",
     IDS_CHROMEOS_NETWORK_ACTIVATION_STATE_ACTIVATED},
    {"OncCellular-ActivationStateActivating",
     IDS_CHROMEOS_NETWORK_ACTIVATION_STATE_ACTIVATING},
    {"OncCellular-ActivationStateNotActivated",
     IDS_CHROMEOS_NETWORK_ACTIVATION_STATE_NOT_ACTIVATED},
    {"OncCellular-ActivationStatePartiallyActivated",
     IDS_CHROMEOS_NETWORK_ACTIVATION_STATE_PARTIALLY_ACTIVATED},
    {"OncConnectionStateConnected", IDS_CHROMEOS_NETWORK_STATE_CONNECTED},
    {"OncConnectionStateConnecting", IDS_CHROMEOS_NETWORK_STATE_CONNECTING},
    {"OncConnectionStateNotConnected",
     IDS_CHROMEOS_NETWORK_STATE_NOT_CONNECTED},
    {"OncCellular-RoamingStateHome", IDS_CHROMEOS_NETWORK_ROAMING_STATE_HOME},
    {"OncCellular-RoamingStateRoaming",
     IDS_CHROMEOS_NETWORK_ROAMING_STATE_ROAMING},
    {"OncTypeCellular", IDS_NETWORK_TYPE_CELLULAR},
    {"OncTypeEthernet", IDS_NETWORK_TYPE_ETHERNET},
    {"OncTypeWiFi", IDS_NETWORK_TYPE_WIFI},
    {"OncTypeWimax", IDS_NETWORK_TYPE_WIMAX},
    {"OncVPN-TypeL2TP-IPsecCert",
     IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_L2TP_IPSEC_USER_CERT},
    {"OncVPN-TypeL2TP-IPsecPSK",
     IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_L2TP_IPSEC_PSK},
    {"OncVPN-TypeOpenVPN", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_OPEN_VPN},

    // Internet details dialog.
    {"restrictedNo", IDS_CONFIRM_MESSAGEBOX_NO_BUTTON_LABEL},
    {"restrictedYes", IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL},
    {"managedNetwork", IDS_OPTIONS_SETTINGS_MANAGED_NETWORK},
    {"wifiNetworkTabLabel", IDS_OPTIONS_SETTINGS_INTERNET_TAB_CONNECTION},
    {"vpnTabLabel", IDS_OPTIONS_SETTINGS_INTERNET_TAB_VPN},
    {"cellularConnTabLabel", IDS_OPTIONS_SETTINGS_INTERNET_TAB_CONNECTION},
    {"cellularDeviceTabLabel", IDS_OPTIONS_SETTINGS_INTERNET_TAB_DEVICE},
    {"networkTabLabel", IDS_OPTIONS_SETTINGS_INTERNET_TAB_NETWORK},
    {"securityTabLabel", IDS_OPTIONS_SETTINGS_INTERNET_TAB_SECURITY},
    {"proxyTabLabel", IDS_OPTIONS_SETTINGS_INTERNET_TAB_PROXY},
    {"connectionState", IDS_OPTIONS_SETTINGS_INTERNET_CONNECTION_STATE},
    // TODO(stevenjb): Rename the IDS constant when we redesign the UI.
    {"restrictedConnectivity",
     IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_RESTRICTED_POOL},
    {"inetAddress", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_ADDRESS},
    {"inetNetmask", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SUBNETMASK},
    {"inetGateway", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_GATEWAY},
    {"inetNameServers", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_DNSSERVER},
    {"ipAutomaticConfiguration",
     IDS_OPTIONS_SETTINGS_INTERNET_IP_AUTOMATIC_CONFIGURATION},
    {"automaticNameServers",
     IDS_OPTIONS_SETTINGS_INTERNET_AUTOMATIC_NAME_SERVERS},
    {"userNameServer1", IDS_OPTIONS_SETTINGS_INTERNET_USER_NAME_SERVER_1},
    {"userNameServer2", IDS_OPTIONS_SETTINGS_INTERNET_USER_NAME_SERVER_2},
    {"userNameServer3", IDS_OPTIONS_SETTINGS_INTERNET_USER_NAME_SERVER_3},
    {"userNameServer4", IDS_OPTIONS_SETTINGS_INTERNET_USER_NAME_SERVER_4},
    {"googleNameServers", IDS_OPTIONS_SETTINGS_INTERNET_GOOGLE_NAME_SERVERS},
    {"userNameServers", IDS_OPTIONS_SETTINGS_INTERNET_USER_NAME_SERVERS},
    {"hardwareAddress", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_HARDWARE_ADDRESS},
    {"detailsInternetDismiss", IDS_CLOSE},
    {"activateButton", IDS_OPTIONS_SETTINGS_ACTIVATE},
    {"connectButton", IDS_OPTIONS_SETTINGS_CONNECT},
    {"configureButton", IDS_OPTIONS_SETTINGS_CONFIGURE},
    {"disconnectButton", IDS_OPTIONS_SETTINGS_DISCONNECT},
    {"viewAccountButton", IDS_STATUSBAR_NETWORK_VIEW_ACCOUNT},
    {"wimaxConnTabLabel", IDS_OPTIONS_SETTINGS_INTERNET_TAB_WIMAX},

    // Wifi Tab.
    {"inetSsid", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_NETWORK_ID},
    {"inetBssid", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_NETWORK_BSSID},
    {"inetEncryption",
     IDS_OPTIONS_SETTIGNS_INTERNET_OPTIONS_NETWORK_ENCRYPTION},
    {"inetFrequency", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_NETWORK_FREQUENCY},
    {"inetFrequencyFormat",
     IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_NETWORK_FREQUENCY_MHZ},
    {"inetSignalStrength",
     IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_NETWORK_STRENGTH},
    {"inetSignalStrengthFormat",
     IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_NETWORK_STRENGTH_PERCENTAGE},
    {"inetPassProtected", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_NET_PROTECTED},
    {"inetNetworkShared", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_NETWORK_SHARED},
    {"inetPreferredNetwork",
     IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PREFER_NETWORK},
    {"inetAutoConnectNetwork",
     IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_AUTO_CONNECT},

    // VPN Tab.
    {"inetServiceName", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_VPN_SERVICE_NAME},
    {"inetServerHostname",
     IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_VPN_SERVER_HOSTNAME},
    {"inetProviderType",
     IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_VPN_PROVIDER_TYPE},
    {"inetUsername", IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_VPN_USERNAME},

    // Cellular Tab.
    {"serviceName", IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_SERVICE_NAME},
    {"networkTechnology",
     IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_NETWORK_TECHNOLOGY},
    {"operatorName", IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_OPERATOR},
    {"operatorCode", IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_OPERATOR_CODE},
    {"activationState",
     IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_ACTIVATION_STATE},
    {"roamingState", IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_ROAMING_STATE},
    {"errorState", IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_ERROR_STATE},
    {"cellularManufacturer",
     IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_MANUFACTURER},
    {"modelId", IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_MODEL_ID},
    {"firmwareRevision",
     IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_FIRMWARE_REVISION},
    {"hardwareRevision",
     IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_HARDWARE_REVISION},
    {"prlVersion", IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_PRL_VERSION},
    {"cellularApnLabel", IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_APN},
    {"cellularApnOther", IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_APN_OTHER},
    {"cellularApnUsername",
     IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_APN_USERNAME},
    {"cellularApnPassword",
     IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_APN_PASSWORD},
    {"cellularApnUseDefault", IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_APN_CLEAR},
    {"cellularApnSet", IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_APN_SET},
    {"cellularApnCancel", IDS_CANCEL},

    // Security Tab.
    {"lockSimCard", IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_LOCK_SIM_CARD},
    {"changePinButton",
     IDS_OPTIONS_SETTINGS_INTERNET_CELLULAR_CHANGE_PIN_BUTTON},

    // Proxy Tab.
    {"webProxyAutoDiscoveryUrl", IDS_PROXY_WEB_PROXY_AUTO_DISCOVERY},
};

const size_t kStringResourcesLength = arraysize(kStringResources);

}  // namespace

void RegisterLocalizedStrings(base::DictionaryValue* localized_strings) {
  for (size_t i = 0; i < kStringResourcesLength; ++i) {
    localized_strings->SetString(
        kStringResources[i].name,
        l10n_util::GetStringUTF16(kStringResources[i].id));
  }
}

}  // namespace internet_options_strings
}  // namespace chromeos
