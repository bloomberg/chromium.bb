// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/internet_section.h"

#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/network_config_service.h"
#include "base/bind.h"
#include "base/no_destructor.h"
#include "chrome/browser/ui/webui/chromeos/network_element_localized_strings_provider.h"
#include "chrome/browser/ui/webui/settings/chromeos/constants/routes.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/internet_handler.h"
#include "chrome/browser/ui/webui/settings/chromeos/search/search_tag_registry.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {
namespace settings {
namespace {

const std::vector<SearchConcept>& GetNetworkSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_NETWORK_SETTINGS,
       mojom::kNetworkSectionPath,
       mojom::SearchResultIcon::kWifi,
       mojom::SearchResultDefaultRank::kHigh,
       mojom::SearchResultType::kSection,
       {.section = mojom::Section::kNetwork},
       {IDS_OS_SETTINGS_TAG_NETWORK_SETTINGS_ALT1, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetEthernetConnectedSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_ETHERNET_CONFIGURE,
       mojom::kEthernetDetailsSubpagePath,
       mojom::SearchResultIcon::kEthernet,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kConfigureEthernet}},
      {IDS_OS_SETTINGS_TAG_ETHERNET,
       mojom::kEthernetDetailsSubpagePath,
       mojom::SearchResultIcon::kEthernet,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kEthernetDetails}},
      {IDS_OS_SETTINGS_TAG_CONFIGURE_IP_AUTOMATICALLY,
       mojom::kEthernetDetailsSubpagePath,
       mojom::SearchResultIcon::kEthernet,
       mojom::SearchResultDefaultRank::kLow,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kEthernetAutoConfigureIp},
       {IDS_OS_SETTINGS_TAG_CONFIGURE_IP_AUTOMATICALLY_ALT1,
        IDS_OS_SETTINGS_TAG_CONFIGURE_IP_AUTOMATICALLY_ALT2,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_NAME_SERVERS,
       mojom::kEthernetDetailsSubpagePath,
       mojom::SearchResultIcon::kEthernet,
       mojom::SearchResultDefaultRank::kLow,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kEthernetDns},
       {IDS_OS_SETTINGS_TAG_NAME_SERVERS_ALT1,
        IDS_OS_SETTINGS_TAG_NAME_SERVERS_ALT2, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_PROXY,
       mojom::kEthernetDetailsSubpagePath,
       mojom::SearchResultIcon::kEthernet,
       mojom::SearchResultDefaultRank::kLow,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kEthernetProxy},
       {IDS_OS_SETTINGS_TAG_PROXY_ALT1, IDS_OS_SETTINGS_TAG_PROXY_ALT2,
        IDS_OS_SETTINGS_TAG_PROXY_ALT3, IDS_OS_SETTINGS_TAG_PROXY_ALT4,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetWifiSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_WIFI,
       mojom::kWifiNetworksSubpagePath,
       mojom::SearchResultIcon::kWifi,
       mojom::SearchResultDefaultRank::kHigh,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kWifiNetworks},
       {IDS_OS_SETTINGS_TAG_WIFI_ALT1, IDS_OS_SETTINGS_TAG_WIFI_ALT2,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_KNOWN_NETWORKS,
       mojom::kKnownNetworksSubpagePath,
       mojom::SearchResultIcon::kWifi,
       mojom::SearchResultDefaultRank::kHigh,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kKnownNetworks},
       {IDS_OS_SETTINGS_TAG_KNOWN_NETWORKS_ALT1,
        IDS_OS_SETTINGS_TAG_KNOWN_NETWORKS_ALT2, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetWifiOnSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_WIFI_TURN_OFF,
       mojom::kWifiNetworksSubpagePath,
       mojom::SearchResultIcon::kWifi,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kWifiOnOff},
       {IDS_OS_SETTINGS_TAG_WIFI_TURN_OFF_ALT1, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetWifiOffSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_WIFI_TURN_ON,
       mojom::kWifiNetworksSubpagePath,
       mojom::SearchResultIcon::kWifi,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kWifiOnOff},
       {IDS_OS_SETTINGS_TAG_WIFI_TURN_ON_ALT1, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetWifiConnectedSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_DISCONNECT_WIFI,
       mojom::kWifiDetailsSubpagePath,
       mojom::SearchResultIcon::kWifi,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kDisconnectWifiNetwork}},
      {IDS_OS_SETTINGS_TAG_PREFER_WIFI_NETWORK,
       mojom::kWifiDetailsSubpagePath,
       mojom::SearchResultIcon::kWifi,
       mojom::SearchResultDefaultRank::kLow,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kPreferWifiNetwork},
       {IDS_OS_SETTINGS_TAG_PREFER_WIFI_NETWORK_ALT1,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_WIFI_CONFIGURE,
       mojom::kWifiDetailsSubpagePath,
       mojom::SearchResultIcon::kWifi,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kConfigureWifi}},
      {IDS_OS_SETTINGS_TAG_FORGET_WIFI,
       mojom::kWifiDetailsSubpagePath,
       mojom::SearchResultIcon::kWifi,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kForgetWifiNetwork}},
      {IDS_OS_SETTINGS_TAG_CONFIGURE_IP_AUTOMATICALLY,
       mojom::kWifiDetailsSubpagePath,
       mojom::SearchResultIcon::kWifi,
       mojom::SearchResultDefaultRank::kLow,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kWifiAutoConfigureIp},
       {IDS_OS_SETTINGS_TAG_CONFIGURE_IP_AUTOMATICALLY_ALT1,
        IDS_OS_SETTINGS_TAG_CONFIGURE_IP_AUTOMATICALLY_ALT2,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_NAME_SERVERS,
       mojom::kWifiDetailsSubpagePath,
       mojom::SearchResultIcon::kWifi,
       mojom::SearchResultDefaultRank::kLow,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kWifiDns},
       {IDS_OS_SETTINGS_TAG_NAME_SERVERS_ALT1,
        IDS_OS_SETTINGS_TAG_NAME_SERVERS_ALT2, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_PROXY,
       mojom::kWifiDetailsSubpagePath,
       mojom::SearchResultIcon::kWifi,
       mojom::SearchResultDefaultRank::kLow,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kWifiProxy},
       {IDS_OS_SETTINGS_TAG_PROXY_ALT1, IDS_OS_SETTINGS_TAG_PROXY_ALT2,
        IDS_OS_SETTINGS_TAG_PROXY_ALT3, IDS_OS_SETTINGS_TAG_PROXY_ALT4,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_AUTO_CONNECT_NETWORK,
       mojom::kWifiDetailsSubpagePath,
       mojom::SearchResultIcon::kWifi,
       mojom::SearchResultDefaultRank::kLow,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kWifiAutoConnectToNetwork},
       {IDS_OS_SETTINGS_TAG_AUTO_CONNECT_NETWORK_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetCellularSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_CELLULAR,
       mojom::kCellularDetailsSubpagePath,
       mojom::SearchResultIcon::kCellular,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kCellularDetails},
       {IDS_OS_SETTINGS_TAG_CELLULAR_ALT1, IDS_OS_SETTINGS_TAG_CELLULAR_ALT2,
        IDS_OS_SETTINGS_TAG_CELLULAR_ALT3, IDS_OS_SETTINGS_TAG_CELLULAR_ALT4,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetCellularOnSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_CELLULAR_SIM_LOCK,
       mojom::kCellularDetailsSubpagePath,
       mojom::SearchResultIcon::kCellular,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kCellularSimLock},
       {IDS_OS_SETTINGS_TAG_CELLULAR_SIM_LOCK_ALT1, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_CELLULAR_ROAMING,
       mojom::kCellularDetailsSubpagePath,
       mojom::SearchResultIcon::kCellular,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kCellularRoaming}},
      {IDS_OS_SETTINGS_TAG_CELLULAR_TURN_OFF,
       mojom::kNetworkSectionPath,
       mojom::SearchResultIcon::kCellular,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kMobileOnOff},
       {IDS_OS_SETTINGS_TAG_CELLULAR_TURN_OFF_ALT1, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_CELLULAR_APN,
       mojom::kCellularDetailsSubpagePath,
       mojom::SearchResultIcon::kCellular,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kCellularApn}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetCellularOffSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_CELLULAR_TURN_ON,
       mojom::kNetworkSectionPath,
       mojom::SearchResultIcon::kCellular,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kMobileOnOff},
       {IDS_OS_SETTINGS_TAG_CELLULAR_TURN_ON_ALT1, SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetCellularConnectedSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_CELLULAR_DISCONNECT,
       mojom::kCellularDetailsSubpagePath,
       mojom::SearchResultIcon::kCellular,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kDisconnectCellularNetwork}},
      {IDS_OS_SETTINGS_TAG_CONFIGURE_IP_AUTOMATICALLY,
       mojom::kCellularDetailsSubpagePath,
       mojom::SearchResultIcon::kCellular,
       mojom::SearchResultDefaultRank::kLow,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kCellularAutoConfigureIp},
       {IDS_OS_SETTINGS_TAG_CONFIGURE_IP_AUTOMATICALLY_ALT1,
        IDS_OS_SETTINGS_TAG_CONFIGURE_IP_AUTOMATICALLY_ALT2,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_NAME_SERVERS,
       mojom::kCellularDetailsSubpagePath,
       mojom::SearchResultIcon::kCellular,
       mojom::SearchResultDefaultRank::kLow,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kCellularDns},
       {IDS_OS_SETTINGS_TAG_NAME_SERVERS_ALT1,
        IDS_OS_SETTINGS_TAG_NAME_SERVERS_ALT2, SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_PROXY,
       mojom::kCellularDetailsSubpagePath,
       mojom::SearchResultIcon::kCellular,
       mojom::SearchResultDefaultRank::kLow,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kCellularProxy},
       {IDS_OS_SETTINGS_TAG_PROXY_ALT1, IDS_OS_SETTINGS_TAG_PROXY_ALT2,
        IDS_OS_SETTINGS_TAG_PROXY_ALT3, IDS_OS_SETTINGS_TAG_PROXY_ALT4,
        SearchConcept::kAltTagEnd}},
      {IDS_OS_SETTINGS_TAG_AUTO_CONNECT_NETWORK,
       mojom::kCellularDetailsSubpagePath,
       mojom::SearchResultIcon::kCellular,
       mojom::SearchResultDefaultRank::kLow,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kCellularAutoConnectToNetwork},
       {IDS_OS_SETTINGS_TAG_AUTO_CONNECT_NETWORK_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetInstantTetheringSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_INSTANT_MOBILE_NETWORKS,
       mojom::kMobileDataNetworksSubpagePath,
       mojom::SearchResultIcon::kInstantTethering,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kMobileDataNetworks}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetInstantTetheringOnSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_INSTANT_TETHERING_TURN_OFF,
       mojom::kMobileDataNetworksSubpagePath,
       mojom::SearchResultIcon::kInstantTethering,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kInstantTetheringOnOff},
       {IDS_OS_SETTINGS_TAG_INSTANT_TETHERING_TURN_OFF_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetInstantTetheringOffSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_INSTANT_TETHERING_TURN_ON,
       mojom::kMobileDataNetworksSubpagePath,
       mojom::SearchResultIcon::kInstantTethering,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kInstantTetheringOnOff},
       {IDS_OS_SETTINGS_TAG_INSTANT_TETHERING_TURN_ON_ALT1,
        SearchConcept::kAltTagEnd}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetInstantTetheringConnectedSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_INSTANT_TETHERING_DISCONNECT,
       mojom::kTetherDetailsSubpagePath,
       mojom::SearchResultIcon::kInstantTethering,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSetting,
       {.setting = mojom::Setting::kDisconnectTetherNetwork}},
      {IDS_OS_SETTINGS_TAG_INSTANT_TETHERING,
       mojom::kTetherDetailsSubpagePath,
       mojom::SearchResultIcon::kInstantTethering,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kTetherDetails}},
  });
  return *tags;
}

const std::vector<SearchConcept>& GetVpnConnectedSearchConcepts() {
  static const base::NoDestructor<std::vector<SearchConcept>> tags({
      {IDS_OS_SETTINGS_TAG_VPN,
       mojom::kVpnDetailsSubpagePath,
       mojom::SearchResultIcon::kWifi,
       mojom::SearchResultDefaultRank::kMedium,
       mojom::SearchResultType::kSubpage,
       {.subpage = mojom::Subpage::kVpnDetails}},
  });
  return *tags;
}

bool IsConnected(network_config::mojom::ConnectionStateType connection_state) {
  return connection_state ==
             network_config::mojom::ConnectionStateType::kOnline ||
         connection_state ==
             network_config::mojom::ConnectionStateType::kConnected;
}

}  // namespace

InternetSection::InternetSection(Profile* profile,
                                 SearchTagRegistry* search_tag_registry)
    : OsSettingsSection(profile, search_tag_registry) {
  // General network search tags are always added.
  registry()->AddSearchTags(GetNetworkSearchConcepts());

  // Receive updates when devices (e.g., Ethernet, Wi-Fi) go on/offline.
  ash::GetNetworkConfigService(
      cros_network_config_.BindNewPipeAndPassReceiver());
  cros_network_config_->AddObserver(receiver_.BindNewPipeAndPassRemote());

  // Fetch initial list of devices and active networks.
  FetchDeviceList();
  FetchActiveNetworks();
}

InternetSection::~InternetSection() = default;

void InternetSection::AddLoadTimeData(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
      {"internetAddConnection", IDS_SETTINGS_INTERNET_ADD_CONNECTION},
      {"internetAddConnectionExpandA11yLabel",
       IDS_SETTINGS_INTERNET_ADD_CONNECTION_EXPAND_ACCESSIBILITY_LABEL},
      {"internetAddConnectionNotAllowed",
       IDS_SETTINGS_INTERNET_ADD_CONNECTION_NOT_ALLOWED},
      {"internetAddThirdPartyVPN", IDS_SETTINGS_INTERNET_ADD_THIRD_PARTY_VPN},
      {"internetAddVPN", IDS_SETTINGS_INTERNET_ADD_VPN},
      {"internetAddWiFi", IDS_SETTINGS_INTERNET_ADD_WIFI},
      {"internetConfigName", IDS_SETTINGS_INTERNET_CONFIG_NAME},
      {"internetDetailPageTitle", IDS_SETTINGS_INTERNET_DETAIL},
      {"internetDeviceEnabling", IDS_SETTINGS_INTERNET_DEVICE_ENABLING},
      {"internetDeviceInitializing", IDS_SETTINGS_INTERNET_DEVICE_INITIALIZING},
      {"internetJoinType", IDS_SETTINGS_INTERNET_JOIN_TYPE},
      {"internetKnownNetworksPageTitle", IDS_SETTINGS_INTERNET_KNOWN_NETWORKS},
      {"internetMobileSearching", IDS_SETTINGS_INTERNET_MOBILE_SEARCH},
      {"internetNoNetworks", IDS_SETTINGS_INTERNET_NO_NETWORKS},
      {"internetPageTitle", IDS_SETTINGS_INTERNET},
      {"internetSummaryButtonA11yLabel",
       IDS_SETTINGS_INTERNET_SUMMARY_BUTTON_ACCESSIBILITY_LABEL},
      {"internetToggleMobileA11yLabel",
       IDS_SETTINGS_INTERNET_TOGGLE_MOBILE_ACCESSIBILITY_LABEL},
      {"internetToggleTetherLabel", IDS_SETTINGS_INTERNET_TOGGLE_TETHER_LABEL},
      {"internetToggleTetherSubtext",
       IDS_SETTINGS_INTERNET_TOGGLE_TETHER_SUBTEXT},
      {"internetToggleWiFiA11yLabel",
       IDS_SETTINGS_INTERNET_TOGGLE_WIFI_ACCESSIBILITY_LABEL},
      {"knownNetworksAll", IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_ALL},
      {"knownNetworksButton", IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_BUTTON},
      {"knownNetworksMessage", IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_MESSAGE},
      {"knownNetworksPreferred",
       IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_PREFFERED},
      {"knownNetworksMenuAddPreferred",
       IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_MENU_ADD_PREFERRED},
      {"knownNetworksMenuRemovePreferred",
       IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_MENU_REMOVE_PREFERRED},
      {"knownNetworksMenuForget",
       IDS_SETTINGS_INTERNET_KNOWN_NETWORKS_MENU_FORGET},
      {"networkAllowDataRoaming",
       IDS_SETTINGS_SETTINGS_NETWORK_ALLOW_DATA_ROAMING},
      {"networkAllowDataRoamingEnabledHome",
       IDS_SETTINGS_SETTINGS_NETWORK_ALLOW_DATA_ROAMING_ENABLED_HOME},
      {"networkAllowDataRoamingEnabledRoaming",
       IDS_SETTINGS_SETTINGS_NETWORK_ALLOW_DATA_ROAMING_ENABLED_ROAMING},
      {"networkAllowDataRoamingDisabled",
       IDS_SETTINGS_SETTINGS_NETWORK_ALLOW_DATA_ROAMING_DISABLED},
      {"networkAlwaysOnVpn", IDS_SETTINGS_INTERNET_NETWORK_ALWAYS_ON_VPN},
      {"networkAutoConnect", IDS_SETTINGS_INTERNET_NETWORK_AUTO_CONNECT},
      {"networkAutoConnectCellular",
       IDS_SETTINGS_INTERNET_NETWORK_AUTO_CONNECT_CELLULAR},
      {"networkButtonActivate", IDS_SETTINGS_INTERNET_BUTTON_ACTIVATE},
      {"networkButtonConfigure", IDS_SETTINGS_INTERNET_BUTTON_CONFIGURE},
      {"networkButtonConnect", IDS_SETTINGS_INTERNET_BUTTON_CONNECT},
      {"networkButtonDisconnect", IDS_SETTINGS_INTERNET_BUTTON_DISCONNECT},
      {"networkButtonForget", IDS_SETTINGS_INTERNET_BUTTON_FORGET},
      {"networkButtonViewAccount", IDS_SETTINGS_INTERNET_BUTTON_VIEW_ACCOUNT},
      {"networkConnectNotAllowed", IDS_SETTINGS_INTERNET_CONNECT_NOT_ALLOWED},
      {"networkIPAddress", IDS_SETTINGS_INTERNET_NETWORK_IP_ADDRESS},
      {"networkIPConfigAuto", IDS_SETTINGS_INTERNET_NETWORK_IP_CONFIG_AUTO},
      {"networkNameserversLearnMore", IDS_LEARN_MORE},
      {"networkPrefer", IDS_SETTINGS_INTERNET_NETWORK_PREFER},
      {"networkPrimaryUserControlled",
       IDS_SETTINGS_INTERNET_NETWORK_PRIMARY_USER_CONTROLLED},
      {"networkScanningLabel", IDS_NETWORK_SCANNING_MESSAGE},
      {"networkSectionAdvanced",
       IDS_SETTINGS_INTERNET_NETWORK_SECTION_ADVANCED},
      {"networkSectionAdvancedA11yLabel",
       IDS_SETTINGS_INTERNET_NETWORK_SECTION_ADVANCED_ACCESSIBILITY_LABEL},
      {"networkSectionNetwork", IDS_SETTINGS_INTERNET_NETWORK_SECTION_NETWORK},
      {"networkSectionNetworkExpandA11yLabel",
       IDS_SETTINGS_INTERNET_NETWORK_SECTION_NETWORK_ACCESSIBILITY_LABEL},
      {"networkSectionProxy", IDS_SETTINGS_INTERNET_NETWORK_SECTION_PROXY},
      {"networkSectionProxyExpandA11yLabel",
       IDS_SETTINGS_INTERNET_NETWORK_SECTION_PROXY_ACCESSIBILITY_LABEL},
      {"networkShared", IDS_SETTINGS_INTERNET_NETWORK_SHARED},
      {"networkVpnBuiltin", IDS_NETWORK_TYPE_VPN_BUILTIN},
      {"networkOutOfRange", IDS_SETTINGS_INTERNET_WIFI_NETWORK_OUT_OF_RANGE},
      {"cellularContactSpecificCarrier",
       IDS_SETTINGS_INTERNET_CELLULAR_CONTACT_SPECIFIC_CARRIER},
      {"cellularContactDefaultCarrier",
       IDS_SETTINGS_INTERNET_CELLULAR_CONTACT_DEFAULT_CARRIER},
      {"tetherPhoneOutOfRange",
       IDS_SETTINGS_INTERNET_TETHER_PHONE_OUT_OF_RANGE},
      {"gmscoreNotificationsTitle",
       IDS_SETTINGS_INTERNET_GMSCORE_NOTIFICATIONS_TITLE},
      {"gmscoreNotificationsOneDeviceSubtitle",
       IDS_SETTINGS_INTERNET_GMSCORE_NOTIFICATIONS_ONE_DEVICE_SUBTITLE},
      {"gmscoreNotificationsTwoDevicesSubtitle",
       IDS_SETTINGS_INTERNET_GMSCORE_NOTIFICATIONS_TWO_DEVICES_SUBTITLE},
      {"gmscoreNotificationsManyDevicesSubtitle",
       IDS_SETTINGS_INTERNET_GMSCORE_NOTIFICATIONS_MANY_DEVICES_SUBTITLE},
      {"gmscoreNotificationsFirstStep",
       IDS_SETTINGS_INTERNET_GMSCORE_NOTIFICATIONS_FIRST_STEP},
      {"gmscoreNotificationsSecondStep",
       IDS_SETTINGS_INTERNET_GMSCORE_NOTIFICATIONS_SECOND_STEP},
      {"gmscoreNotificationsThirdStep",
       IDS_SETTINGS_INTERNET_GMSCORE_NOTIFICATIONS_THIRD_STEP},
      {"gmscoreNotificationsFourthStep",
       IDS_SETTINGS_INTERNET_GMSCORE_NOTIFICATIONS_FOURTH_STEP},
      {"tetherConnectionDialogTitle",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_DIALOG_TITLE},
      {"tetherConnectionAvailableDeviceTitle",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_AVAILABLE_DEVICE_TITLE},
      {"tetherConnectionBatteryPercentage",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_BATTERY_PERCENTAGE},
      {"tetherConnectionExplanation",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_EXPLANATION},
      {"tetherConnectionCarrierWarning",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_CARRIER_WARNING},
      {"tetherConnectionDescriptionTitle",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_DESCRIPTION_TITLE},
      {"tetherConnectionDescriptionMobileData",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_DESCRIPTION_MOBILE_DATA},
      {"tetherConnectionDescriptionBattery",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_DESCRIPTION_BATTERY},
      {"tetherConnectionDescriptionWiFi",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_DESCRIPTION_WIFI},
      {"tetherConnectionNotNowButton",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_NOT_NOW_BUTTON},
      {"tetherConnectionConnectButton",
       IDS_SETTINGS_INTERNET_TETHER_CONNECTION_CONNECT_BUTTON},
      {"tetherEnableBluetooth", IDS_ENABLE_BLUETOOTH},
  };
  AddLocalizedStringsBulk(html_source, kLocalizedStrings);

  chromeos::network_element::AddLocalizedStrings(html_source);
  chromeos::network_element::AddOncLocalizedStrings(html_source);
  chromeos::network_element::AddDetailsLocalizedStrings(html_source);
  chromeos::network_element::AddConfigLocalizedStrings(html_source);
  chromeos::network_element::AddErrorLocalizedStrings(html_source);

  html_source->AddBoolean("showTechnologyBadge",
                          !ash::features::IsSeparateNetworkIconsEnabled());

  html_source->AddString("networkGoogleNameserversLearnMoreUrl",
                         chrome::kGoogleNameserversLearnMoreURL);

  html_source->AddString(
      "networkSyncedShared",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_INTERNET_NETWORK_SYNCED_SHARED,
          GetHelpUrlWithBoard(chrome::kWifiSyncLearnMoreURL)));
  html_source->AddString(
      "networkSyncedUser",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_INTERNET_NETWORK_SYNCED_USER,
          GetHelpUrlWithBoard(chrome::kWifiSyncLearnMoreURL)));
  html_source->AddString(
      "internetNoNetworksMobileData",
      l10n_util::GetStringFUTF16(
          IDS_SETTINGS_INTERNET_LOOKING_FOR_MOBILE_NETWORK,
          GetHelpUrlWithBoard(chrome::kInstantTetheringLearnMoreURL)));
}

void InternetSection::AddHandlers(content::WebUI* web_ui) {
  web_ui->AddMessageHandler(std::make_unique<InternetHandler>(profile()));
}

void InternetSection::OnDeviceStateListChanged() {
  FetchDeviceList();
}

void InternetSection::OnActiveNetworksChanged(
    std::vector<network_config::mojom::NetworkStatePropertiesPtr> networks) {
  OnActiveNetworks(std::move(networks));
}

void InternetSection::FetchDeviceList() {
  cros_network_config_->GetDeviceStateList(
      base::BindOnce(&InternetSection::OnDeviceList, base::Unretained(this)));
}

void InternetSection::OnDeviceList(
    std::vector<network_config::mojom::DeviceStatePropertiesPtr> devices) {
  using network_config::mojom::DeviceStateType;
  using network_config::mojom::NetworkType;

  registry()->RemoveSearchTags(GetWifiSearchConcepts());
  registry()->RemoveSearchTags(GetWifiOnSearchConcepts());
  registry()->RemoveSearchTags(GetWifiOffSearchConcepts());
  registry()->RemoveSearchTags(GetCellularSearchConcepts());
  registry()->RemoveSearchTags(GetCellularOnSearchConcepts());
  registry()->RemoveSearchTags(GetCellularOffSearchConcepts());
  registry()->RemoveSearchTags(GetInstantTetheringSearchConcepts());
  registry()->RemoveSearchTags(GetInstantTetheringOnSearchConcepts());
  registry()->RemoveSearchTags(GetInstantTetheringOffSearchConcepts());

  for (const auto& device : devices) {
    switch (device->type) {
      case NetworkType::kWiFi:
        registry()->AddSearchTags(GetWifiSearchConcepts());
        if (device->device_state == DeviceStateType::kEnabled)
          registry()->AddSearchTags(GetWifiOnSearchConcepts());
        else if (device->device_state == DeviceStateType::kDisabled)
          registry()->AddSearchTags(GetWifiOffSearchConcepts());
        break;

      case NetworkType::kCellular:
        registry()->AddSearchTags(GetCellularSearchConcepts());
        if (device->device_state == DeviceStateType::kEnabled)
          registry()->AddSearchTags(GetCellularOnSearchConcepts());
        else if (device->device_state == DeviceStateType::kDisabled)
          registry()->AddSearchTags(GetCellularOffSearchConcepts());
        break;

      case NetworkType::kTether:
        registry()->AddSearchTags(GetInstantTetheringSearchConcepts());
        if (device->device_state == DeviceStateType::kEnabled)
          registry()->AddSearchTags(GetInstantTetheringOnSearchConcepts());
        else if (device->device_state == DeviceStateType::kDisabled)
          registry()->AddSearchTags(GetInstantTetheringOffSearchConcepts());
        break;

      default:
        // Note: Ethernet and VPN only show search tags when connected, and
        // categories such as Mobile/Wireless do not have search tags.
        break;
    }
  }
}

void InternetSection::FetchActiveNetworks() {
  cros_network_config_->GetNetworkStateList(
      network_config::mojom::NetworkFilter::New(
          network_config::mojom::FilterType::kActive,
          network_config::mojom::NetworkType::kAll,
          network_config::mojom::kNoLimit),
      base::Bind(&InternetSection::OnActiveNetworks, base::Unretained(this)));
}

void InternetSection::OnActiveNetworks(
    std::vector<network_config::mojom::NetworkStatePropertiesPtr> networks) {
  using network_config::mojom::NetworkType;

  registry()->RemoveSearchTags(GetEthernetConnectedSearchConcepts());
  registry()->RemoveSearchTags(GetWifiConnectedSearchConcepts());
  registry()->RemoveSearchTags(GetCellularConnectedSearchConcepts());
  registry()->RemoveSearchTags(GetInstantTetheringConnectedSearchConcepts());
  registry()->RemoveSearchTags(GetVpnConnectedSearchConcepts());

  for (const auto& network : networks) {
    if (!IsConnected(network->connection_state))
      continue;

    switch (network->type) {
      case NetworkType::kEthernet:
        registry()->AddSearchTags(GetEthernetConnectedSearchConcepts());
        break;

      case NetworkType::kWiFi:
        registry()->AddSearchTags(GetWifiConnectedSearchConcepts());
        break;

      case NetworkType::kCellular:
        registry()->AddSearchTags(GetCellularConnectedSearchConcepts());
        break;

      case NetworkType::kTether:
        registry()->AddSearchTags(GetInstantTetheringConnectedSearchConcepts());
        break;

      case NetworkType::kVPN:
        registry()->AddSearchTags(GetVpnConnectedSearchConcepts());
        break;

      default:
        // Note: Category types such as Mobile/Wireless do not have search tags.
        break;
    }
  }
}

}  // namespace settings
}  // namespace chromeos
