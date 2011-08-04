// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/network_library.h"

#include <algorithm>
#include <map>
#include <set>

#include "base/i18n/icu_encoding_detection.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "base/string_number_conversions.h"
#include "base/string_tokenizer.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "base/utf_string_conversion_utils.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/network_login_observer.h"
#include "chrome/browser/chromeos/user_cros_settings_provider.h"
#include "chrome/common/time_format.h"
#include "content/browser/browser_thread.h"
#include "crypto/nss_util.h"  // crypto::GetTPMTokenInfo() for 802.1X and VPN.
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/text/bytes_formatting.h"

////////////////////////////////////////////////////////////////////////////////
// Implementation notes.
// NetworkLibraryImpl manages a series of classes that describe network devices
// and services:
//
// NetworkDevice: e.g. ethernet, wifi modem, cellular modem
//  device_map_: canonical map<path,NetworkDevice*> for devices
//
// Network: a network service ("network").
//  network_map_: canonical map<path,Network*> for all visible networks.
//  EthernetNetwork
//   ethernet_: EthernetNetwork* to the active ethernet network in network_map_.
//  WirelessNetwork: a WiFi or Cellular Network.
//  WifiNetwork
//   active_wifi_: WifiNetwork* to the active wifi network in network_map_.
//   wifi_networks_: ordered vector of WifiNetwork* entries in network_map_,
//       in descending order of importance.
//  CellularNetwork
//   active_cellular_: Cellular version of wifi_.
//   cellular_networks_: Cellular version of wifi_.
// network_unique_id_map_: map<unique_id,Network*> for visible networks.
// remembered_network_map_: a canonical map<path,Network*> for all networks
//     remembered in the active Profile ("favorites").
// remembered_wifi_networks_: ordered vector of WifiNetwork* entries in
//     remembered_network_map_, in descending order of preference.
// remembered_virtual_networks_: ordered vector of VirtualNetwork* entries in
//     remembered_network_map_, in descending order of preference.
//
// network_manager_monitor_: a handle to the libcros network Manager handler.
// NetworkManagerStatusChanged: This handles all messages from the Manager.
//   Messages are parsed here and the appropriate updates are then requested.
//
// UpdateNetworkServiceList: This is the primary Manager handler. It handles
//  the "Services" message which list all visible networks. The handler
//  rebuilds the network lists without destroying existing Network structures,
//  then requests neccessary updates to be fetched asynchronously from
//  libcros (RequestNetworkServiceInfo).
//
// TODO(stevenjb): Document cellular data plan handlers.
//
// AddNetworkObserver: Adds an observer for a specific network.
// UpdateNetworkStatus: This handles changes to a monitored service, typically
//     changes to transient states like Strength. (Note: also updates State).
//
// AddNetworkDeviceObserver: Adds an observer for a specific device.
//                           Will be called on any device property change.
// UpdateNetworkDeviceStatus: Handles changes to a monitored device, like
//     SIM lock state and updates device state.
//
// All *Pin(...) methods use internal callback that would update cellular
//    device state once async call is completed and notify all device observers.
//
////////////////////////////////////////////////////////////////////////////////

namespace chromeos {

// Local constants.
namespace {

// Only send network change notifications to observers once every 50ms.
const int kNetworkNotifyDelayMs = 50;

// How long we should remember that cellular plan payment was received.
const int kRecentPlanPaymentHours = 6;

// Default value of the SIM unlock retries count. It is updated to the real
// retries count once cellular device with SIM card is initialized.
// If cellular device doesn't have SIM card, then retries are never used.
const int kDefaultSimUnlockRetriesCount = 999;

// Format of the Carrier ID: <carrier name> (<carrier country>).
const char kCarrierIdFormat[] = "%s (%s)";

// Path of the default (shared) flimflam profile.
const char kSharedProfilePath[] = "/profile/default";

// Type of a pending SIM operation.
enum SimOperationType {
  SIM_OPERATION_NONE               = 0,
  SIM_OPERATION_CHANGE_PIN         = 1,
  SIM_OPERATION_CHANGE_REQUIRE_PIN = 2,
  SIM_OPERATION_ENTER_PIN          = 3,
  SIM_OPERATION_UNBLOCK_PIN        = 4,
};

// D-Bus interface string constants.

// Flimflam manager properties.
const char kAvailableTechnologiesProperty[] = "AvailableTechnologies";
const char kEnabledTechnologiesProperty[] = "EnabledTechnologies";
const char kConnectedTechnologiesProperty[] = "ConnectedTechnologies";
const char kDefaultTechnologyProperty[] = "DefaultTechnology";
const char kOfflineModeProperty[] = "OfflineMode";
const char kActiveProfileProperty[] = "ActiveProfile";
const char kProfilesProperty[] = "Profiles";
const char kServicesProperty[] = "Services";
const char kServiceWatchListProperty[] = "ServiceWatchList";
const char kDevicesProperty[] = "Devices";
const char kPortalURLProperty[] = "PortalURL";
const char kCheckPortalListProperty[] = "CheckPortalList";

// Flimflam service properties.
const char kSecurityProperty[] = "Security";
const char kPassphraseProperty[] = "Passphrase";
const char kIdentityProperty[] = "Identity";
const char kPassphraseRequiredProperty[] = "PassphraseRequired";
const char kSaveCredentialsProperty[] = "SaveCredentials";
const char kSignalStrengthProperty[] = "Strength";
const char kNameProperty[] = "Name";
const char kStateProperty[] = "State";
const char kTypeProperty[] = "Type";
const char kDeviceProperty[] = "Device";
const char kProfileProperty[] = "Profile";
const char kTechnologyFamilyProperty[] = "Cellular.Family";
const char kActivationStateProperty[] = "Cellular.ActivationState";
const char kNetworkTechnologyProperty[] = "Cellular.NetworkTechnology";
const char kRoamingStateProperty[] = "Cellular.RoamingState";
const char kOperatorNameProperty[] = "Cellular.OperatorName";
const char kOperatorCodeProperty[] = "Cellular.OperatorCode";
const char kServingOperatorProperty[] = "Cellular.ServingOperator";
const char kPaymentURLProperty[] = "Cellular.OlpUrl";
const char kUsageURLProperty[] = "Cellular.UsageUrl";
const char kCellularApnProperty[] = "Cellular.APN";
const char kCellularLastGoodApnProperty[] = "Cellular.LastGoodAPN";
const char kCellularApnListProperty[] = "Cellular.APNList";
const char kWifiHexSsid[] = "WiFi.HexSSID";
const char kWifiFrequency[] = "WiFi.Frequency";
const char kWifiHiddenSsid[] = "WiFi.HiddenSSID";
const char kWifiPhyMode[] = "WiFi.PhyMode";
const char kWifiAuthMode[] = "WiFi.AuthMode";
const char kFavoriteProperty[] = "Favorite";
const char kConnectableProperty[] = "Connectable";
const char kPriorityProperty[] = "Priority";
const char kAutoConnectProperty[] = "AutoConnect";
const char kIsActiveProperty[] = "IsActive";
const char kModeProperty[] = "Mode";
const char kErrorProperty[] = "Error";
const char kEntriesProperty[] = "Entries";
const char kProviderProperty[] = "Provider";
const char kHostProperty[] = "Host";
const char kProxyConfigProperty[] = "ProxyConfig";

// Flimflam property names for SIMLock status.
const char kSIMLockStatusProperty[] = "Cellular.SIMLockStatus";
const char kSIMLockTypeProperty[] = "LockType";
const char kSIMLockRetriesLeftProperty[] = "RetriesLeft";

// Flimflam property names for Cellular.FoundNetworks.
const char kLongNameProperty[] = "long_name";
const char kStatusProperty[] = "status";
const char kShortNameProperty[] = "short_name";
const char kTechnologyProperty[] = "technology";
const char kNetworkIdProperty[] = "network_id";

// Flimflam SIMLock status types.
const char kSIMLockPin[] = "sim-pin";
const char kSIMLockPuk[] = "sim-puk";

// APN info property names.
const char kApnProperty[] = "apn";
const char kApnNetworkIdProperty[] = "network_id";
const char kApnUsernameProperty[] = "username";
const char kApnPasswordProperty[] = "password";
const char kApnNameProperty[] = "name";
const char kApnLocalizedNameProperty[] = "localized_name";
const char kApnLanguageProperty[] = "language";

// Operator info property names.
const char kOperatorNameKey[] = "name";
const char kOperatorCodeKey[] = "code";
const char kOperatorCountryKey[] = "country";

// Flimflam device info property names.
const char kScanningProperty[] = "Scanning";
const char kPoweredProperty[] = "Powered";
const char kNetworksProperty[] = "Networks";
const char kCarrierProperty[] = "Cellular.Carrier";
const char kCellularAllowRoamingProperty[] = "Cellular.AllowRoaming";
const char kHomeProviderProperty[] = "Cellular.HomeProvider";
const char kMeidProperty[] = "Cellular.MEID";
const char kImeiProperty[] = "Cellular.IMEI";
const char kImsiProperty[] = "Cellular.IMSI";
const char kEsnProperty[] = "Cellular.ESN";
const char kMdnProperty[] = "Cellular.MDN";
const char kMinProperty[] = "Cellular.MIN";
const char kModelIDProperty[] = "Cellular.ModelID";
const char kManufacturerProperty[] = "Cellular.Manufacturer";
const char kFirmwareRevisionProperty[] = "Cellular.FirmwareRevision";
const char kHardwareRevisionProperty[] = "Cellular.HardwareRevision";
const char kPRLVersionProperty[] = "Cellular.PRLVersion"; // (INT16)
const char kSelectedNetworkProperty[] = "Cellular.SelectedNetwork";
const char kSupportNetworkScanProperty[] = "Cellular.SupportNetworkScan";
const char kFoundNetworksProperty[] = "Cellular.FoundNetworks";

// Flimflam ip config property names.
const char kAddressProperty[] = "Address";
const char kPrefixlenProperty[] = "Prefixlen";
const char kGatewayProperty[] = "Gateway";
const char kNameServersProperty[] = "NameServers";

// Flimflam type options.
const char kTypeEthernet[] = "ethernet";
const char kTypeWifi[] = "wifi";
const char kTypeWimax[] = "wimax";
const char kTypeBluetooth[] = "bluetooth";
const char kTypeCellular[] = "cellular";
const char kTypeVPN[] = "vpn";

// Flimflam mode options.
const char kModeManaged[] = "managed";
const char kModeAdhoc[] = "adhoc";

// Flimflam security options.
const char kSecurityWpa[] = "wpa";
const char kSecurityWep[] = "wep";
const char kSecurityRsn[] = "rsn";
const char kSecurity8021x[] = "802_1x";
const char kSecurityPsk[] = "psk";
const char kSecurityNone[] = "none";

// Flimflam L2TPIPsec property names.
const char kL2TPIPSecCACertNSSProperty[] = "L2TPIPsec.CACertNSS";
const char kL2TPIPSecClientCertIDProperty[] = "L2TPIPsec.ClientCertID";
const char kL2TPIPSecClientCertSlotProp[] = "L2TPIPsec.ClientCertSlot";
const char kL2TPIPSecPINProperty[] = "L2TPIPsec.PIN";
const char kL2TPIPSecPSKProperty[] = "L2TPIPsec.PSK";
const char kL2TPIPSecUserProperty[] = "L2TPIPsec.User";
const char kL2TPIPSecPasswordProperty[] = "L2TPIPsec.Password";

// Flimflam EAP property names.
// See src/third_party/flimflam/doc/service-api.txt.
const char kEapIdentityProperty[] = "EAP.Identity";
const char kEapMethodProperty[] = "EAP.EAP";
const char kEapPhase2AuthProperty[] = "EAP.InnerEAP";
const char kEapAnonymousIdentityProperty[] = "EAP.AnonymousIdentity";
const char kEapClientCertProperty[] = "EAP.ClientCert";  // path
const char kEapCertIDProperty[] = "EAP.CertID";  // PKCS#11 ID
const char kEapClientCertNssProperty[] = "EAP.ClientCertNSS";  // NSS nickname
const char kEapPrivateKeyProperty[] = "EAP.PrivateKey";
const char kEapPrivateKeyPasswordProperty[] = "EAP.PrivateKeyPassword";
const char kEapKeyIDProperty[] = "EAP.KeyID";
const char kEapCaCertProperty[] = "EAP.CACert";  // server CA cert path
const char kEapCaCertIDProperty[] = "EAP.CACertID";  // server CA PKCS#11 ID
const char kEapCaCertNssProperty[] = "EAP.CACertNSS";  // server CA NSS nickname
const char kEapUseSystemCAsProperty[] = "EAP.UseSystemCAs";
const char kEapPinProperty[] = "EAP.PIN";
const char kEapPasswordProperty[] = "EAP.Password";
const char kEapKeyMgmtProperty[] = "EAP.KeyMgmt";

// Flimflam EAP method options.
const char kEapMethodPEAP[] = "PEAP";
const char kEapMethodTLS[] = "TLS";
const char kEapMethodTTLS[] = "TTLS";
const char kEapMethodLEAP[] = "LEAP";

// Flimflam EAP phase 2 auth options.
const char kEapPhase2AuthPEAPMD5[] = "auth=MD5";
const char kEapPhase2AuthPEAPMSCHAPV2[] = "auth=MSCHAPV2";
const char kEapPhase2AuthTTLSMD5[] = "autheap=MD5";
const char kEapPhase2AuthTTLSMSCHAPV2[] = "autheap=MSCHAPV2";
const char kEapPhase2AuthTTLSMSCHAP[] = "autheap=MSCHAP";
const char kEapPhase2AuthTTLSPAP[] = "autheap=PAP";
const char kEapPhase2AuthTTLSCHAP[] = "autheap=CHAP";

// Flimflam VPN provider types.
const char kProviderL2tpIpsec[] = "l2tpipsec";
const char kProviderOpenVpn[] = "openvpn";


// Flimflam state options.
const char kStateIdle[] = "idle";
const char kStateCarrier[] = "carrier";
const char kStateAssociation[] = "association";
const char kStateConfiguration[] = "configuration";
const char kStateReady[] = "ready";
const char kStatePortal[] = "portal";
const char kStateOnline[] = "online";
const char kStateDisconnect[] = "disconnect";
const char kStateFailure[] = "failure";
const char kStateActivationFailure[] = "activation-failure";

// Flimflam network technology options.
const char kNetworkTechnology1Xrtt[] = "1xRTT";
const char kNetworkTechnologyEvdo[] = "EVDO";
const char kNetworkTechnologyGprs[] = "GPRS";
const char kNetworkTechnologyEdge[] = "EDGE";
const char kNetworkTechnologyUmts[] = "UMTS";
const char kNetworkTechnologyHspa[] = "HSPA";
const char kNetworkTechnologyHspaPlus[] = "HSPA+";
const char kNetworkTechnologyLte[] = "LTE";
const char kNetworkTechnologyLteAdvanced[] = "LTE Advanced";
const char kNetworkTechnologyGsm[] = "GSM";

// Flimflam roaming state options
const char kRoamingStateHome[] = "home";
const char kRoamingStateRoaming[] = "roaming";
const char kRoamingStateUnknown[] = "unknown";

// Flimflam activation state options
const char kActivationStateActivated[] = "activated";
const char kActivationStateActivating[] = "activating";
const char kActivationStateNotActivated[] = "not-activated";
const char kActivationStatePartiallyActivated[] = "partially-activated";
const char kActivationStateUnknown[] = "unknown";

// FlimFlam technology family options
const char kTechnologyFamilyCdma[] = "CDMA";
const char kTechnologyFamilyGsm[] = "GSM";

// Flimflam error options.
const char kErrorOutOfRange[] = "out-of-range";
const char kErrorPinMissing[] = "pin-missing";
const char kErrorDhcpFailed[] = "dhcp-failed";
const char kErrorConnectFailed[] = "connect-failed";
const char kErrorBadPassphrase[] = "bad-passphrase";
const char kErrorBadWEPKey[] = "bad-wepkey";
const char kErrorActivationFailed[] = "activation-failed";
const char kErrorNeedEvdo[] = "need-evdo";
const char kErrorNeedHomeNetwork[] = "need-home-network";
const char kErrorOtaspFailed[] = "otasp-failed";
const char kErrorAaaFailed[] = "aaa-failed";
const char kErrorInternal[] = "internal-error";
const char kErrorDNSLookupFailed[] = "dns-lookup-failed";
const char kErrorHTTPGetFailed[] = "http-get-failed";

// Flimflam error messages.
const char kErrorPassphraseRequiredMsg[] = "Passphrase required";
const char kErrorIncorrectPinMsg[] = "org.chromium.flimflam.Error.IncorrectPin";
const char kErrorPinBlockedMsg[] = "org.chromium.flimflam.Error.PinBlocked";
const char kErrorPinRequiredMsg[] = "org.chromium.flimflam.Error.PinRequired";

const char kUnknownString[] = "UNKNOWN";

////////////////////////////////////////////////////////////////////////////

static const char* ConnectionTypeToString(ConnectionType type) {
  switch (type) {
    case TYPE_UNKNOWN:
      break;
    case TYPE_ETHERNET:
      return kTypeEthernet;
    case TYPE_WIFI:
      return kTypeWifi;
    case TYPE_WIMAX:
      return kTypeWimax;
    case TYPE_BLUETOOTH:
      return kTypeBluetooth;
    case TYPE_CELLULAR:
      return kTypeCellular;
    case TYPE_VPN:
      return kTypeVPN;
  }
  LOG(ERROR) << "ConnectionTypeToString called with unknown type: " << type;
  return kUnknownString;
}

static const char* SecurityToString(ConnectionSecurity security) {
  switch (security) {
    case SECURITY_NONE:
      return kSecurityNone;
    case SECURITY_WEP:
      return kSecurityWep;
    case SECURITY_WPA:
      return kSecurityWpa;
    case SECURITY_RSN:
      return kSecurityRsn;
    case SECURITY_8021X:
      return kSecurity8021x;
    case SECURITY_PSK:
      return kSecurityPsk;
    case SECURITY_UNKNOWN:
      break;
  }
  LOG(ERROR) << "SecurityToString called with unknown type: " << security;
  return kUnknownString;
}

static const char* ProviderTypeToString(VirtualNetwork::ProviderType type) {
  switch (type) {
    case VirtualNetwork::PROVIDER_TYPE_L2TP_IPSEC_PSK:
    case VirtualNetwork::PROVIDER_TYPE_L2TP_IPSEC_USER_CERT:
      return kProviderL2tpIpsec;
    case VirtualNetwork::PROVIDER_TYPE_OPEN_VPN:
      return kProviderOpenVpn;
    case VirtualNetwork::PROVIDER_TYPE_MAX:
      break;
  }
  LOG(ERROR) << "ProviderTypeToString called with unknown type: " << type;
  return kUnknownString;
}

////////////////////////////////////////////////////////////////////////////

// Helper class to cache maps of strings to enums.
template <typename Type>
class StringToEnum {
 public:
  struct Pair {
    const char* key;
    const Type value;
  };

  StringToEnum(const Pair* list, size_t num_entries, Type unknown)
      : unknown_value_(unknown) {
    for (size_t i = 0; i < num_entries; ++i, ++list)
      enum_map_[list->key] = list->value;
  }

  Type Get(const std::string& type) const {
    EnumMapConstIter iter = enum_map_.find(type);
    if (iter != enum_map_.end())
      return iter->second;
    return unknown_value_;
  }

 private:
  typedef typename std::map<std::string, Type> EnumMap;
  typedef typename std::map<std::string, Type>::const_iterator EnumMapConstIter;
  EnumMap enum_map_;
  Type unknown_value_;
  DISALLOW_COPY_AND_ASSIGN(StringToEnum);
};

////////////////////////////////////////////////////////////////////////////

enum PropertyIndex {
  PROPERTY_INDEX_ACTIVATION_STATE,
  PROPERTY_INDEX_ACTIVE_PROFILE,
  PROPERTY_INDEX_AUTO_CONNECT,
  PROPERTY_INDEX_AVAILABLE_TECHNOLOGIES,
  PROPERTY_INDEX_CARRIER,
  PROPERTY_INDEX_CELLULAR_ALLOW_ROAMING,
  PROPERTY_INDEX_CELLULAR_APN,
  PROPERTY_INDEX_CELLULAR_APN_LIST,
  PROPERTY_INDEX_CELLULAR_LAST_GOOD_APN,
  PROPERTY_INDEX_CHECK_PORTAL_LIST,
  PROPERTY_INDEX_CONNECTABLE,
  PROPERTY_INDEX_CONNECTED_TECHNOLOGIES,
  PROPERTY_INDEX_CONNECTIVITY_STATE,
  PROPERTY_INDEX_DEFAULT_TECHNOLOGY,
  PROPERTY_INDEX_DEVICE,
  PROPERTY_INDEX_DEVICES,
  PROPERTY_INDEX_EAP_ANONYMOUS_IDENTITY,
  PROPERTY_INDEX_EAP_CA_CERT,
  PROPERTY_INDEX_EAP_CA_CERT_ID,
  PROPERTY_INDEX_EAP_CA_CERT_NSS,
  PROPERTY_INDEX_EAP_CERT_ID,
  PROPERTY_INDEX_EAP_CLIENT_CERT,
  PROPERTY_INDEX_EAP_CLIENT_CERT_NSS,
  PROPERTY_INDEX_EAP_IDENTITY,
  PROPERTY_INDEX_EAP_KEY_ID,
  PROPERTY_INDEX_EAP_KEY_MGMT,
  PROPERTY_INDEX_EAP_METHOD,
  PROPERTY_INDEX_EAP_PASSWORD,
  PROPERTY_INDEX_EAP_PHASE_2_AUTH,
  PROPERTY_INDEX_EAP_PIN,
  PROPERTY_INDEX_EAP_PRIVATE_KEY,
  PROPERTY_INDEX_EAP_PRIVATE_KEY_PASSWORD,
  PROPERTY_INDEX_EAP_USE_SYSTEM_CAS,
  PROPERTY_INDEX_ENABLED_TECHNOLOGIES,
  PROPERTY_INDEX_ERROR,
  PROPERTY_INDEX_ESN,
  PROPERTY_INDEX_FAVORITE,
  PROPERTY_INDEX_FIRMWARE_REVISION,
  PROPERTY_INDEX_FOUND_NETWORKS,
  PROPERTY_INDEX_HARDWARE_REVISION,
  PROPERTY_INDEX_HOME_PROVIDER,
  PROPERTY_INDEX_HOST,
  PROPERTY_INDEX_IDENTITY,
  PROPERTY_INDEX_IMEI,
  PROPERTY_INDEX_IMSI,
  PROPERTY_INDEX_IS_ACTIVE,
  PROPERTY_INDEX_L2TPIPSEC_CA_CERT_NSS,
  PROPERTY_INDEX_L2TPIPSEC_CLIENT_CERT_ID,
  PROPERTY_INDEX_L2TPIPSEC_CLIENT_CERT_SLOT,
  PROPERTY_INDEX_L2TPIPSEC_PASSWORD,
  PROPERTY_INDEX_L2TPIPSEC_PIN,
  PROPERTY_INDEX_L2TPIPSEC_PSK,
  PROPERTY_INDEX_L2TPIPSEC_USER,
  PROPERTY_INDEX_MANUFACTURER,
  PROPERTY_INDEX_MDN,
  PROPERTY_INDEX_MEID,
  PROPERTY_INDEX_MIN,
  PROPERTY_INDEX_MODE,
  PROPERTY_INDEX_MODEL_ID,
  PROPERTY_INDEX_NAME,
  PROPERTY_INDEX_NETWORKS,
  PROPERTY_INDEX_NETWORK_TECHNOLOGY,
  PROPERTY_INDEX_OFFLINE_MODE,
  PROPERTY_INDEX_OPERATOR_CODE,
  PROPERTY_INDEX_OPERATOR_NAME,
  PROPERTY_INDEX_PASSPHRASE,
  PROPERTY_INDEX_PASSPHRASE_REQUIRED,
  PROPERTY_INDEX_PAYMENT_URL,
  PROPERTY_INDEX_PORTAL_URL,
  PROPERTY_INDEX_POWERED,
  PROPERTY_INDEX_PRIORITY,
  PROPERTY_INDEX_PRL_VERSION,
  PROPERTY_INDEX_PROFILE,
  PROPERTY_INDEX_PROFILES,
  PROPERTY_INDEX_PROVIDER,
  PROPERTY_INDEX_PROXY_CONFIG,
  PROPERTY_INDEX_ROAMING_STATE,
  PROPERTY_INDEX_SAVE_CREDENTIALS,
  PROPERTY_INDEX_SCANNING,
  PROPERTY_INDEX_SECURITY,
  PROPERTY_INDEX_SELECTED_NETWORK,
  PROPERTY_INDEX_SERVICES,
  PROPERTY_INDEX_SERVICE_WATCH_LIST,
  PROPERTY_INDEX_SERVING_OPERATOR,
  PROPERTY_INDEX_SIGNAL_STRENGTH,
  PROPERTY_INDEX_SIM_LOCK,
  PROPERTY_INDEX_STATE,
  PROPERTY_INDEX_SUPPORT_NETWORK_SCAN,
  PROPERTY_INDEX_TECHNOLOGY_FAMILY,
  PROPERTY_INDEX_TYPE,
  PROPERTY_INDEX_UNKNOWN,
  PROPERTY_INDEX_USAGE_URL,
  PROPERTY_INDEX_WIFI_AUTH_MODE,
  PROPERTY_INDEX_WIFI_FREQUENCY,
  PROPERTY_INDEX_WIFI_HEX_SSID,
  PROPERTY_INDEX_WIFI_HIDDEN_SSID,
  PROPERTY_INDEX_WIFI_PHY_MODE,
};

StringToEnum<PropertyIndex>::Pair property_index_table[] =  {
  { kActivationStateProperty, PROPERTY_INDEX_ACTIVATION_STATE },
  { kActiveProfileProperty, PROPERTY_INDEX_ACTIVE_PROFILE },
  { kAutoConnectProperty, PROPERTY_INDEX_AUTO_CONNECT },
  { kAvailableTechnologiesProperty, PROPERTY_INDEX_AVAILABLE_TECHNOLOGIES },
  { kCarrierProperty, PROPERTY_INDEX_CARRIER },
  { kCellularAllowRoamingProperty, PROPERTY_INDEX_CELLULAR_ALLOW_ROAMING },
  { kCellularApnListProperty, PROPERTY_INDEX_CELLULAR_APN_LIST },
  { kCellularApnProperty, PROPERTY_INDEX_CELLULAR_APN },
  { kCellularLastGoodApnProperty, PROPERTY_INDEX_CELLULAR_LAST_GOOD_APN },
  { kCheckPortalListProperty, PROPERTY_INDEX_CHECK_PORTAL_LIST },
  { kConnectableProperty, PROPERTY_INDEX_CONNECTABLE },
  { kConnectedTechnologiesProperty, PROPERTY_INDEX_CONNECTED_TECHNOLOGIES },
  { kDefaultTechnologyProperty, PROPERTY_INDEX_DEFAULT_TECHNOLOGY },
  { kDeviceProperty, PROPERTY_INDEX_DEVICE },
  { kDevicesProperty, PROPERTY_INDEX_DEVICES },
  { kEapAnonymousIdentityProperty, PROPERTY_INDEX_EAP_ANONYMOUS_IDENTITY },
  { kEapCaCertIDProperty, PROPERTY_INDEX_EAP_CA_CERT_ID },
  { kEapCaCertNssProperty, PROPERTY_INDEX_EAP_CA_CERT_NSS },
  { kEapCaCertProperty, PROPERTY_INDEX_EAP_CA_CERT },
  { kEapCertIDProperty, PROPERTY_INDEX_EAP_CERT_ID },
  { kEapClientCertNssProperty, PROPERTY_INDEX_EAP_CLIENT_CERT_NSS },
  { kEapClientCertProperty, PROPERTY_INDEX_EAP_CLIENT_CERT },
  { kEapIdentityProperty, PROPERTY_INDEX_EAP_IDENTITY },
  { kEapKeyIDProperty, PROPERTY_INDEX_EAP_KEY_ID },
  { kEapKeyMgmtProperty, PROPERTY_INDEX_EAP_KEY_MGMT },
  { kEapMethodProperty, PROPERTY_INDEX_EAP_METHOD },
  { kEapPasswordProperty, PROPERTY_INDEX_EAP_PASSWORD },
  { kEapPhase2AuthProperty, PROPERTY_INDEX_EAP_PHASE_2_AUTH },
  { kEapPinProperty, PROPERTY_INDEX_EAP_PIN },
  { kEapPrivateKeyPasswordProperty, PROPERTY_INDEX_EAP_PRIVATE_KEY_PASSWORD },
  { kEapPrivateKeyProperty, PROPERTY_INDEX_EAP_PRIVATE_KEY },
  { kEapUseSystemCAsProperty, PROPERTY_INDEX_EAP_USE_SYSTEM_CAS },
  { kEnabledTechnologiesProperty, PROPERTY_INDEX_ENABLED_TECHNOLOGIES },
  { kErrorProperty, PROPERTY_INDEX_ERROR },
  { kEsnProperty, PROPERTY_INDEX_ESN },
  { kFavoriteProperty, PROPERTY_INDEX_FAVORITE },
  { kFirmwareRevisionProperty, PROPERTY_INDEX_FIRMWARE_REVISION },
  { kFoundNetworksProperty, PROPERTY_INDEX_FOUND_NETWORKS },
  { kHardwareRevisionProperty, PROPERTY_INDEX_HARDWARE_REVISION },
  { kHomeProviderProperty, PROPERTY_INDEX_HOME_PROVIDER },
  { kHostProperty, PROPERTY_INDEX_HOST },
  { kIdentityProperty, PROPERTY_INDEX_IDENTITY },
  { kImeiProperty, PROPERTY_INDEX_IMEI },
  { kImsiProperty, PROPERTY_INDEX_IMSI },
  { kIsActiveProperty, PROPERTY_INDEX_IS_ACTIVE },
  { kL2TPIPSecCACertNSSProperty, PROPERTY_INDEX_L2TPIPSEC_CA_CERT_NSS },
  { kL2TPIPSecClientCertIDProperty, PROPERTY_INDEX_L2TPIPSEC_CLIENT_CERT_ID },
  { kL2TPIPSecClientCertSlotProp, PROPERTY_INDEX_L2TPIPSEC_CLIENT_CERT_SLOT },
  { kL2TPIPSecPasswordProperty, PROPERTY_INDEX_L2TPIPSEC_PASSWORD },
  { kL2TPIPSecPINProperty, PROPERTY_INDEX_L2TPIPSEC_PIN },
  { kL2TPIPSecPSKProperty, PROPERTY_INDEX_L2TPIPSEC_PSK },
  { kL2TPIPSecUserProperty, PROPERTY_INDEX_L2TPIPSEC_USER },
  { kManufacturerProperty, PROPERTY_INDEX_MANUFACTURER },
  { kMdnProperty, PROPERTY_INDEX_MDN },
  { kMeidProperty, PROPERTY_INDEX_MEID },
  { kMinProperty, PROPERTY_INDEX_MIN },
  { kModeProperty, PROPERTY_INDEX_MODE },
  { kModelIDProperty, PROPERTY_INDEX_MODEL_ID },
  { kNameProperty, PROPERTY_INDEX_NAME },
  { kNetworkTechnologyProperty, PROPERTY_INDEX_NETWORK_TECHNOLOGY },
  { kNetworksProperty, PROPERTY_INDEX_NETWORKS },
  { kOfflineModeProperty, PROPERTY_INDEX_OFFLINE_MODE },
  { kOperatorCodeProperty, PROPERTY_INDEX_OPERATOR_CODE },
  { kOperatorNameProperty, PROPERTY_INDEX_OPERATOR_NAME },
  { kPRLVersionProperty, PROPERTY_INDEX_PRL_VERSION },
  { kPassphraseProperty, PROPERTY_INDEX_PASSPHRASE },
  { kPassphraseRequiredProperty, PROPERTY_INDEX_PASSPHRASE_REQUIRED },
  { kPaymentURLProperty, PROPERTY_INDEX_PAYMENT_URL },
  { kPortalURLProperty, PROPERTY_INDEX_PORTAL_URL },
  { kPoweredProperty, PROPERTY_INDEX_POWERED },
  { kPriorityProperty, PROPERTY_INDEX_PRIORITY },
  { kProfileProperty, PROPERTY_INDEX_PROFILE },
  { kProfilesProperty, PROPERTY_INDEX_PROFILES },
  { kProviderProperty, PROPERTY_INDEX_PROVIDER },
  { kProxyConfigProperty, PROPERTY_INDEX_PROXY_CONFIG },
  { kRoamingStateProperty, PROPERTY_INDEX_ROAMING_STATE },
  { kSIMLockStatusProperty, PROPERTY_INDEX_SIM_LOCK },
  { kSaveCredentialsProperty, PROPERTY_INDEX_SAVE_CREDENTIALS },
  { kScanningProperty, PROPERTY_INDEX_SCANNING },
  { kSecurityProperty, PROPERTY_INDEX_SECURITY },
  { kSelectedNetworkProperty, PROPERTY_INDEX_SELECTED_NETWORK },
  { kServiceWatchListProperty, PROPERTY_INDEX_SERVICE_WATCH_LIST },
  { kServicesProperty, PROPERTY_INDEX_SERVICES },
  { kServingOperatorProperty, PROPERTY_INDEX_SERVING_OPERATOR },
  { kSignalStrengthProperty, PROPERTY_INDEX_SIGNAL_STRENGTH },
  { kStateProperty, PROPERTY_INDEX_STATE },
  { kSupportNetworkScanProperty, PROPERTY_INDEX_SUPPORT_NETWORK_SCAN },
  { kTechnologyFamilyProperty, PROPERTY_INDEX_TECHNOLOGY_FAMILY },
  { kTypeProperty, PROPERTY_INDEX_TYPE },
  { kUsageURLProperty, PROPERTY_INDEX_USAGE_URL },
  { kWifiAuthMode, PROPERTY_INDEX_WIFI_AUTH_MODE },
  { kWifiFrequency, PROPERTY_INDEX_WIFI_FREQUENCY },
  { kWifiHexSsid, PROPERTY_INDEX_WIFI_HEX_SSID },
  { kWifiHiddenSsid, PROPERTY_INDEX_WIFI_HIDDEN_SSID },
  { kWifiPhyMode, PROPERTY_INDEX_WIFI_PHY_MODE },
};

StringToEnum<PropertyIndex>& property_index_parser() {
  static StringToEnum<PropertyIndex> parser(property_index_table,
                                            arraysize(property_index_table),
                                            PROPERTY_INDEX_UNKNOWN);
  return parser;
}

////////////////////////////////////////////////////////////////////////////
// Parse strings from libcros.

// Network.
static ConnectionType ParseType(const std::string& type) {
  static StringToEnum<ConnectionType>::Pair table[] = {
    { kTypeEthernet, TYPE_ETHERNET },
    { kTypeWifi, TYPE_WIFI },
    { kTypeWimax, TYPE_WIMAX },
    { kTypeBluetooth, TYPE_BLUETOOTH },
    { kTypeCellular, TYPE_CELLULAR },
    { kTypeVPN, TYPE_VPN },
  };
  static StringToEnum<ConnectionType> parser(
      table, arraysize(table), TYPE_UNKNOWN);
  return parser.Get(type);
}

ConnectionType ParseTypeFromDictionary(const DictionaryValue* info) {
  std::string type_string;
  info->GetString(kTypeProperty, &type_string);
  return ParseType(type_string);
}

static ConnectionMode ParseMode(const std::string& mode) {
  static StringToEnum<ConnectionMode>::Pair table[] = {
    { kModeManaged, MODE_MANAGED },
    { kModeAdhoc, MODE_ADHOC },
  };
  static StringToEnum<ConnectionMode> parser(
      table, arraysize(table), MODE_UNKNOWN);
  return parser.Get(mode);
}

static ConnectionState ParseState(const std::string& state) {
  static StringToEnum<ConnectionState>::Pair table[] = {
    { kStateIdle, STATE_IDLE },
    { kStateCarrier, STATE_CARRIER },
    { kStateAssociation, STATE_ASSOCIATION },
    { kStateConfiguration, STATE_CONFIGURATION },
    { kStateReady, STATE_READY },
    { kStateDisconnect, STATE_DISCONNECT },
    { kStateFailure, STATE_FAILURE },
    { kStateActivationFailure, STATE_ACTIVATION_FAILURE },
    { kStatePortal, STATE_PORTAL },
    { kStateOnline, STATE_ONLINE },
  };
  static StringToEnum<ConnectionState> parser(
      table, arraysize(table), STATE_UNKNOWN);
  return parser.Get(state);
}

static ConnectionError ParseError(const std::string& error) {
  static StringToEnum<ConnectionError>::Pair table[] = {
    { kErrorOutOfRange, ERROR_OUT_OF_RANGE },
    { kErrorPinMissing, ERROR_PIN_MISSING },
    { kErrorDhcpFailed, ERROR_DHCP_FAILED },
    { kErrorConnectFailed, ERROR_CONNECT_FAILED },
    { kErrorBadPassphrase, ERROR_BAD_PASSPHRASE },
    { kErrorBadWEPKey, ERROR_BAD_WEPKEY },
    { kErrorActivationFailed, ERROR_ACTIVATION_FAILED },
    { kErrorNeedEvdo, ERROR_NEED_EVDO },
    { kErrorNeedHomeNetwork, ERROR_NEED_HOME_NETWORK },
    { kErrorOtaspFailed, ERROR_OTASP_FAILED },
    { kErrorAaaFailed, ERROR_AAA_FAILED },
    { kErrorInternal, ERROR_INTERNAL },
    { kErrorDNSLookupFailed, ERROR_DNS_LOOKUP_FAILED },
    { kErrorHTTPGetFailed, ERROR_HTTP_GET_FAILED },
  };
  static StringToEnum<ConnectionError> parser(
      table, arraysize(table), ERROR_NO_ERROR);
  return parser.Get(error);
}

// VirtualNetwork
static VirtualNetwork::ProviderType ParseProviderType(const std::string& mode) {
  static StringToEnum<VirtualNetwork::ProviderType>::Pair table[] = {
    { kProviderL2tpIpsec, VirtualNetwork::PROVIDER_TYPE_L2TP_IPSEC_PSK },
    { kProviderOpenVpn, VirtualNetwork::PROVIDER_TYPE_OPEN_VPN },
  };
  static StringToEnum<VirtualNetwork::ProviderType> parser(
      table, arraysize(table), VirtualNetwork::PROVIDER_TYPE_MAX);
  return parser.Get(mode);
}

// CellularNetwork.
static ActivationState ParseActivationState(const std::string& state) {
  static StringToEnum<ActivationState>::Pair table[] = {
    { kActivationStateActivated, ACTIVATION_STATE_ACTIVATED },
    { kActivationStateActivating, ACTIVATION_STATE_ACTIVATING },
    { kActivationStateNotActivated, ACTIVATION_STATE_NOT_ACTIVATED },
    { kActivationStatePartiallyActivated, ACTIVATION_STATE_PARTIALLY_ACTIVATED},
    { kActivationStateUnknown, ACTIVATION_STATE_UNKNOWN},
  };
  static StringToEnum<ActivationState> parser(
      table, arraysize(table), ACTIVATION_STATE_UNKNOWN);
  return parser.Get(state);
}

static NetworkTechnology ParseNetworkTechnology(const std::string& technology) {
  static StringToEnum<NetworkTechnology>::Pair table[] = {
    { kNetworkTechnology1Xrtt, NETWORK_TECHNOLOGY_1XRTT },
    { kNetworkTechnologyEvdo, NETWORK_TECHNOLOGY_EVDO },
    { kNetworkTechnologyGprs, NETWORK_TECHNOLOGY_GPRS },
    { kNetworkTechnologyEdge, NETWORK_TECHNOLOGY_EDGE },
    { kNetworkTechnologyUmts, NETWORK_TECHNOLOGY_UMTS },
    { kNetworkTechnologyHspa, NETWORK_TECHNOLOGY_HSPA },
    { kNetworkTechnologyHspaPlus, NETWORK_TECHNOLOGY_HSPA_PLUS },
    { kNetworkTechnologyLte, NETWORK_TECHNOLOGY_LTE },
    { kNetworkTechnologyLteAdvanced, NETWORK_TECHNOLOGY_LTE_ADVANCED },
    { kNetworkTechnologyGsm, NETWORK_TECHNOLOGY_GSM },
  };
  static StringToEnum<NetworkTechnology> parser(
      table, arraysize(table), NETWORK_TECHNOLOGY_UNKNOWN);
  return parser.Get(technology);
}

static SIMLockState ParseSimLockState(const std::string& state) {
  static StringToEnum<SIMLockState>::Pair table[] = {
    { "", SIM_UNLOCKED },
    { kSIMLockPin, SIM_LOCKED_PIN },
    { kSIMLockPuk, SIM_LOCKED_PUK },
  };
  static StringToEnum<SIMLockState> parser(
      table, arraysize(table), SIM_UNKNOWN);
  SIMLockState parsed_state = parser.Get(state);
  DCHECK_NE(parsed_state, SIM_UNKNOWN) << "Unknown SIMLock state encountered";
  return parsed_state;
}

static bool ParseSimLockStateFromDictionary(const DictionaryValue* info,
                                            SIMLockState* out_state,
                                            int* out_retries) {
  std::string state_string;
  if (!info->GetString(kSIMLockTypeProperty, &state_string) ||
      !info->GetInteger(kSIMLockRetriesLeftProperty, out_retries)) {
    LOG(ERROR) << "Error parsing SIMLock state";
    return false;
  }
  *out_state = ParseSimLockState(state_string);
  return true;
}

static bool ParseFoundNetworksFromList(const ListValue* list,
                                       CellularNetworkList* found_networks_) {
  found_networks_->clear();
  found_networks_->reserve(list->GetSize());
  for (ListValue::const_iterator it = list->begin(); it != list->end(); ++it) {
    if ((*it)->IsType(Value::TYPE_DICTIONARY)) {
      found_networks_->resize(found_networks_->size() + 1);
      const DictionaryValue* dict = static_cast<const DictionaryValue*>(*it);
      dict->GetStringWithoutPathExpansion(
          kStatusProperty, &found_networks_->back().status);
      dict->GetStringWithoutPathExpansion(
          kNetworkIdProperty, &found_networks_->back().network_id);
      dict->GetStringWithoutPathExpansion(
          kShortNameProperty, &found_networks_->back().short_name);
      dict->GetStringWithoutPathExpansion(
          kLongNameProperty, &found_networks_->back().long_name);
      dict->GetStringWithoutPathExpansion(
          kTechnologyProperty, &found_networks_->back().technology);
    } else {
      return false;
    }
  }
  return true;
}

static bool ParseApnList(const ListValue* list,
                         CellularApnList* apn_list) {
  apn_list->clear();
  apn_list->reserve(list->GetSize());
  for (ListValue::const_iterator it = list->begin(); it != list->end(); ++it) {
    if ((*it)->IsType(Value::TYPE_DICTIONARY)) {
      apn_list->resize(apn_list->size() + 1);
      apn_list->back().Set(*static_cast<const DictionaryValue*>(*it));
    } else {
      return false;
    }
  }
  return true;
}

static NetworkRoamingState ParseRoamingState(const std::string& roaming_state) {
  static StringToEnum<NetworkRoamingState>::Pair table[] = {
    { kRoamingStateHome, ROAMING_STATE_HOME },
    { kRoamingStateRoaming, ROAMING_STATE_ROAMING },
    { kRoamingStateUnknown, ROAMING_STATE_UNKNOWN },
  };
  static StringToEnum<NetworkRoamingState> parser(
      table, arraysize(table), ROAMING_STATE_UNKNOWN);
  return parser.Get(roaming_state);
}

// WifiNetwork
static ConnectionSecurity ParseSecurity(const std::string& security) {
  static StringToEnum<ConnectionSecurity>::Pair table[] = {
    { kSecurityNone, SECURITY_NONE },
    { kSecurityWep, SECURITY_WEP },
    { kSecurityWpa, SECURITY_WPA },
    { kSecurityRsn, SECURITY_RSN },
    { kSecurityPsk, SECURITY_PSK },
    { kSecurity8021x, SECURITY_8021X },
  };
  static StringToEnum<ConnectionSecurity> parser(
      table, arraysize(table), SECURITY_UNKNOWN);
  return parser.Get(security);
}

static EAPMethod ParseEAPMethod(const std::string& method) {
  static StringToEnum<EAPMethod>::Pair table[] = {
    { kEapMethodPEAP, EAP_METHOD_PEAP },
    { kEapMethodTLS, EAP_METHOD_TLS },
    { kEapMethodTTLS, EAP_METHOD_TTLS },
    { kEapMethodLEAP, EAP_METHOD_LEAP },
  };
  static StringToEnum<EAPMethod> parser(
      table, arraysize(table), EAP_METHOD_UNKNOWN);
  return parser.Get(method);
}

static EAPPhase2Auth ParseEAPPhase2Auth(const std::string& auth) {
  static StringToEnum<EAPPhase2Auth>::Pair table[] = {
    { kEapPhase2AuthPEAPMD5, EAP_PHASE_2_AUTH_MD5 },
    { kEapPhase2AuthPEAPMSCHAPV2, EAP_PHASE_2_AUTH_MSCHAPV2 },
    { kEapPhase2AuthTTLSMD5, EAP_PHASE_2_AUTH_MD5 },
    { kEapPhase2AuthTTLSMSCHAPV2, EAP_PHASE_2_AUTH_MSCHAPV2 },
    { kEapPhase2AuthTTLSMSCHAP, EAP_PHASE_2_AUTH_MSCHAP },
    { kEapPhase2AuthTTLSPAP, EAP_PHASE_2_AUTH_PAP },
    { kEapPhase2AuthTTLSCHAP, EAP_PHASE_2_AUTH_CHAP },
  };
  static StringToEnum<EAPPhase2Auth> parser(
      table, arraysize(table), EAP_PHASE_2_AUTH_AUTO);
  return parser.Get(auth);
}

// NetworkDevice
static TechnologyFamily ParseTechnologyFamily(const std::string& family) {
  static StringToEnum<TechnologyFamily>::Pair table[] = {
    { kTechnologyFamilyCdma, TECHNOLOGY_FAMILY_CDMA },
    { kTechnologyFamilyGsm, TECHNOLOGY_FAMILY_GSM },
  };
  static StringToEnum<TechnologyFamily> parser(
      table, arraysize(table), TECHNOLOGY_FAMILY_UNKNOWN);
  return parser.Get(family);
}

////////////////////////////////////////////////////////////////////////////////
// Misc.

// Safe string constructor since we can't rely on non NULL pointers
// for string values from libcros.
static std::string SafeString(const char* s) {
  return s ? std::string(s) : std::string();
}

// Erase the memory used by a string, then clear it.
static void WipeString(std::string* str) {
  str->assign(str->size(), '\0');
  str->clear();
}

static bool EnsureCrosLoaded() {
  if (!CrosLibrary::Get()->EnsureLoaded() ||
      !CrosLibrary::Get()->GetNetworkLibrary()->IsCros()) {
    return false;
  } else {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI))
        << "chromeos_network calls made from non UI thread!";
    return true;
  }
}

static void ValidateUTF8(const std::string& str, std::string* output) {
  output->clear();

  for (int32 index = 0; index < static_cast<int32>(str.size()); ++index) {
    uint32 code_point_out;
    bool is_unicode_char = base::ReadUnicodeCharacter(str.c_str(), str.size(),
                                                      &index, &code_point_out);
    if (is_unicode_char && (code_point_out >= 0x20))
      base::WriteUnicodeCharacter(code_point_out, output);
    else
      // Puts REPLACEMENT CHARACTER (U+FFFD) if character is not readable UTF-8
      base::WriteUnicodeCharacter(0xFFFD, output);
  }
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// FoundCellularNetwork

FoundCellularNetwork::FoundCellularNetwork() {}

FoundCellularNetwork::~FoundCellularNetwork() {}

////////////////////////////////////////////////////////////////////////////////
// NetworkDevice

NetworkDevice::NetworkDevice(const std::string& device_path)
    : device_path_(device_path),
      type_(TYPE_UNKNOWN),
      scanning_(false),
      sim_lock_state_(SIM_UNKNOWN),
      sim_retries_left_(kDefaultSimUnlockRetriesCount),
      sim_pin_required_(SIM_PIN_REQUIRE_UNKNOWN),
      PRL_version_(0),
      data_roaming_allowed_(false),
      support_network_scan_(false) {
}

NetworkDevice::~NetworkDevice() {}

bool NetworkDevice::ParseValue(int index, const Value* value) {
  switch (index) {
    case PROPERTY_INDEX_TYPE: {
      std::string type_string;
      if (value->GetAsString(&type_string)) {
        type_ = ParseType(type_string);
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_NAME:
      return value->GetAsString(&name_);
    case PROPERTY_INDEX_CARRIER:
      return value->GetAsString(&carrier_);
    case PROPERTY_INDEX_SCANNING:
      return value->GetAsBoolean(&scanning_);
    case PROPERTY_INDEX_CELLULAR_ALLOW_ROAMING:
      return value->GetAsBoolean(&data_roaming_allowed_);
    case PROPERTY_INDEX_CELLULAR_APN_LIST:
      if (value->IsType(Value::TYPE_LIST)) {
        return ParseApnList(static_cast<const ListValue*>(value),
                            &provider_apn_list_);
      }
      break;
    case PROPERTY_INDEX_NETWORKS:
      if (value->IsType(Value::TYPE_LIST)) {
        // Ignored.
        return true;
      }
      break;
    case PROPERTY_INDEX_FOUND_NETWORKS:
      if (value->IsType(Value::TYPE_LIST)) {
        return ParseFoundNetworksFromList(
            static_cast<const ListValue*>(value),
            &found_cellular_networks_);
      }
      break;
    case PROPERTY_INDEX_HOME_PROVIDER: {
      if (value->IsType(Value::TYPE_DICTIONARY)) {
        const DictionaryValue* dict =
            static_cast<const DictionaryValue*>(value);
        home_provider_code_.clear();
        home_provider_country_.clear();
        home_provider_name_.clear();
        dict->GetStringWithoutPathExpansion(kOperatorCodeKey,
                                            &home_provider_code_);
        dict->GetStringWithoutPathExpansion(kOperatorCountryKey,
                                            &home_provider_country_);
        dict->GetStringWithoutPathExpansion(kOperatorNameKey,
                                            &home_provider_name_);
        if (!home_provider_name_.empty() && !home_provider_country_.empty()) {
          home_provider_id_ = base::StringPrintf(
              kCarrierIdFormat,
              home_provider_name_.c_str(),
              home_provider_country_.c_str());
        } else {
          home_provider_id_ = home_provider_code_;
          LOG(WARNING) << "Carrier ID not defined, using code instead: "
                       << home_provider_id_;
        }
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_MEID:
      return value->GetAsString(&MEID_);
    case PROPERTY_INDEX_IMEI:
      return value->GetAsString(&IMEI_);
    case PROPERTY_INDEX_IMSI:
      return value->GetAsString(&IMSI_);
    case PROPERTY_INDEX_ESN:
      return value->GetAsString(&ESN_);
    case PROPERTY_INDEX_MDN:
      return value->GetAsString(&MDN_);
    case PROPERTY_INDEX_MIN:
      return value->GetAsString(&MIN_);
    case PROPERTY_INDEX_MODEL_ID:
      return value->GetAsString(&model_id_);
    case PROPERTY_INDEX_MANUFACTURER:
      return value->GetAsString(&manufacturer_);
    case PROPERTY_INDEX_SIM_LOCK:
      if (value->IsType(Value::TYPE_DICTIONARY)) {
        bool result = ParseSimLockStateFromDictionary(
            static_cast<const DictionaryValue*>(value),
            &sim_lock_state_,
            &sim_retries_left_);
        // Initialize PinRequired value only once.
        // See SIMPinRequire enum comments.
        if (sim_pin_required_ == SIM_PIN_REQUIRE_UNKNOWN) {
          if (sim_lock_state_ == SIM_UNLOCKED) {
            sim_pin_required_ = SIM_PIN_NOT_REQUIRED;
          } else if (sim_lock_state_ == SIM_LOCKED_PIN ||
                     sim_lock_state_ == SIM_LOCKED_PUK) {
            sim_pin_required_ = SIM_PIN_REQUIRED;
          }
        }
        return result;
      }
      break;
    case PROPERTY_INDEX_FIRMWARE_REVISION:
      return value->GetAsString(&firmware_revision_);
    case PROPERTY_INDEX_HARDWARE_REVISION:
      return value->GetAsString(&hardware_revision_);
    case PROPERTY_INDEX_POWERED:
      // we don't care about the value, just the fact that it changed
      return true;
    case PROPERTY_INDEX_PRL_VERSION:
      return value->GetAsInteger(&PRL_version_);
    case PROPERTY_INDEX_SELECTED_NETWORK:
      return value->GetAsString(&selected_cellular_network_);
    case PROPERTY_INDEX_SUPPORT_NETWORK_SCAN:
      return value->GetAsBoolean(&support_network_scan_);
    case PROPERTY_INDEX_TECHNOLOGY_FAMILY: {
      std::string technology_family_string;
      if (value->GetAsString(&technology_family_string)) {
        technology_family_ = ParseTechnologyFamily(technology_family_string);
        return true;
      }
      break;
    }
    default:
      break;
  }
  return false;
}

void NetworkDevice::ParseInfo(const DictionaryValue* info) {
  for (DictionaryValue::key_iterator iter = info->begin_keys();
       iter != info->end_keys(); ++iter) {
    const std::string& key = *iter;
    Value* value;
    bool res = info->GetWithoutPathExpansion(key, &value);
    DCHECK(res);
    if (res) {
      int index = property_index_parser().Get(key);
      if (!ParseValue(index, value))
        VLOG(1) << "NetworkDevice: Unhandled key: " << key;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// Network

Network::Network(const std::string& service_path, ConnectionType type)
    : state_(STATE_UNKNOWN),
      error_(ERROR_NO_ERROR),
      connectable_(true),
      is_active_(false),
      priority_(kPriorityNotSet),
      auto_connect_(false),
      save_credentials_(false),
      priority_order_(0),
      added_(false),
      notify_failure_(false),
      profile_type_(PROFILE_NONE),
      service_path_(service_path),
      type_(type) {
}

Network::~Network() {}

void Network::SetState(ConnectionState new_state) {
  if (new_state == state_)
    return;
  ConnectionState old_state = state_;
  state_ = new_state;
  if (new_state == STATE_FAILURE) {
    if (old_state != STATE_UNKNOWN) {
      // New failure, the user needs to be notified.
      notify_failure_ = true;
    }
  } else {
    // State changed, so refresh IP address.
    // Note: blocking DBus call. TODO(stevenjb): refactor this.
    InitIPAddress();
  }
  VLOG(1) << name() << ".State = " << GetStateString();
}

void Network::SetName(const std::string& name) {
  std::string name_utf8;
  ValidateUTF8(name, &name_utf8);
  set_name(name_utf8);
}

bool Network::ParseValue(int index, const Value* value) {
  switch (index) {
    case PROPERTY_INDEX_TYPE: {
      std::string type_string;
      if (value->GetAsString(&type_string)) {
        ConnectionType type = ParseType(type_string);
        LOG_IF(ERROR, type != type_)
            << "Network with mismatched type: " << service_path_
            << " " << type << " != " << type_;
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_DEVICE:
      return value->GetAsString(&device_path_);
    case PROPERTY_INDEX_NAME: {
      std::string name;
      if (value->GetAsString(&name)) {
        SetName(name);
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_PROFILE:
      // Note: currently this is only provided for non remembered networks.
      return value->GetAsString(&profile_path_);
    case PROPERTY_INDEX_STATE: {
      std::string state_string;
      if (value->GetAsString(&state_string)) {
        SetState(ParseState(state_string));
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_MODE: {
      std::string mode_string;
      if (value->GetAsString(&mode_string)) {
        mode_ = ParseMode(mode_string);
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_ERROR: {
      std::string error_string;
      if (value->GetAsString(&error_string)) {
        error_ = ParseError(error_string);
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_CONNECTABLE:
      return value->GetAsBoolean(&connectable_);
    case PROPERTY_INDEX_IS_ACTIVE:
      return value->GetAsBoolean(&is_active_);
    case PROPERTY_INDEX_FAVORITE:
      // Not used currently.
      return true;
    case PROPERTY_INDEX_PRIORITY:
      return value->GetAsInteger(&priority_);
    case PROPERTY_INDEX_AUTO_CONNECT:
      return value->GetAsBoolean(&auto_connect_);
    case PROPERTY_INDEX_SAVE_CREDENTIALS:
      return value->GetAsBoolean(&save_credentials_);
    case PROPERTY_INDEX_PROXY_CONFIG:
      return value->GetAsString(&proxy_config_);
    default:
      break;
  }
  return false;
}

void Network::ParseInfo(const DictionaryValue* info) {
  // Set default values for properties that may not exist in the dictionary.
  priority_ = kPriorityNotSet;

  for (DictionaryValue::key_iterator iter = info->begin_keys();
       iter != info->end_keys(); ++iter) {
    const std::string& key = *iter;
    Value* value;
    bool res = info->GetWithoutPathExpansion(key, &value);
    DCHECK(res);
    if (res) {
      int index = property_index_parser().Get(key);
      if (!ParseValue(index, value))  // virtual.
        VLOG(1) << "Network: " << name()
                << " Type: " << ConnectionTypeToString(type())
                << " Unhandled key: " << key;
    }
  }
  CalculateUniqueId();
}

void Network::EraseCredentials() {
}

void Network::CalculateUniqueId() {
  unique_id_ = name_;
}

bool Network::RequiresUserProfile() const {
  return false;
}

void Network::CopyCredentialsFromRemembered(Network* remembered) {
}

void Network::SetValueProperty(const char* prop, Value* val) {
  DCHECK(prop);
  DCHECK(val);
  if (!EnsureCrosLoaded())
    return;
  chromeos::SetNetworkServiceProperty(service_path_.c_str(), prop, val);
}

void Network::ClearProperty(const char* prop) {
  DCHECK(prop);
  if (!EnsureCrosLoaded())
    return;
  chromeos::ClearNetworkServiceProperty(service_path_.c_str(), prop);
}

void Network::SetStringProperty(
    const char* prop, const std::string& str, std::string* dest) {
  if (dest)
    *dest = str;
  scoped_ptr<Value> value(Value::CreateStringValue(str));
  SetValueProperty(prop, value.get());
}

void Network::SetOrClearStringProperty(const char* prop,
                                       const std::string& str,
                                       std::string* dest) {
  if (str.empty()) {
    ClearProperty(prop);
    if (dest)
      dest->clear();
  } else {
    SetStringProperty(prop, str, dest);
  }
}

void Network::SetBooleanProperty(const char* prop, bool b, bool* dest) {
  if (dest)
    *dest = b;
  scoped_ptr<Value> value(Value::CreateBooleanValue(b));
  SetValueProperty(prop, value.get());
}

void Network::SetIntegerProperty(const char* prop, int i, int* dest) {
  if (dest)
    *dest = i;
  scoped_ptr<Value> value(Value::CreateIntegerValue(i));
  SetValueProperty(prop, value.get());
}

void Network::SetPreferred(bool preferred) {
  if (preferred) {
    SetIntegerProperty(kPriorityProperty, kPriorityPreferred, &priority_);
  } else {
    ClearProperty(kPriorityProperty);
    priority_ = kPriorityNotSet;
  }
}

void Network::SetAutoConnect(bool auto_connect) {
  SetBooleanProperty(kAutoConnectProperty, auto_connect, &auto_connect_);
}

void Network::SetSaveCredentials(bool save_credentials) {
  SetBooleanProperty(kSaveCredentialsProperty, save_credentials,
                     &save_credentials_);
}

void Network::SetProfilePath(const std::string& profile_path) {
  VLOG(1) << "Setting profile for: " << name_ << " to: " << profile_path;
  SetOrClearStringProperty(kProfileProperty, profile_path, &profile_path_);
}

std::string Network::GetStateString() const {
  switch (state_) {
    case STATE_UNKNOWN:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_UNKNOWN);
    case STATE_IDLE:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_IDLE);
    case STATE_CARRIER:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_CARRIER);
    case STATE_ASSOCIATION:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_ASSOCIATION);
    case STATE_CONFIGURATION:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_CONFIGURATION);
    case STATE_READY:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_READY);
    case STATE_DISCONNECT:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_DISCONNECT);
    case STATE_FAILURE:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_FAILURE);
    case STATE_ACTIVATION_FAILURE:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_STATE_ACTIVATION_FAILURE);
    case STATE_PORTAL:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_PORTAL);
    case STATE_ONLINE:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_ONLINE);
  }
  return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_UNRECOGNIZED);
}

std::string Network::GetErrorString() const {
  switch (error_) {
    case ERROR_NO_ERROR:
      // TODO(nkostylev): Introduce new error message "None" instead.
      return std::string();
    case ERROR_OUT_OF_RANGE:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_ERROR_OUT_OF_RANGE);
    case ERROR_PIN_MISSING:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_ERROR_PIN_MISSING);
    case ERROR_DHCP_FAILED:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_ERROR_DHCP_FAILED);
    case ERROR_CONNECT_FAILED:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ERROR_CONNECT_FAILED);
    case ERROR_BAD_PASSPHRASE:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ERROR_BAD_PASSPHRASE);
    case ERROR_BAD_WEPKEY:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_ERROR_BAD_WEPKEY);
    case ERROR_ACTIVATION_FAILED:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ERROR_ACTIVATION_FAILED);
    case ERROR_NEED_EVDO:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_ERROR_NEED_EVDO);
    case ERROR_NEED_HOME_NETWORK:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ERROR_NEED_HOME_NETWORK);
    case ERROR_OTASP_FAILED:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_ERROR_OTASP_FAILED);
    case ERROR_AAA_FAILED:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_ERROR_AAA_FAILED);
    case ERROR_INTERNAL:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_ERROR_INTERNAL);
    case ERROR_DNS_LOOKUP_FAILED:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ERROR_DNS_LOOKUP_FAILED);
    case ERROR_HTTP_GET_FAILED:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ERROR_HTTP_GET_FAILED);
  }
  return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_UNRECOGNIZED);
}

void Network::SetProxyConfig(const std::string& proxy_config) {
  SetStringProperty(kProxyConfigProperty, proxy_config, &proxy_config_);
}

void Network::InitIPAddress() {
  ip_address_.clear();
  if (!EnsureCrosLoaded())
    return;
  // If connected, get ip config.
  if (connected() && !device_path_.empty()) {
    IPConfigStatus* ipconfig_status =
        chromeos::ListIPConfigs(device_path_.c_str());
    if (ipconfig_status) {
      for (int i = 0; i < ipconfig_status->size; ++i) {
        IPConfig ipconfig = ipconfig_status->ips[i];
        if (strlen(ipconfig.address) > 0) {
          ip_address_ = ipconfig.address;
          break;
        }
      }
      chromeos::FreeIPConfigStatus(ipconfig_status);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
// VirtualNetwork

VirtualNetwork::VirtualNetwork(const std::string& service_path)
    : Network(service_path, TYPE_VPN),
      provider_type_(PROVIDER_TYPE_L2TP_IPSEC_PSK) {
}

VirtualNetwork::~VirtualNetwork() {}

bool VirtualNetwork::ParseProviderValue(int index, const Value* value) {
  switch (index) {
    case PROPERTY_INDEX_HOST:
      return value->GetAsString(&server_hostname_);
    case PROPERTY_INDEX_NAME:
      // Note: shadows Network::name_ property.
      return value->GetAsString(&name_);
    case PROPERTY_INDEX_TYPE: {
      std::string provider_type_string;
      if (value->GetAsString(&provider_type_string)) {
        provider_type_ = ParseProviderType(provider_type_string);
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_L2TPIPSEC_CA_CERT_NSS:
      return value->GetAsString(&ca_cert_nss_);
    case PROPERTY_INDEX_L2TPIPSEC_PIN:
      // Ignore PIN property.
      return true;
    case PROPERTY_INDEX_L2TPIPSEC_PSK:
      return value->GetAsString(&psk_passphrase_);
    case PROPERTY_INDEX_L2TPIPSEC_CLIENT_CERT_ID:
      return value->GetAsString(&client_cert_id_);
    case PROPERTY_INDEX_L2TPIPSEC_CLIENT_CERT_SLOT:
      // Ignore ClientCertSlot property.
      return true;
    case PROPERTY_INDEX_L2TPIPSEC_USER:
      return value->GetAsString(&username_);
    case PROPERTY_INDEX_L2TPIPSEC_PASSWORD:
      return value->GetAsString(&user_passphrase_);
    default:
      break;
  }
  return false;
}

bool VirtualNetwork::ParseValue(int index, const Value* value) {
  switch (index) {
    case PROPERTY_INDEX_PROVIDER: {
      DCHECK_EQ(value->GetType(), Value::TYPE_DICTIONARY);
      const DictionaryValue* dict = static_cast<const DictionaryValue*>(value);
      for (DictionaryValue::key_iterator iter = dict->begin_keys();
           iter != dict->end_keys(); ++iter) {
        const std::string& key = *iter;
        Value* v;
        bool res = dict->GetWithoutPathExpansion(key, &v);
        DCHECK(res);
        if (res) {
          int index = property_index_parser().Get(key);
          if (!ParseProviderValue(index, v))
            VLOG(1) << name() << ": Provider unhandled key: " << key
                    << " Type: " << v->GetType();
        }
      }
      return true;
    }
    default:
      return Network::ParseValue(index, value);
      break;
  }
  return false;
}

void VirtualNetwork::ParseInfo(const DictionaryValue* info) {
  Network::ParseInfo(info);
  VLOG(1) << "VPN: " << name()
          << " Server: " << server_hostname()
          << " Type: " << ProviderTypeToString(provider_type());
  if (provider_type_ == PROVIDER_TYPE_L2TP_IPSEC_PSK) {
    if (!client_cert_id_.empty())
      provider_type_ = PROVIDER_TYPE_L2TP_IPSEC_USER_CERT;
  }
}

void VirtualNetwork::EraseCredentials() {
  WipeString(&ca_cert_nss_);
  WipeString(&psk_passphrase_);
  WipeString(&client_cert_id_);
  WipeString(&user_passphrase_);
}

void VirtualNetwork::CalculateUniqueId() {
  std::string provider_type(ProviderTypeToString(provider_type_));
  unique_id_ = provider_type + "|" + server_hostname_;
}

bool VirtualNetwork::RequiresUserProfile() const {
  return true;
}

void VirtualNetwork::CopyCredentialsFromRemembered(Network* remembered) {
  DCHECK_EQ(remembered->type(), TYPE_VPN);
  VirtualNetwork* remembered_vpn = static_cast<VirtualNetwork*>(remembered);
  VLOG(1) << "Copy VPN credentials: " << name()
          << " username: " << remembered_vpn->username();
  if (ca_cert_nss_.empty())
    ca_cert_nss_ = remembered_vpn->ca_cert_nss();
  if (psk_passphrase_.empty())
    psk_passphrase_ = remembered_vpn->psk_passphrase();
  if (client_cert_id_.empty())
    client_cert_id_ = remembered_vpn->client_cert_id();
  if (username_.empty())
    username_ = remembered_vpn->username();
  if (user_passphrase_.empty())
    user_passphrase_ = remembered_vpn->user_passphrase();
}

bool VirtualNetwork::NeedMoreInfoToConnect() const {
  if (server_hostname_.empty() || username_.empty() || user_passphrase_.empty())
    return true;
  switch (provider_type_) {
    case PROVIDER_TYPE_L2TP_IPSEC_PSK:
      if (psk_passphrase_.empty())
        return true;
      break;
    case PROVIDER_TYPE_L2TP_IPSEC_USER_CERT:
    case PROVIDER_TYPE_OPEN_VPN:
      if (client_cert_id_.empty())
        return true;
      break;
    case PROVIDER_TYPE_MAX:
      break;
  }
  return false;
}

std::string VirtualNetwork::GetProviderTypeString() const {
  switch (this->provider_type_) {
    case PROVIDER_TYPE_L2TP_IPSEC_PSK:
      return l10n_util::GetStringUTF8(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_L2TP_IPSEC_PSK);
      break;
    case PROVIDER_TYPE_L2TP_IPSEC_USER_CERT:
      return l10n_util::GetStringUTF8(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_L2TP_IPSEC_USER_CERT);
      break;
    case PROVIDER_TYPE_OPEN_VPN:
      return l10n_util::GetStringUTF8(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_OPEN_VPN);
      break;
    default:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ERROR_UNKNOWN);
      break;
  }
}

void VirtualNetwork::SetCACertNSS(const std::string& ca_cert_nss) {
  SetStringProperty(kL2TPIPSecCACertNSSProperty, ca_cert_nss, &ca_cert_nss_);
}

void VirtualNetwork::SetPSKPassphrase(const std::string& psk_passphrase) {
  SetStringProperty(kL2TPIPSecPSKProperty, psk_passphrase,
                           &psk_passphrase_);
}

void VirtualNetwork::SetClientCertID(const std::string& cert_id) {
  SetStringProperty(kL2TPIPSecClientCertIDProperty, cert_id, &client_cert_id_);
}

void VirtualNetwork::SetUsername(const std::string& username) {
  SetStringProperty(kL2TPIPSecUserProperty, username, &username_);
}

void VirtualNetwork::SetUserPassphrase(const std::string& user_passphrase) {
  SetStringProperty(kL2TPIPSecPasswordProperty, user_passphrase,
                    &user_passphrase_);
}

void VirtualNetwork::SetCertificateSlotAndPin(
    const std::string& slot, const std::string& pin) {
  SetOrClearStringProperty(kL2TPIPSecClientCertSlotProp, slot, NULL);
  SetOrClearStringProperty(kL2TPIPSecPINProperty, pin, NULL);
}

////////////////////////////////////////////////////////////////////////////////
// WirelessNetwork

bool WirelessNetwork::ParseValue(int index, const Value* value) {
  switch (index) {
    case PROPERTY_INDEX_SIGNAL_STRENGTH:
      return value->GetAsInteger(&strength_);
    default:
      return Network::ParseValue(index, value);
      break;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// CellularDataPlan

CellularDataPlan::CellularDataPlan()
    : plan_name("Unknown"),
      plan_type(CELLULAR_DATA_PLAN_UNLIMITED),
      plan_data_bytes(0),
      data_bytes_used(0) {
}

CellularDataPlan::CellularDataPlan(const CellularDataPlanInfo &plan)
    : plan_name(plan.plan_name ? plan.plan_name : ""),
      plan_type(plan.plan_type),
      update_time(base::Time::FromInternalValue(plan.update_time)),
      plan_start_time(base::Time::FromInternalValue(plan.plan_start_time)),
      plan_end_time(base::Time::FromInternalValue(plan.plan_end_time)),
      plan_data_bytes(plan.plan_data_bytes),
      data_bytes_used(plan.data_bytes_used) {
}

CellularDataPlan::~CellularDataPlan() {}

string16 CellularDataPlan::GetPlanDesciption() const {
  switch (plan_type) {
    case chromeos::CELLULAR_DATA_PLAN_UNLIMITED: {
      return l10n_util::GetStringFUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PURCHASE_UNLIMITED_DATA,
          base::TimeFormatFriendlyDate(plan_start_time));
      break;
    }
    case chromeos::CELLULAR_DATA_PLAN_METERED_PAID: {
      return l10n_util::GetStringFUTF16(
                IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PURCHASE_DATA,
                ui::FormatBytes(plan_data_bytes),
                base::TimeFormatFriendlyDate(plan_start_time));
    }
    case chromeos::CELLULAR_DATA_PLAN_METERED_BASE: {
      return l10n_util::GetStringFUTF16(
                IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_RECEIVED_FREE_DATA,
                ui::FormatBytes(plan_data_bytes),
                base::TimeFormatFriendlyDate(plan_start_time));
    default:
      break;
    }
  }
  return string16();
}

string16 CellularDataPlan::GetRemainingWarning() const {
  if (plan_type == chromeos::CELLULAR_DATA_PLAN_UNLIMITED) {
    // Time based plan. Show nearing expiration and data expiration.
    if (remaining_time().InSeconds() <= chromeos::kCellularDataVeryLowSecs) {
      return GetPlanExpiration();
    }
  } else if (plan_type == chromeos::CELLULAR_DATA_PLAN_METERED_PAID ||
             plan_type == chromeos::CELLULAR_DATA_PLAN_METERED_BASE) {
    // Metered plan. Show low data and out of data.
    if (remaining_data() <= chromeos::kCellularDataVeryLowBytes) {
      int64 remaining_mbytes = remaining_data() / (1024 * 1024);
      return l10n_util::GetStringFUTF16(
          IDS_NETWORK_DATA_REMAINING_MESSAGE,
          UTF8ToUTF16(base::Int64ToString(remaining_mbytes)));
    }
  }
  return string16();
}

string16 CellularDataPlan::GetDataRemainingDesciption() const {
  int64 remaining_bytes = remaining_data();
  switch (plan_type) {
    case chromeos::CELLULAR_DATA_PLAN_UNLIMITED: {
      return l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_UNLIMITED);
    }
    case chromeos::CELLULAR_DATA_PLAN_METERED_PAID: {
      return ui::FormatBytes(remaining_bytes);
    }
    case chromeos::CELLULAR_DATA_PLAN_METERED_BASE: {
      return ui::FormatBytes(remaining_bytes);
    }
    default:
      break;
  }
  return string16();
}

string16 CellularDataPlan::GetUsageInfo() const {
  if (plan_type == chromeos::CELLULAR_DATA_PLAN_UNLIMITED) {
    // Time based plan. Show nearing expiration and data expiration.
    return GetPlanExpiration();
  } else if (plan_type == chromeos::CELLULAR_DATA_PLAN_METERED_PAID ||
             plan_type == chromeos::CELLULAR_DATA_PLAN_METERED_BASE) {
    // Metered plan. Show low data and out of data.
    int64 remaining_bytes = remaining_data();
    if (remaining_bytes == 0) {
      return l10n_util::GetStringUTF16(
          IDS_NETWORK_DATA_NONE_AVAILABLE_MESSAGE);
    } else if (remaining_bytes < 1024 * 1024) {
      return l10n_util::GetStringUTF16(
          IDS_NETWORK_DATA_LESS_THAN_ONE_MB_AVAILABLE_MESSAGE);
    } else {
      int64 remaining_mb = remaining_bytes / (1024 * 1024);
      return l10n_util::GetStringFUTF16(
          IDS_NETWORK_DATA_MB_AVAILABLE_MESSAGE,
          UTF8ToUTF16(base::Int64ToString(remaining_mb)));
    }
  }
  return string16();
}

std::string CellularDataPlan::GetUniqueIdentifier() const {
  // A cellular plan is uniquely described by the union of name, type,
  // start time, end time, and max bytes.
  // So we just return a union of all these variables.
  return plan_name + "|" +
      base::Int64ToString(plan_type) + "|" +
      base::Int64ToString(plan_start_time.ToInternalValue()) + "|" +
      base::Int64ToString(plan_end_time.ToInternalValue()) + "|" +
      base::Int64ToString(plan_data_bytes);
}

base::TimeDelta CellularDataPlan::remaining_time() const {
  base::TimeDelta time = plan_end_time - base::Time::Now();
  return time.InMicroseconds() < 0 ? base::TimeDelta() : time;
}

int64 CellularDataPlan::remaining_minutes() const {
  return remaining_time().InMinutes();
}

int64 CellularDataPlan::remaining_data() const {
  int64 data = plan_data_bytes - data_bytes_used;
  return data < 0 ? 0 : data;
}

string16 CellularDataPlan::GetPlanExpiration() const {
  return TimeFormat::TimeRemaining(remaining_time());
}

////////////////////////////////////////////////////////////////////////////////
// CellTower

CellTower::CellTower() {}

////////////////////////////////////////////////////////////////////////////////
// WifiAccessPoint

WifiAccessPoint::WifiAccessPoint() {}

////////////////////////////////////////////////////////////////////////////////
// NetworkIPConfig

NetworkIPConfig::NetworkIPConfig(
    const std::string& device_path, IPConfigType type,
    const std::string& address, const std::string& netmask,
    const std::string& gateway, const std::string& name_servers)
    : device_path(device_path),
      type(type),
      address(address),
      netmask(netmask),
      gateway(gateway),
      name_servers(name_servers) {
}

NetworkIPConfig::~NetworkIPConfig() {}

int32 NetworkIPConfig::GetPrefixLength() const {
  int count = 0;
  int prefixlen = 0;
  StringTokenizer t(netmask, ".");
  while (t.GetNext()) {
    // If there are more than 4 numbers, than it's invalid.
    if (count == 4) {
      return -1;
    }
    std::string token = t.token();
    // If we already found the last mask and the current one is not
    // "0" then the netmask is invalid. For example, 255.224.255.0
    if (prefixlen / 8 != count) {
      if (token != "0") {
        return -1;
      }
    } else if (token == "255") {
      prefixlen += 8;
    } else if (token == "254") {
      prefixlen += 7;
    } else if (token == "252") {
      prefixlen += 6;
    } else if (token == "248") {
      prefixlen += 5;
    } else if (token == "240") {
      prefixlen += 4;
    } else if (token == "224") {
      prefixlen += 3;
    } else if (token == "192") {
      prefixlen += 2;
    } else if (token == "128") {
      prefixlen += 1;
    } else if (token == "0") {
      prefixlen += 0;
    } else {
      // mask is not a valid number.
      return -1;
    }
    count++;
  }
  if (count < 4)
    return -1;
  return prefixlen;
}

////////////////////////////////////////////////////////////////////////////////
// CellularApn

CellularApn::CellularApn() {}

CellularApn::CellularApn(
    const std::string& apn, const std::string& network_id,
    const std::string& username, const std::string& password)
    : apn(apn), network_id(network_id),
      username(username), password(password) {
}

CellularApn::~CellularApn() {}

void CellularApn::Set(const DictionaryValue& dict) {
  if (!dict.GetStringWithoutPathExpansion(kApnProperty, &apn))
    apn.clear();
  if (!dict.GetStringWithoutPathExpansion(kApnNetworkIdProperty, &network_id))
    network_id.clear();
  if (!dict.GetStringWithoutPathExpansion(kApnUsernameProperty, &username))
    username.clear();
  if (!dict.GetStringWithoutPathExpansion(kApnPasswordProperty, &password))
    password.clear();
  if (!dict.GetStringWithoutPathExpansion(kApnNameProperty, &name))
    name.clear();
  if (!dict.GetStringWithoutPathExpansion(kApnLocalizedNameProperty,
                                          &localized_name))
    localized_name.clear();
  if (!dict.GetStringWithoutPathExpansion(kApnLanguageProperty, &language))
    language.clear();
}

////////////////////////////////////////////////////////////////////////////////
// CellularNetwork

CellularNetwork::CellularNetwork(const std::string& service_path)
    : WirelessNetwork(service_path, TYPE_CELLULAR),
      activation_state_(ACTIVATION_STATE_UNKNOWN),
      network_technology_(NETWORK_TECHNOLOGY_UNKNOWN),
      roaming_state_(ROAMING_STATE_UNKNOWN),
      data_left_(DATA_UNKNOWN) {
}

CellularNetwork::~CellularNetwork() {
}

bool CellularNetwork::ParseValue(int index, const Value* value) {
  switch (index) {
    case PROPERTY_INDEX_ACTIVATION_STATE: {
      std::string activation_state_string;
      if (value->GetAsString(&activation_state_string)) {
        ActivationState prev_state = activation_state_;
        activation_state_ = ParseActivationState(activation_state_string);
        if (activation_state_ != prev_state)
          RefreshDataPlansIfNeeded();
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_CELLULAR_APN: {
      if (value->IsType(Value::TYPE_DICTIONARY)) {
        apn_.Set(*static_cast<const DictionaryValue*>(value));
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_CELLULAR_LAST_GOOD_APN: {
      if (value->IsType(Value::TYPE_DICTIONARY)) {
        last_good_apn_.Set(*static_cast<const DictionaryValue*>(value));
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_NETWORK_TECHNOLOGY: {
      std::string network_technology_string;
      if (value->GetAsString(&network_technology_string)) {
        network_technology_ = ParseNetworkTechnology(network_technology_string);
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_ROAMING_STATE: {
      std::string roaming_state_string;
      if (value->GetAsString(&roaming_state_string)) {
        roaming_state_ = ParseRoamingState(roaming_state_string);
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_OPERATOR_NAME:
      return value->GetAsString(&operator_name_);
    case PROPERTY_INDEX_OPERATOR_CODE:
      return value->GetAsString(&operator_code_);
    case PROPERTY_INDEX_SERVING_OPERATOR: {
      if (value->IsType(Value::TYPE_DICTIONARY)) {
        const DictionaryValue* dict =
            static_cast<const DictionaryValue*>(value);
        operator_code_.clear();
        operator_country_.clear();
        operator_name_.clear();
        dict->GetStringWithoutPathExpansion(kOperatorNameKey,
                                            &operator_name_);
        dict->GetStringWithoutPathExpansion(kOperatorCodeKey,
                                            &operator_code_);
        dict->GetStringWithoutPathExpansion(kOperatorCountryKey,
                                            &operator_country_);
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_PAYMENT_URL:
      return value->GetAsString(&payment_url_);
    case PROPERTY_INDEX_USAGE_URL:
      return value->GetAsString(&usage_url_);
    case PROPERTY_INDEX_STATE: {
      // Save previous state before calling WirelessNetwork::ParseValue.
      ConnectionState prev_state = state_;
      if (WirelessNetwork::ParseValue(index, value)) {
        if (state_ != prev_state)
          RefreshDataPlansIfNeeded();
        return true;
      }
      break;
    }
    default:
      return WirelessNetwork::ParseValue(index, value);
  }
  return false;
}

bool CellularNetwork::StartActivation() const {
  if (!EnsureCrosLoaded())
    return false;
  return chromeos::ActivateCellularModem(service_path().c_str(), NULL);
}

void CellularNetwork::RefreshDataPlansIfNeeded() const {
  if (!EnsureCrosLoaded())
    return;
  if (connected() && activated())
    chromeos::RequestCellularDataPlanUpdate(service_path().c_str());
}

void CellularNetwork::SetApn(const CellularApn& apn) {
  if (!apn.apn.empty()) {
    DictionaryValue value;
    // Only use the fields that are needed for establishing
    // connections, and ignore the rest.
    value.SetString(kApnProperty, apn.apn);
    value.SetString(kApnNetworkIdProperty, apn.network_id);
    value.SetString(kApnUsernameProperty, apn.username);
    value.SetString(kApnPasswordProperty, apn.password);
    SetValueProperty(kCellularApnProperty, &value);
  } else {
    ClearProperty(kCellularApnProperty);
  }
}

bool CellularNetwork::SupportsActivation() const {
  return SupportsDataPlan();
}

bool CellularNetwork::SupportsDataPlan() const {
  // TODO(nkostylev): Are there cases when only one of this is defined?
  return !usage_url().empty() || !payment_url().empty();
}

std::string CellularNetwork::GetNetworkTechnologyString() const {
  // No need to localize these cellular technology abbreviations.
  switch (network_technology_) {
    case NETWORK_TECHNOLOGY_1XRTT:
      return "1xRTT";
      break;
    case NETWORK_TECHNOLOGY_EVDO:
      return "EVDO";
      break;
    case NETWORK_TECHNOLOGY_GPRS:
      return "GPRS";
      break;
    case NETWORK_TECHNOLOGY_EDGE:
      return "EDGE";
      break;
    case NETWORK_TECHNOLOGY_UMTS:
      return "UMTS";
      break;
    case NETWORK_TECHNOLOGY_HSPA:
      return "HSPA";
      break;
    case NETWORK_TECHNOLOGY_HSPA_PLUS:
      return "HSPA Plus";
      break;
    case NETWORK_TECHNOLOGY_LTE:
      return "LTE";
      break;
    case NETWORK_TECHNOLOGY_LTE_ADVANCED:
      return "LTE Advanced";
      break;
    case NETWORK_TECHNOLOGY_GSM:
      return "GSM";
      break;
    default:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_CELLULAR_TECHNOLOGY_UNKNOWN);
      break;
  }
}

std::string CellularNetwork::ActivationStateToString(
    ActivationState activation_state) {
  switch (activation_state) {
    case ACTIVATION_STATE_ACTIVATED:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ACTIVATION_STATE_ACTIVATED);
      break;
    case ACTIVATION_STATE_ACTIVATING:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ACTIVATION_STATE_ACTIVATING);
      break;
    case ACTIVATION_STATE_NOT_ACTIVATED:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ACTIVATION_STATE_NOT_ACTIVATED);
      break;
    case ACTIVATION_STATE_PARTIALLY_ACTIVATED:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ACTIVATION_STATE_PARTIALLY_ACTIVATED);
      break;
    default:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ACTIVATION_STATE_UNKNOWN);
      break;
  }
}

std::string CellularNetwork::GetActivationStateString() const {
  return ActivationStateToString(this->activation_state_);
}

std::string CellularNetwork::GetRoamingStateString() const {
  switch (this->roaming_state_) {
    case ROAMING_STATE_HOME:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ROAMING_STATE_HOME);
      break;
    case ROAMING_STATE_ROAMING:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ROAMING_STATE_ROAMING);
      break;
    default:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_ROAMING_STATE_UNKNOWN);
      break;
  }
}

////////////////////////////////////////////////////////////////////////////////
// WifiNetwork

WifiNetwork::WifiNetwork(const std::string& service_path)
    : WirelessNetwork(service_path, TYPE_WIFI),
      encryption_(SECURITY_NONE),
      passphrase_required_(false),
      eap_method_(EAP_METHOD_UNKNOWN),
      eap_phase_2_auth_(EAP_PHASE_2_AUTH_AUTO),
      eap_use_system_cas_(true) {
}

WifiNetwork::~WifiNetwork() {}

void WifiNetwork::CalculateUniqueId() {
  ConnectionSecurity encryption = encryption_;
  // Flimflam treats wpa and rsn as psk internally, so convert those types
  // to psk for unique naming.
  if (encryption == SECURITY_WPA || encryption == SECURITY_RSN)
    encryption = SECURITY_PSK;
  std::string security = std::string(SecurityToString(encryption));
  unique_id_ = security + "|" + name_;
}

bool WifiNetwork::SetSsid(const std::string& ssid) {
  // Detects encoding and convert to UTF-8.
  std::string ssid_utf8;
  if (!IsStringUTF8(ssid)) {
    std::string encoding;
    if (base::DetectEncoding(ssid, &encoding)) {
      if (!base::ConvertToUtf8AndNormalize(ssid, encoding, &ssid_utf8)) {
        ssid_utf8.clear();
      }
    }
  }

  if (ssid_utf8.empty())
    SetName(ssid);
  else
    SetName(ssid_utf8);

  return true;
}

bool WifiNetwork::SetHexSsid(const std::string& ssid_hex) {
  // Converts ascii hex dump (eg. "49656c6c6f") to string (eg. "Hello").
  std::vector<uint8> ssid_raw;
  if (!base::HexStringToBytes(ssid_hex, &ssid_raw)) {
    LOG(ERROR) << "Illegal hex char is found in WiFi.HexSSID.";
    ssid_raw.clear();
    return false;
  }

  return SetSsid(std::string(ssid_raw.begin(), ssid_raw.end()));
}

bool WifiNetwork::ParseValue(int index, const Value* value) {
  switch (index) {
    case PROPERTY_INDEX_WIFI_HEX_SSID: {
      std::string ssid_hex;
      if (!value->GetAsString(&ssid_hex))
        return false;

      SetHexSsid(ssid_hex);
      return true;
    }
    case PROPERTY_INDEX_WIFI_AUTH_MODE:
    case PROPERTY_INDEX_WIFI_PHY_MODE:
    case PROPERTY_INDEX_WIFI_HIDDEN_SSID:
    case PROPERTY_INDEX_WIFI_FREQUENCY:
      // These properties are currently not used in the UI.
      return true;
    case PROPERTY_INDEX_NAME: {
      // Does not change network name when it was already set by WiFi.HexSSID.
      if (!name().empty())
        return true;
      else
        return WirelessNetwork::ParseValue(index, value);
    }
    case PROPERTY_INDEX_SECURITY: {
      std::string security_string;
      if (value->GetAsString(&security_string)) {
        encryption_ = ParseSecurity(security_string);
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_PASSPHRASE: {
      std::string passphrase;
      if (value->GetAsString(&passphrase)) {
        // Only store the passphrase if we are the owner.
        // TODO(stevenjb): Remove this when chromium-os:12948 is resolved.
        if (chromeos::UserManager::Get()->current_user_is_owner())
          passphrase_ = passphrase;
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_PASSPHRASE_REQUIRED:
      return value->GetAsBoolean(&passphrase_required_);
    case PROPERTY_INDEX_IDENTITY:
      return value->GetAsString(&identity_);
    case PROPERTY_INDEX_EAP_IDENTITY:
      return value->GetAsString(&eap_identity_);
    case PROPERTY_INDEX_EAP_METHOD: {
      std::string method;
      if (value->GetAsString(&method)) {
        eap_method_ = ParseEAPMethod(method);
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_EAP_PHASE_2_AUTH: {
      std::string auth;
      if (value->GetAsString(&auth)) {
        eap_phase_2_auth_ = ParseEAPPhase2Auth(auth);
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_EAP_ANONYMOUS_IDENTITY:
      return value->GetAsString(&eap_anonymous_identity_);
    case PROPERTY_INDEX_EAP_CERT_ID:
      return value->GetAsString(&eap_client_cert_pkcs11_id_);
    case PROPERTY_INDEX_EAP_CA_CERT_NSS:
      return value->GetAsString(&eap_server_ca_cert_nss_nickname_);
    case PROPERTY_INDEX_EAP_USE_SYSTEM_CAS:
      return value->GetAsBoolean(&eap_use_system_cas_);
    case PROPERTY_INDEX_EAP_PASSWORD:
      return value->GetAsString(&eap_passphrase_);
    case PROPERTY_INDEX_EAP_CLIENT_CERT:
    case PROPERTY_INDEX_EAP_CLIENT_CERT_NSS:
    case PROPERTY_INDEX_EAP_PRIVATE_KEY:
    case PROPERTY_INDEX_EAP_PRIVATE_KEY_PASSWORD:
    case PROPERTY_INDEX_EAP_KEY_ID:
    case PROPERTY_INDEX_EAP_CA_CERT:
    case PROPERTY_INDEX_EAP_CA_CERT_ID:
    case PROPERTY_INDEX_EAP_PIN:
    case PROPERTY_INDEX_EAP_KEY_MGMT:
      // These properties are currently not used in the UI.
      return true;
    default:
      return WirelessNetwork::ParseValue(index, value);
  }
  return false;
}

const std::string& WifiNetwork::GetPassphrase() const {
  if (!user_passphrase_.empty())
    return user_passphrase_;
  return passphrase_;
}

void WifiNetwork::SetPassphrase(const std::string& passphrase) {
  // Set the user_passphrase_ only; passphrase_ stores the flimflam value.
  // If the user sets an empty passphrase, restore it to the passphrase
  // remembered by flimflam.
  if (!passphrase.empty()) {
    user_passphrase_ = passphrase;
    passphrase_ = passphrase;
  } else {
    user_passphrase_ = passphrase_;
  }
  // Send the change to flimflam. If the format is valid, it will propagate to
  // passphrase_ with a service update.
  SetOrClearStringProperty(kPassphraseProperty, passphrase, NULL);
}

// See src/third_party/flimflam/doc/service-api.txt for properties that
// flimflam will forget when SaveCredentials is false.
void WifiNetwork::EraseCredentials() {
  WipeString(&passphrase_);
  WipeString(&user_passphrase_);
  WipeString(&eap_client_cert_pkcs11_id_);
  WipeString(&eap_identity_);
  WipeString(&eap_anonymous_identity_);
  WipeString(&eap_passphrase_);
}

void WifiNetwork::SetIdentity(const std::string& identity) {
  SetStringProperty(kIdentityProperty, identity, &identity_);
}

void WifiNetwork::SetEAPMethod(EAPMethod method) {
  eap_method_ = method;
  switch (method) {
    case EAP_METHOD_PEAP:
      SetStringProperty(kEapMethodProperty, kEapMethodPEAP, NULL);
      break;
    case EAP_METHOD_TLS:
      SetStringProperty(kEapMethodProperty, kEapMethodTLS, NULL);
      break;
    case EAP_METHOD_TTLS:
      SetStringProperty(kEapMethodProperty, kEapMethodTTLS, NULL);
      break;
    case EAP_METHOD_LEAP:
      SetStringProperty(kEapMethodProperty, kEapMethodLEAP, NULL);
      break;
    default:
      ClearProperty(kEapMethodProperty);
      break;
  }
}

void WifiNetwork::SetEAPPhase2Auth(EAPPhase2Auth auth) {
  eap_phase_2_auth_ = auth;
  bool is_peap = (eap_method_ == EAP_METHOD_PEAP);
  switch (auth) {
    case EAP_PHASE_2_AUTH_AUTO:
      ClearProperty(kEapPhase2AuthProperty);
      break;
    case EAP_PHASE_2_AUTH_MD5:
      SetStringProperty(kEapPhase2AuthProperty,
                        is_peap ? kEapPhase2AuthPEAPMD5
                                : kEapPhase2AuthTTLSMD5,
                        NULL);
      break;
    case EAP_PHASE_2_AUTH_MSCHAPV2:
      SetStringProperty(kEapPhase2AuthProperty,
                        is_peap ? kEapPhase2AuthPEAPMSCHAPV2
                                : kEapPhase2AuthTTLSMSCHAPV2,
                        NULL);
      break;
    case EAP_PHASE_2_AUTH_MSCHAP:
      SetStringProperty(kEapPhase2AuthProperty, kEapPhase2AuthTTLSMSCHAP, NULL);
      break;
    case EAP_PHASE_2_AUTH_PAP:
      SetStringProperty(kEapPhase2AuthProperty, kEapPhase2AuthTTLSPAP, NULL);
      break;
    case EAP_PHASE_2_AUTH_CHAP:
      SetStringProperty(kEapPhase2AuthProperty, kEapPhase2AuthTTLSCHAP, NULL);
      break;
  }
}

void WifiNetwork::SetEAPServerCaCertNssNickname(
    const std::string& nss_nickname) {
  VLOG(1) << "SetEAPServerCaCertNssNickname " << nss_nickname;
  SetOrClearStringProperty(kEapCaCertNssProperty, nss_nickname,
                           &eap_server_ca_cert_nss_nickname_);
}

void WifiNetwork::SetEAPClientCertPkcs11Id(const std::string& pkcs11_id) {
  VLOG(1) << "SetEAPClientCertPkcs11Id " << pkcs11_id;
  SetOrClearStringProperty(kEapCertIDProperty, pkcs11_id,
                           &eap_client_cert_pkcs11_id_);
  // flimflam requires both CertID and KeyID for TLS connections, despite
  // the fact that by convention they are the same ID.
  SetOrClearStringProperty(kEapKeyIDProperty, pkcs11_id, NULL);
}

void WifiNetwork::SetEAPUseSystemCAs(bool use_system_cas) {
  SetBooleanProperty(kEapUseSystemCAsProperty, use_system_cas,
                     &eap_use_system_cas_);
}

void WifiNetwork::SetEAPIdentity(const std::string& identity) {
  SetOrClearStringProperty(kEapIdentityProperty, identity, &eap_identity_);
}

void WifiNetwork::SetEAPAnonymousIdentity(const std::string& identity) {
  SetOrClearStringProperty(kEapAnonymousIdentityProperty, identity,
                           &eap_anonymous_identity_);
}

void WifiNetwork::SetEAPPassphrase(const std::string& passphrase) {
  SetOrClearStringProperty(kEapPasswordProperty, passphrase, &eap_passphrase_);
}

std::string WifiNetwork::GetEncryptionString() const {
  switch (encryption_) {
    case SECURITY_UNKNOWN:
      break;
    case SECURITY_NONE:
      return "";
    case SECURITY_WEP:
      return "WEP";
    case SECURITY_WPA:
      return "WPA";
    case SECURITY_RSN:
      return "RSN";
    case SECURITY_8021X: {
      std::string result("8021X");
      switch (eap_method_) {
        case EAP_METHOD_PEAP:
          result += "+PEAP";
          break;
        case EAP_METHOD_TLS:
          result += "+TLS";
          break;
        case EAP_METHOD_TTLS:
          result += "+TTLS";
          break;
        case EAP_METHOD_LEAP:
          result += "+LEAP";
          break;
        default:
          break;
      }
      return result;
    }
    case SECURITY_PSK:
      return "PSK";
  }
  return "Unknown";
}

bool WifiNetwork::IsPassphraseRequired() const {
  // TODO(stevenjb): Remove error_ tests when fixed in flimflam
  // (http://crosbug.com/10135).
  if (error_ == ERROR_BAD_PASSPHRASE || error_ == ERROR_BAD_WEPKEY)
    return true;
  // For 802.1x networks, configuration is required if connectable is false.
  if (encryption_ == SECURITY_8021X)
    return !connectable_;
  return passphrase_required_;
}

bool WifiNetwork::RequiresUserProfile() const {
  // 8021X requires certificates which are only stored for individual users.
  if (encryption_ == SECURITY_8021X &&
      (eap_method_ == EAP_METHOD_TLS ||
       !eap_client_cert_pkcs11_id().empty()))
    return true;
  return false;
}

void WifiNetwork::SetCertificatePin(const std::string& pin) {
  SetOrClearStringProperty(kEapPinProperty, pin, NULL);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkLibrary

class NetworkLibraryImplBase : public NetworkLibrary  {
 public:
  NetworkLibraryImplBase();
  virtual ~NetworkLibraryImplBase();

  //////////////////////////////////////////////////////////////////////////////
  // NetworkLibraryImplBase virtual functions.

  // Functions for monitoring networks & devices.
  virtual void MonitorNetworkStart(const std::string& service_path) = 0;
  virtual void MonitorNetworkStop(const std::string& service_path) = 0;
  virtual void MonitorNetworkDeviceStart(const std::string& device_path) = 0;
  virtual void MonitorNetworkDeviceStop(const std::string& device_path) = 0;

  // Called from ConnectToWifiNetwork.
  // Calls ConnectToWifiNetworkUsingConnectData if network request succeeds.
  virtual void CallRequestWifiNetworkAndConnect(
      const std::string& ssid, ConnectionSecurity security) = 0;
  // Called from ConnectToVirtualNetwork*.
  // Calls ConnectToVirtualNetworkUsingConnectData if network request succeeds.
  virtual void CallRequestVirtualNetworkAndConnect(
      const std::string& service_name,
      const std::string& server_hostname,
      VirtualNetwork::ProviderType provider_type) = 0;
  // Called from NetworkConnectStart.
  // Calls NetworkConnectCompleted when the connection attept completes.
  virtual void CallConnectToNetwork(Network* network) = 0;
  // Called from DeleteRememberedNetwork.
  virtual void CallDeleteRememberedNetwork(
      const std::string& profile_path, const std::string& service_path) = 0;

  // Called from Enable*NetworkDevice.
  // Asynchronously enables or disables the specified device type.
  virtual void CallEnableNetworkDeviceType(
      ConnectionType device, bool enable) = 0;

  //////////////////////////////////////////////////////////////////////////////
  // NetworkLibrary implementation.

  // virtual Init implemented in derived classes.
  // virtual IsCros implemented in derived classes.

  virtual void AddNetworkManagerObserver(
      NetworkManagerObserver* observer) OVERRIDE;
  virtual void RemoveNetworkManagerObserver(
      NetworkManagerObserver* observer) OVERRIDE;
  virtual void AddNetworkObserver(const std::string& service_path,
                                  NetworkObserver* observer) OVERRIDE;
  virtual void RemoveNetworkObserver(const std::string& service_path,
                                     NetworkObserver* observer) OVERRIDE;
  virtual void RemoveObserverForAllNetworks(
      NetworkObserver* observer) OVERRIDE;
  virtual void AddNetworkDeviceObserver(
      const std::string& device_path,
      NetworkDeviceObserver* observer) OVERRIDE;
  virtual void RemoveNetworkDeviceObserver(
      const std::string& device_path,
      NetworkDeviceObserver* observer) OVERRIDE;

  virtual void Lock() OVERRIDE;
  virtual void Unlock() OVERRIDE;
  virtual bool IsLocked() OVERRIDE;

  virtual void AddCellularDataPlanObserver(
      CellularDataPlanObserver* observer) OVERRIDE;
  virtual void RemoveCellularDataPlanObserver(
      CellularDataPlanObserver* observer) OVERRIDE;
  virtual void AddPinOperationObserver(
      PinOperationObserver* observer) OVERRIDE;
  virtual void RemovePinOperationObserver(
      PinOperationObserver* observer) OVERRIDE;
  virtual void AddUserActionObserver(
      UserActionObserver* observer) OVERRIDE;
  virtual void RemoveUserActionObserver(
      UserActionObserver* observer) OVERRIDE;

  virtual const EthernetNetwork* ethernet_network() const OVERRIDE {
    return ethernet_;
  }
  virtual bool ethernet_connecting() const OVERRIDE {
    return ethernet_ ? ethernet_->connecting() : false;
  }
  virtual bool ethernet_connected() const OVERRIDE {
    return ethernet_ ? ethernet_->connected() : false;
  }
  virtual const WifiNetwork* wifi_network() const OVERRIDE {
    return active_wifi_;
  }
  virtual bool wifi_connecting() const OVERRIDE {
    return active_wifi_ ? active_wifi_->connecting() : false;
  }
  virtual bool wifi_connected() const OVERRIDE {
    return active_wifi_ ? active_wifi_->connected() : false;
  }
  virtual const CellularNetwork* cellular_network() const OVERRIDE {
    return active_cellular_;
  }
  virtual bool cellular_connecting() const OVERRIDE {
    return active_cellular_ ? active_cellular_->connecting() : false;
  }
  virtual bool cellular_connected() const OVERRIDE {
    return active_cellular_ ? active_cellular_->connected() : false;
  }
  virtual const VirtualNetwork* virtual_network() const OVERRIDE {
    return active_virtual_;
  }
  virtual bool virtual_network_connecting() const OVERRIDE {
    return active_virtual_ ? active_virtual_->connecting() : false;
  }
  virtual bool virtual_network_connected() const OVERRIDE {
    return active_virtual_ ? active_virtual_->connected() : false;
  }
  virtual bool Connected() const OVERRIDE {
    return ethernet_connected() || wifi_connected() || cellular_connected();
  }
  virtual bool Connecting() const OVERRIDE {
    return ethernet_connecting() || wifi_connecting() || cellular_connecting();
  }
  virtual const WifiNetworkVector& wifi_networks() const OVERRIDE {
    return wifi_networks_;
  }
  virtual const WifiNetworkVector& remembered_wifi_networks() const OVERRIDE {
    return remembered_wifi_networks_;
  }
  virtual const CellularNetworkVector& cellular_networks() const OVERRIDE {
    return cellular_networks_;
  }
  virtual const VirtualNetworkVector& virtual_networks() const OVERRIDE {
    return virtual_networks_;
  }
  virtual const VirtualNetworkVector&
      remembered_virtual_networks() const OVERRIDE {
    return remembered_virtual_networks_;
  }
  virtual const Network* active_network() const OVERRIDE;
  virtual const Network* connected_network() const OVERRIDE;
  virtual const Network* connecting_network() const;
  virtual bool ethernet_available() const OVERRIDE {
    return available_devices_ & (1 << TYPE_ETHERNET);
  }
  virtual bool wifi_available() const OVERRIDE {
    return available_devices_ & (1 << TYPE_WIFI);
  }
  virtual bool cellular_available() const OVERRIDE {
    return available_devices_ & (1 << TYPE_CELLULAR);
  }
  virtual bool ethernet_enabled() const OVERRIDE {
    return enabled_devices_ & (1 << TYPE_ETHERNET);
  }
  virtual bool wifi_enabled() const OVERRIDE {
    return enabled_devices_ & (1 << TYPE_WIFI);
  }
  virtual bool cellular_enabled() const OVERRIDE {
    return enabled_devices_ & (1 << TYPE_CELLULAR);
  }
  virtual bool wifi_scanning() const OVERRIDE {
    return wifi_scanning_;
  }
  virtual bool offline_mode() const OVERRIDE { return offline_mode_; }
  virtual const std::string& IPAddress() const OVERRIDE;

  virtual const NetworkDevice* FindNetworkDeviceByPath(
      const std::string& path) const OVERRIDE;
  NetworkDevice* FindNetworkDeviceByPath(const std::string& path);
  virtual const NetworkDevice* FindCellularDevice() const OVERRIDE;
  virtual const NetworkDevice* FindEthernetDevice() const OVERRIDE;
  virtual const NetworkDevice* FindWifiDevice() const OVERRIDE;
  virtual Network* FindNetworkByPath(const std::string& path) const OVERRIDE;
  WirelessNetwork* FindWirelessNetworkByPath(const std::string& path) const;
  virtual WifiNetwork* FindWifiNetworkByPath(
      const std::string& path) const OVERRIDE;
  virtual CellularNetwork* FindCellularNetworkByPath(
      const std::string& path) const OVERRIDE;
  virtual VirtualNetwork* FindVirtualNetworkByPath(
      const std::string& path) const OVERRIDE;
  virtual Network* FindNetworkFromRemembered(
      const Network* remembered) const OVERRIDE;
  Network* FindRememberedFromNetwork(const Network* network) const;
  virtual Network* FindRememberedNetworkByPath(
      const std::string& path) const OVERRIDE;

  virtual const CellularDataPlanVector* GetDataPlans(
      const std::string& path) const OVERRIDE;
  virtual const CellularDataPlan* GetSignificantDataPlan(
      const std::string& path) const OVERRIDE;
  virtual void SignalCellularPlanPayment() OVERRIDE;
  virtual bool HasRecentCellularPlanPayment() OVERRIDE;
  virtual std::string GetCellularHomeCarrierId() const OVERRIDE;

  // virtual ChangePin implemented in derived classes.
  // virtual ChangeRequiredPin implemented in derived classes.
  // virtual EnterPin implemented in derived classes.
  // virtual UnblockPin implemented in derived classes.

  // virtual RequestCellularScan implemented in derived classes.
  // virtual RequestCellularRegister implemented in derived classes.
  // virtual SetCellularDataRoamingAllowed implemented in derived classes.
  // virtual RequestNetworkScan implemented in derived classes.
  // virtual GetWifiAccessPoints implemented in derived classes.

  virtual bool HasProfileType(NetworkProfileType type) const OVERRIDE;
  virtual void SetNetworkProfile(const std::string& service_path,
                                 NetworkProfileType type) OVERRIDE;
  virtual bool CanConnectToNetwork(const Network* network) const OVERRIDE;

  // Connect to an existing network.
  virtual void ConnectToWifiNetwork(WifiNetwork* wifi) OVERRIDE;
  virtual void ConnectToWifiNetwork(WifiNetwork* wifi, bool shared) OVERRIDE;
  virtual void ConnectToCellularNetwork(CellularNetwork* cellular) OVERRIDE;
  virtual void ConnectToVirtualNetwork(VirtualNetwork* vpn) OVERRIDE;

  // Request a network and connect to it.
  virtual void ConnectToUnconfiguredWifiNetwork(
      const std::string& ssid,
      ConnectionSecurity security,
      const std::string& passphrase,
      const EAPConfigData* eap_config,
      bool save_credentials,
      bool shared) OVERRIDE;
  virtual void ConnectToVirtualNetworkPSK(
      const std::string& service_name,
      const std::string& server_hostname,
      const std::string& psk,
      const std::string& username,
      const std::string& user_passphrase) OVERRIDE;
  virtual void ConnectToVirtualNetworkCert(
      const std::string& service_name,
      const std::string& server_hostname,
      const std::string& server_ca_cert_nss_nickname,
      const std::string& client_cert_pkcs11_id,
      const std::string& username,
      const std::string& user_passphrase) OVERRIDE;

  // virtual DisconnectFromNetwork implemented in derived classes.
  virtual void ForgetNetwork(const std::string& service_path) OVERRIDE;
  virtual void EnableEthernetNetworkDevice(bool enable) OVERRIDE;
  virtual void EnableWifiNetworkDevice(bool enable) OVERRIDE;
  virtual void EnableCellularNetworkDevice(bool enable) OVERRIDE;
  // virtual EnableOfflineMode implemented in derived classes.
  // virtual GetIPConfigs implemented in derived classes.
  // virtual SetIPConfig implemented in derived classes.
  virtual void SwitchToPreferredNetwork() OVERRIDE;

 protected:
  typedef ObserverList<NetworkObserver> NetworkObserverList;
  typedef std::map<std::string, NetworkObserverList*> NetworkObserverMap;

  typedef ObserverList<NetworkDeviceObserver> NetworkDeviceObserverList;
  typedef std::map<std::string, NetworkDeviceObserverList*>
      NetworkDeviceObserverMap;

  typedef std::map<std::string, Network*> NetworkMap;
  typedef std::map<std::string, int> PriorityMap;
  typedef std::map<std::string, NetworkDevice*> NetworkDeviceMap;
  typedef std::map<std::string, CellularDataPlanVector*> CellularDataPlanMap;

  struct NetworkProfile {
    NetworkProfile(const std::string& p, NetworkProfileType t)
        : path(p), type(t) {}
    std::string path;
    NetworkProfileType type;
    typedef std::set<std::string> ServiceList;
    ServiceList services;
  };
  typedef std::list<NetworkProfile> NetworkProfileList;

  struct ConnectData {
    ConnectData() :
        security(SECURITY_NONE),
        eap_method(EAP_METHOD_UNKNOWN),
        eap_auth(EAP_PHASE_2_AUTH_AUTO),
        eap_use_system_cas(false),
        save_credentials(false),
        profile_type(PROFILE_NONE) {}
    ConnectionSecurity security;
    std::string service_name;  // For example, SSID.
    std::string passphrase;
    std::string server_hostname;
    std::string server_ca_cert_nss_nickname;
    std::string client_cert_pkcs11_id;
    EAPMethod eap_method;
    EAPPhase2Auth eap_auth;
    bool eap_use_system_cas;
    std::string eap_identity;
    std::string eap_anonymous_identity;
    std::string psk_key;
    std::string psk_username;
    bool save_credentials;
    NetworkProfileType profile_type;
  };

  enum NetworkConnectStatus {
    CONNECT_SUCCESS,
    CONNECT_BAD_PASSPHRASE,
    CONNECT_FAILED
  };

  // Called from ConnectTo*Network.
  void NetworkConnectStartWifi(
      WifiNetwork* network, NetworkProfileType profile_type);
  void NetworkConnectStartVPN(VirtualNetwork* network);
  void NetworkConnectStart(Network* network, NetworkProfileType profile_type);
  // Called from CallConnectToNetwork.
  void NetworkConnectCompleted(Network* network,
                               NetworkConnectStatus status);
  // Called from CallRequestWifiNetworkAndConnect.
  void ConnectToWifiNetworkUsingConnectData(WifiNetwork* wifi);
  // Called from CallRequestVirtualNetworkAndConnect.
  void ConnectToVirtualNetworkUsingConnectData(VirtualNetwork* vpn);
  // Called from DisconnectFromNetwork.
  void NetworkDisconnectCompleted(const std::string& service_path);
  // Called from GetSignificantDataPlan.
  const CellularDataPlan* GetSignificantDataPlanFromVector(
      const CellularDataPlanVector* plans) const;
  CellularNetwork::DataLeft GetDataLeft(CellularDataPlanVector* data_plans);
  void UpdateCellularDataPlan(const std::string& service_path,
                              const CellularDataPlanList* data_plan_list);

  // Network list management functions.
  void UpdateActiveNetwork(Network* network);
  void AddNetwork(Network* network);
  void DeleteNetwork(Network* network);
  void AddRememberedNetwork(Network* network);
  void DeleteRememberedNetwork(const std::string& service_path);
  Network* CreateNewNetwork(
      ConnectionType type, const std::string& service_path);
  void ClearNetworks();
  void ClearRememberedNetworks();
  void DeleteNetworks();
  void DeleteRememberedNetworks();
  void DeleteDevice(const std::string& device_path);
  void DeleteDeviceFromDeviceObserversMap(const std::string& device_path);

  // Profile management functions.
  void SetProfileType(Network* network, NetworkProfileType type);
  void SetProfileTypeFromPath(Network* network);
  std::string GetProfilePath(NetworkProfileType type);

  // Notifications.
  void NotifyNetworkManagerChanged(bool force_update);
  void SignalNetworkManagerObservers();
  void NotifyNetworkChanged(Network* network);
  void NotifyNetworkDeviceChanged(NetworkDevice* device, PropertyIndex index);
  void NotifyCellularDataPlanChanged();
  void NotifyPinOperationCompleted(PinOperationError error);
  void NotifyUserConnectionInitiated(const Network* network);

  // TPM related functions.
  void GetTpmInfo();
  const std::string& GetTpmSlot();
  const std::string& GetTpmPin();

  // Pin related functions.
  void FlipSimPinRequiredStateIfNeeded();

  // Network manager observer list.
  ObserverList<NetworkManagerObserver> network_manager_observers_;

  // Cellular data plan observer list.
  ObserverList<CellularDataPlanObserver> data_plan_observers_;

  // PIN operation observer list.
  ObserverList<PinOperationObserver> pin_operation_observers_;

  // User action observer list.
  ObserverList<UserActionObserver> user_action_observers_;

  // Network observer map.
  NetworkObserverMap network_observers_;

  // Network device observer map.
  NetworkDeviceObserverMap network_device_observers_;

  // Network login observer.
  scoped_ptr<NetworkLoginObserver> network_login_observer_;

  // List of profiles.
  NetworkProfileList profile_list_;

  // List of networks to move to the user profile once logged in.
  std::list<std::string> user_networks_;

  // A service path based map of all Networks.
  NetworkMap network_map_;

  // A unique_id_ based map of Networks.
  NetworkMap network_unique_id_map_;

  // A service path based map of all remembered Networks.
  NetworkMap remembered_network_map_;

  // A list of services that we are awaiting updates for.
  PriorityMap network_update_requests_;

  // A device path based map of all NetworkDevices.
  NetworkDeviceMap device_map_;

  // A network service path based map of all CellularDataPlanVectors.
  CellularDataPlanMap data_plan_map_;

  // The ethernet network.
  EthernetNetwork* ethernet_;

  // The list of available wifi networks.
  WifiNetworkVector wifi_networks_;

  // The current connected (or connecting) wifi network.
  WifiNetwork* active_wifi_;

  // The remembered wifi networks.
  WifiNetworkVector remembered_wifi_networks_;

  // The list of available cellular networks.
  CellularNetworkVector cellular_networks_;

  // The current connected (or connecting) cellular network.
  CellularNetwork* active_cellular_;

  // The list of available virtual networks.
  VirtualNetworkVector virtual_networks_;

  // The current connected (or connecting) virtual network.
  VirtualNetwork* active_virtual_;

  // The remembered virtual networks.
  VirtualNetworkVector remembered_virtual_networks_;

  // The path of the active profile (for retrieving remembered services).
  std::string active_profile_path_;

  // The current available network devices. Bitwise flag of ConnectionTypes.
  int available_devices_;

  // The current enabled network devices. Bitwise flag of ConnectionTypes.
  int enabled_devices_;

  // The current connected network devices. Bitwise flag of ConnectionTypes.
  int connected_devices_;

  // True if we are currently scanning for wifi networks.
  bool wifi_scanning_;

  // Currently not implemented. TODO(stevenjb): implement or eliminate.
  bool offline_mode_;

  // True if access network library is locked.
  bool is_locked_;

  // TPM module user slot and PIN, needed by flimflam to access certificates.
  std::string tpm_slot_;
  std::string tpm_pin_;

  // Type of pending SIM operation, SIM_OPERATION_NONE otherwise.
  SimOperationType sim_operation_;

  // Delayed task to notify a network change.
  CancelableTask* notify_task_;

  // Cellular plan payment time.
  base::Time cellular_plan_payment_time_;

  // Temporary connection data for async connect calls.
  ConnectData connect_data_;

  DISALLOW_COPY_AND_ASSIGN(NetworkLibraryImplBase);
};

NetworkLibraryImplBase::NetworkLibraryImplBase()
    : ethernet_(NULL),
      active_wifi_(NULL),
      active_cellular_(NULL),
      active_virtual_(NULL),
      available_devices_(0),
      enabled_devices_(0),
      connected_devices_(0),
      wifi_scanning_(false),
      offline_mode_(false),
      is_locked_(false),
      sim_operation_(SIM_OPERATION_NONE),
      notify_task_(NULL) {
  network_login_observer_.reset(new NetworkLoginObserver(this));
}

NetworkLibraryImplBase::~NetworkLibraryImplBase() {
  network_manager_observers_.Clear();
  data_plan_observers_.Clear();
  pin_operation_observers_.Clear();
  user_action_observers_.Clear();
  DeleteNetworks();
  DeleteRememberedNetworks();
  STLDeleteValues(&data_plan_map_);
  STLDeleteValues(&device_map_);
  STLDeleteValues(&network_device_observers_);
  STLDeleteValues(&network_observers_);
}

//////////////////////////////////////////////////////////////////////////////
// NetworkLibrary implementation.

void NetworkLibraryImplBase::AddNetworkManagerObserver(
    NetworkManagerObserver* observer) {
  if (!network_manager_observers_.HasObserver(observer))
    network_manager_observers_.AddObserver(observer);
}

void NetworkLibraryImplBase::RemoveNetworkManagerObserver(
    NetworkManagerObserver* observer) {
  network_manager_observers_.RemoveObserver(observer);
}

void NetworkLibraryImplBase::AddNetworkObserver(
    const std::string& service_path, NetworkObserver* observer) {
  // First, add the observer to the callback map.
  NetworkObserverMap::iterator iter = network_observers_.find(service_path);
  NetworkObserverList* oblist;
  if (iter != network_observers_.end()) {
    oblist = iter->second;
  } else {
    oblist = new NetworkObserverList();
    network_observers_[service_path] = oblist;
  }
  if (observer && !oblist->HasObserver(observer))
    oblist->AddObserver(observer);
  MonitorNetworkStart(service_path);
}

void NetworkLibraryImplBase::RemoveNetworkObserver(
    const std::string& service_path, NetworkObserver* observer) {
  DCHECK(service_path.size());
  NetworkObserverMap::iterator map_iter =
      network_observers_.find(service_path);
  if (map_iter != network_observers_.end()) {
    map_iter->second->RemoveObserver(observer);
    if (!map_iter->second->size()) {
      delete map_iter->second;
      network_observers_.erase(map_iter);
    }
  }
  MonitorNetworkStop(service_path);
}

void NetworkLibraryImplBase::RemoveObserverForAllNetworks(
    NetworkObserver* observer) {
  DCHECK(observer);
  NetworkObserverMap::iterator map_iter = network_observers_.begin();
  while (map_iter != network_observers_.end()) {
    map_iter->second->RemoveObserver(observer);
    if (!map_iter->second->size()) {
      delete map_iter->second;
      network_observers_.erase(map_iter++);
    } else {
      ++map_iter;
    }
  }
}

void NetworkLibraryImplBase::AddNetworkDeviceObserver(
    const std::string& device_path, NetworkDeviceObserver* observer) {
  // First, add the observer to the callback map.
  NetworkDeviceObserverMap::iterator iter =
      network_device_observers_.find(device_path);
  NetworkDeviceObserverList* oblist;
  if (iter != network_device_observers_.end()) {
    oblist = iter->second;
  } else {
    oblist = new NetworkDeviceObserverList();
    network_device_observers_[device_path] = oblist;
  }
  if (!oblist->HasObserver(observer))
    oblist->AddObserver(observer);
  MonitorNetworkDeviceStart(device_path);
}

void NetworkLibraryImplBase::RemoveNetworkDeviceObserver(
    const std::string& device_path, NetworkDeviceObserver* observer) {
  DCHECK(device_path.size());
  NetworkDeviceObserverMap::iterator map_iter =
      network_device_observers_.find(device_path);
  if (map_iter != network_device_observers_.end()) {
    map_iter->second->RemoveObserver(observer);
  }
}

void NetworkLibraryImplBase::DeleteDeviceFromDeviceObserversMap(
    const std::string& device_path) {
  // Delete all device observers associated with this device.
  NetworkDeviceObserverMap::iterator map_iter =
      network_device_observers_.find(device_path);
  if (map_iter != network_device_observers_.end()) {
    delete map_iter->second;
    network_device_observers_.erase(map_iter);
  }
}

//////////////////////////////////////////////////////////////////////////////

void NetworkLibraryImplBase::Lock() {
  if (is_locked_)
    return;
  is_locked_ = true;
  NotifyNetworkManagerChanged(true);  // Forced update.
}

void NetworkLibraryImplBase::Unlock() {
  DCHECK(is_locked_);
  if (!is_locked_)
    return;
  is_locked_ = false;
  NotifyNetworkManagerChanged(true);  // Forced update.
}

bool NetworkLibraryImplBase::IsLocked() {
  return is_locked_;
}

//////////////////////////////////////////////////////////////////////////////

void NetworkLibraryImplBase::AddCellularDataPlanObserver(
    CellularDataPlanObserver* observer) {
  if (!data_plan_observers_.HasObserver(observer))
    data_plan_observers_.AddObserver(observer);
}

void NetworkLibraryImplBase::RemoveCellularDataPlanObserver(
    CellularDataPlanObserver* observer) {
  data_plan_observers_.RemoveObserver(observer);
}

void NetworkLibraryImplBase::AddPinOperationObserver(
    PinOperationObserver* observer) {
  if (!pin_operation_observers_.HasObserver(observer))
    pin_operation_observers_.AddObserver(observer);
}

void NetworkLibraryImplBase::RemovePinOperationObserver(
    PinOperationObserver* observer) {
  pin_operation_observers_.RemoveObserver(observer);
}

void NetworkLibraryImplBase::AddUserActionObserver(
    UserActionObserver* observer) {
  if (!user_action_observers_.HasObserver(observer))
    user_action_observers_.AddObserver(observer);
}

void NetworkLibraryImplBase::RemoveUserActionObserver(
    UserActionObserver* observer) {
  user_action_observers_.RemoveObserver(observer);
}

// Use flimflam's ordering of the services to determine which type of
// network to return (i.e. don't assume priority of network types).
// Note: This does not include any virtual networks.
const Network* NetworkLibraryImplBase::active_network() const {
  Network* result = NULL;
  if (ethernet_ && ethernet_->is_active())
    result = ethernet_;
  if ((active_wifi_ && active_wifi_->is_active()) &&
      (!result ||
       active_wifi_->priority_order_ < result->priority_order_))
    result = active_wifi_;
  if ((active_cellular_ && active_cellular_->is_active()) &&
      (!result ||
       active_cellular_->priority_order_ < result->priority_order_))
    result = active_cellular_;
  return result;
}

const Network* NetworkLibraryImplBase::connected_network() const {
  Network* result = NULL;
  if (ethernet_ && ethernet_->connected())
    result = ethernet_;
  if ((active_wifi_ && active_wifi_->connected()) &&
      (!result ||
       active_wifi_->priority_order_ < result->priority_order_))
    result = active_wifi_;
  if ((active_cellular_ && active_cellular_->connected()) &&
      (!result ||
       active_cellular_->priority_order_ < result->priority_order_))
    result = active_cellular_;
  return result;
}

// Connecting order in logical prefernce.
const Network* NetworkLibraryImplBase::connecting_network() const {
  if (ethernet_connecting())
    return ethernet_network();
  else if (wifi_connecting())
    return wifi_network();
  else if (cellular_connecting())
    return cellular_network();
  return NULL;
}

// Returns the IP address for the active network.
// TODO(stevenjb): Fix this for VPNs. See chromium-os:13972.
const std::string& NetworkLibraryImplBase::IPAddress() const {
  const Network* result = active_network();
  if (!result)
    result = connected_network();  // happens if we are connected to a VPN.
  if (!result)
    result = ethernet_;  // Use non active ethernet addr if no active network.
  if (result)
    return result->ip_address();
  static std::string null_address("0.0.0.0");
  return null_address;
}

/////////////////////////////////////////////////////////////////////////////

const NetworkDevice* NetworkLibraryImplBase::FindNetworkDeviceByPath(
    const std::string& path) const {
  NetworkDeviceMap::const_iterator iter = device_map_.find(path);
  if (iter != device_map_.end())
    return iter->second;
  LOG(WARNING) << "Device path not found: " << path;
  return NULL;
}

NetworkDevice* NetworkLibraryImplBase::FindNetworkDeviceByPath(
    const std::string& path) {
  NetworkDeviceMap::iterator iter = device_map_.find(path);
  if (iter != device_map_.end())
    return iter->second;
  LOG(WARNING) << "Device path not found: " << path;
  return NULL;
}

const NetworkDevice* NetworkLibraryImplBase::FindCellularDevice() const {
  for (NetworkDeviceMap::const_iterator iter = device_map_.begin();
       iter != device_map_.end(); ++iter) {
    if (iter->second->type() == TYPE_CELLULAR)
      return iter->second;
  }
  return NULL;
}

const NetworkDevice* NetworkLibraryImplBase::FindEthernetDevice() const {
  for (NetworkDeviceMap::const_iterator iter = device_map_.begin();
       iter != device_map_.end(); ++iter) {
    if (iter->second->type() == TYPE_ETHERNET)
      return iter->second;
  }
  return NULL;
}

const NetworkDevice* NetworkLibraryImplBase::FindWifiDevice() const {
  for (NetworkDeviceMap::const_iterator iter = device_map_.begin();
       iter != device_map_.end(); ++iter) {
    if (iter->second->type() == TYPE_WIFI)
      return iter->second;
  }
  return NULL;
}

Network* NetworkLibraryImplBase::FindNetworkByPath(
    const std::string& path) const {
  NetworkMap::const_iterator iter = network_map_.find(path);
  if (iter != network_map_.end())
    return iter->second;
  return NULL;
}

WirelessNetwork* NetworkLibraryImplBase::FindWirelessNetworkByPath(
    const std::string& path) const {
  Network* network = FindNetworkByPath(path);
  if (network &&
      (network->type() == TYPE_WIFI || network->type() == TYPE_CELLULAR))
    return static_cast<WirelessNetwork*>(network);
  return NULL;
}

WifiNetwork* NetworkLibraryImplBase::FindWifiNetworkByPath(
    const std::string& path) const {
  Network* network = FindNetworkByPath(path);
  if (network && network->type() == TYPE_WIFI)
    return static_cast<WifiNetwork*>(network);
  return NULL;
}

CellularNetwork* NetworkLibraryImplBase::FindCellularNetworkByPath(
    const std::string& path) const {
  Network* network = FindNetworkByPath(path);
  if (network && network->type() == TYPE_CELLULAR)
    return static_cast<CellularNetwork*>(network);
  return NULL;
}

VirtualNetwork* NetworkLibraryImplBase::FindVirtualNetworkByPath(
    const std::string& path) const {
  Network* network = FindNetworkByPath(path);
  if (network && network->type() == TYPE_VPN)
    return static_cast<VirtualNetwork*>(network);
  return NULL;
}

Network* NetworkLibraryImplBase::FindNetworkFromRemembered(
    const Network* remembered) const {
  NetworkMap::const_iterator found =
      network_unique_id_map_.find(remembered->unique_id());
  if (found != network_unique_id_map_.end())
    return found->second;
  return NULL;
}

Network* NetworkLibraryImplBase::FindRememberedFromNetwork(
    const Network* network) const {
  for (NetworkMap::const_iterator iter = remembered_network_map_.begin();
       iter != remembered_network_map_.end(); ++iter) {
    if (iter->second->unique_id() == network->unique_id())
      return iter->second;
  }
  return NULL;
}

Network* NetworkLibraryImplBase::FindRememberedNetworkByPath(
    const std::string& path) const {
  NetworkMap::const_iterator iter = remembered_network_map_.find(path);
  if (iter != remembered_network_map_.end())
    return iter->second;
  return NULL;
}

////////////////////////////////////////////////////////////////////////////
// Data Plans.

const CellularDataPlanVector* NetworkLibraryImplBase::GetDataPlans(
    const std::string& path) const {
  CellularDataPlanMap::const_iterator iter = data_plan_map_.find(path);
  if (iter == data_plan_map_.end())
    return NULL;
  // If we need a new plan, then ignore any data plans we have.
  CellularNetwork* cellular = FindCellularNetworkByPath(path);
  if (cellular && cellular->needs_new_plan())
    return NULL;
  return iter->second;
}

const CellularDataPlan* NetworkLibraryImplBase::GetSignificantDataPlan(
    const std::string& path) const {
  const CellularDataPlanVector* plans = GetDataPlans(path);
  if (plans)
    return GetSignificantDataPlanFromVector(plans);
  return NULL;
}

const CellularDataPlan* NetworkLibraryImplBase::
GetSignificantDataPlanFromVector(const CellularDataPlanVector* plans) const {
  const CellularDataPlan* significant = NULL;
  for (CellularDataPlanVector::const_iterator iter = plans->begin();
       iter != plans->end(); ++iter) {
    // Set significant to the first plan or to first non metered base plan.
    if (significant == NULL ||
        significant->plan_type == CELLULAR_DATA_PLAN_METERED_BASE)
      significant = *iter;
  }
  return significant;
}

CellularNetwork::DataLeft NetworkLibraryImplBase::GetDataLeft(
    CellularDataPlanVector* data_plans) {
  const CellularDataPlan* plan = GetSignificantDataPlanFromVector(data_plans);
  if (!plan)
    return CellularNetwork::DATA_UNKNOWN;
  if (plan->plan_type == CELLULAR_DATA_PLAN_UNLIMITED) {
    base::TimeDelta remaining = plan->remaining_time();
    if (remaining <= base::TimeDelta::FromSeconds(0))
      return CellularNetwork::DATA_NONE;
    if (remaining <= base::TimeDelta::FromSeconds(kCellularDataVeryLowSecs))
      return CellularNetwork::DATA_VERY_LOW;
    if (remaining <= base::TimeDelta::FromSeconds(kCellularDataLowSecs))
      return CellularNetwork::DATA_LOW;
    return CellularNetwork::DATA_NORMAL;
  } else if (plan->plan_type == CELLULAR_DATA_PLAN_METERED_PAID ||
             plan->plan_type == CELLULAR_DATA_PLAN_METERED_BASE) {
    int64 remaining = plan->remaining_data();
    if (remaining <= 0)
      return CellularNetwork::DATA_NONE;
    if (remaining <= kCellularDataVeryLowBytes)
      return CellularNetwork::DATA_VERY_LOW;
    // For base plans, we do not care about low data.
    if (remaining <= kCellularDataLowBytes &&
        plan->plan_type != CELLULAR_DATA_PLAN_METERED_BASE)
      return CellularNetwork::DATA_LOW;
    return CellularNetwork::DATA_NORMAL;
  }
  return CellularNetwork::DATA_UNKNOWN;
}

void NetworkLibraryImplBase::UpdateCellularDataPlan(
    const std::string& service_path,
    const CellularDataPlanList* data_plan_list) {
  VLOG(1) << "Updating cellular data plans for: " << service_path;
  CellularDataPlanVector* data_plans = NULL;
  // Find and delete any existing data plans associated with |service_path|.
  CellularDataPlanMap::iterator found = data_plan_map_.find(service_path);
  if (found != data_plan_map_.end()) {
    data_plans = found->second;
    data_plans->reset();  // This will delete existing data plans.
  } else {
    data_plans = new CellularDataPlanVector;
    data_plan_map_[service_path] = data_plans;
  }
  for (size_t i = 0; i < data_plan_list->plans_size; ++i) {
    const CellularDataPlanInfo* info(data_plan_list->GetCellularDataPlan(i));
    CellularDataPlan* plan = new CellularDataPlan(*info);
    data_plans->push_back(plan);
    VLOG(2) << " Plan: " << plan->GetPlanDesciption()
            << " : " << plan->GetDataRemainingDesciption();
  }
  // Now, update any matching cellular network's cached data
  CellularNetwork* cellular = FindCellularNetworkByPath(service_path);
  if (cellular) {
    CellularNetwork::DataLeft data_left;
    // If the network needs a new plan, then there's no data.
    if (cellular->needs_new_plan())
      data_left = CellularNetwork::DATA_NONE;
    else
      data_left = GetDataLeft(data_plans);
    VLOG(2) << " Data left: " << data_left
            << " Need plan: " << cellular->needs_new_plan();
    cellular->set_data_left(data_left);
  }
  NotifyCellularDataPlanChanged();
}

void NetworkLibraryImplBase::SignalCellularPlanPayment() {
  DCHECK(!HasRecentCellularPlanPayment());
  cellular_plan_payment_time_ = base::Time::Now();
}

bool NetworkLibraryImplBase::HasRecentCellularPlanPayment() {
  return (base::Time::Now() -
          cellular_plan_payment_time_).InHours() < kRecentPlanPaymentHours;
}

std::string NetworkLibraryImplBase::GetCellularHomeCarrierId() const {
  const NetworkDevice* cellular = FindCellularDevice();
  if (cellular)
    return cellular->home_provider_id();
  return std::string();
}

/////////////////////////////////////////////////////////////////////////////
// Profiles.

bool NetworkLibraryImplBase::HasProfileType(NetworkProfileType type) const {
  for (NetworkProfileList::const_iterator iter = profile_list_.begin();
       iter != profile_list_.end(); ++iter) {
    if ((*iter).type == type)
      return true;
  }
  return false;
}

// Note: currently there is no way to change the profile of a network
// unless it is visible (i.e. in network_map_). We change the profile by
// setting the Profile property of the visible network.
// See chromium-os:15523.
void NetworkLibraryImplBase::SetNetworkProfile(
    const std::string& service_path, NetworkProfileType type) {
  Network* network = FindNetworkByPath(service_path);
  if (!network) {
    LOG(WARNING) << "SetNetworkProfile: Network not found: " << service_path;
    return;
  }
  if (network->profile_type() == type) {
    LOG(WARNING) << "Remembered Network: " << service_path
                 << " already in profile: " << type;
    return;
  }
  if (network->RequiresUserProfile() && type == PROFILE_SHARED) {
    LOG(WARNING) << "SetNetworkProfile to Shared for non sharable network: "
                 << service_path;
    return;
  }
  VLOG(1) << "Setting network: " << network->name()
          << " to profile type: " << type;
  SetProfileType(network, type);
  NotifyNetworkManagerChanged(false);
}

/////////////////////////////////////////////////////////////////////////////
// Connect to an existing network.

bool NetworkLibraryImplBase::CanConnectToNetwork(const Network* network) const {
  if (!HasProfileType(PROFILE_USER) && network->RequiresUserProfile())
    return false;
  return true;
}

// 1. Request a connection to an existing wifi network.
// Use |shared| to pass along the desired profile type.
void NetworkLibraryImplBase::ConnectToWifiNetwork(
    WifiNetwork* wifi, bool shared) {
  NetworkConnectStartWifi(wifi, shared ? PROFILE_SHARED : PROFILE_USER);
}

// 1. Request a connection to an existing wifi network.
void NetworkLibraryImplBase::ConnectToWifiNetwork(WifiNetwork* wifi) {
  NetworkConnectStartWifi(wifi, PROFILE_NONE);
}

// 1. Connect to a cellular network.
void NetworkLibraryImplBase::ConnectToCellularNetwork(
    CellularNetwork* cellular) {
  NetworkConnectStart(cellular, PROFILE_NONE);
}

// 1. Connect to an existing virtual network.
void NetworkLibraryImplBase::ConnectToVirtualNetwork(VirtualNetwork* vpn) {
  NetworkConnectStartVPN(vpn);
}

// 2. Start the connection.
void NetworkLibraryImplBase::NetworkConnectStartWifi(
    WifiNetwork* wifi, NetworkProfileType profile_type) {
  // This will happen if a network resets, gets out of range or is forgotten.
  if (wifi->user_passphrase_ != wifi->passphrase_ ||
      wifi->passphrase_required())
    wifi->SetPassphrase(wifi->user_passphrase_);
  // For enterprise 802.1X networks, always provide TPM PIN when available.
  // flimflam uses the PIN if it needs to access certificates in the TPM and
  // ignores it otherwise.
  if (wifi->encryption() == SECURITY_8021X) {
    // If the TPM initialization has not completed, GetTpmPin() will return
    // an empty value, in which case we do not want to clear the PIN since
    // that will cause flimflam to flag the network as unconfigured.
    // TODO(stevenjb): We may want to delay attempting to connect, or fail
    // immediately, rather than let the network layer attempt a connection.
    std::string tpm_pin = GetTpmPin();
    if (!tpm_pin.empty())
      wifi->SetCertificatePin(tpm_pin);
  }
  NetworkConnectStart(wifi, profile_type);
}

void NetworkLibraryImplBase::NetworkConnectStartVPN(VirtualNetwork* vpn) {
  // flimflam needs the TPM PIN for some VPN networks to access client
  // certificates, and ignores the PIN if it doesn't need them. Only set this
  // if the TPM is ready (see comment in NetworkConnectStartWifi).
  std::string tpm_pin = GetTpmPin();
  if (!tpm_pin.empty()) {
    std::string tpm_slot = GetTpmSlot();
    vpn->SetCertificateSlotAndPin(tpm_slot, tpm_pin);
  }
  NetworkConnectStart(vpn, PROFILE_NONE);
}

void NetworkLibraryImplBase::NetworkConnectStart(
    Network* network, NetworkProfileType profile_type) {
  DCHECK(network);
  // In order to be certain to trigger any notifications, set the connecting
  // state locally and notify observers. Otherwise there might be a state
  // change without a forced notify.
  network->set_connecting(true);
  NotifyNetworkManagerChanged(true);  // Forced update.
  VLOG(1) << "Requesting connect to network: " << network->service_path()
          << " profile type: " << profile_type;
  // Specify the correct profile for wifi networks (if specified or unset).
  if (network->type() == TYPE_WIFI &&
      (profile_type != PROFILE_NONE ||
       network->profile_type() == PROFILE_NONE)) {
    if (network->RequiresUserProfile())
      profile_type = PROFILE_USER;  // Networks with certs can not be shared.
    else if (profile_type == PROFILE_NONE)
      profile_type = PROFILE_SHARED;  // Other networks are shared by default.
    std::string profile_path = GetProfilePath(profile_type);
    if (!profile_path.empty()) {
      if (profile_path != network->profile_path())
        network->SetProfilePath(profile_path);
    } else if (profile_type == PROFILE_USER) {
      // The user profile was specified but is not available (i.e. pre-login).
      // Add this network to the list of networks to move to the user profile
      // when it becomes available.
      VLOG(1) << "Queuing: " << network->name() << " to user_networks list.";
      user_networks_.push_back(network->service_path());
    }
  }
  CallConnectToNetwork(network);
}

// 3. Start the connection attempt for Network.
// Must Call NetworkConnectCompleted when the connection attept completes.
// virtual void CallConnectToNetwork(Network* network) = 0;

// 4. Complete the connection.
void NetworkLibraryImplBase::NetworkConnectCompleted(
    Network* network, NetworkConnectStatus status) {
  DCHECK(network);
  if (status != CONNECT_SUCCESS) {
    // This will trigger the connection failed notification.
    // TODO(stevenjb): Remove if chromium-os:13203 gets fixed.
    network->SetState(STATE_FAILURE);
    if (status == CONNECT_BAD_PASSPHRASE) {
      network->set_error(ERROR_BAD_PASSPHRASE);
    } else {
      network->set_error(ERROR_CONNECT_FAILED);
    }
    NotifyNetworkManagerChanged(true);  // Forced update.
    return;
  }

  VLOG(1) << "Connected to service: " << network->name();

  // If the user asked not to save credentials, flimflam will have
  // forgotten them.  Wipe our cache as well.
  if (!network->save_credentials())
    network->EraseCredentials();

  // Update local cache and notify listeners.
  if (network->type() == TYPE_WIFI) {
    active_wifi_ = static_cast<WifiNetwork*>(network);
  } else if (network->type() == TYPE_CELLULAR) {
    active_cellular_ = static_cast<CellularNetwork*>(network);
  } else if (network->type() == TYPE_VPN) {
    active_virtual_ = static_cast<VirtualNetwork*>(network);
  } else {
    LOG(ERROR) << "Network of unexpected type: " << network->type();
  }

  // Notify observers.
  NotifyNetworkManagerChanged(true);  // Forced update.
  NotifyUserConnectionInitiated(network);
}

/////////////////////////////////////////////////////////////////////////////
// Request a network and connect to it.

// 1. Connect to an unconfigured or unlisted wifi network.
// This needs to request information about the named service.
// The connection attempt will occur in the callback.
void NetworkLibraryImplBase::ConnectToUnconfiguredWifiNetwork(
    const std::string& ssid,
    ConnectionSecurity security,
    const std::string& passphrase,
    const EAPConfigData* eap_config,
    bool save_credentials,
    bool shared) {
  // Store the connection data to be used by the callback.
  connect_data_.security = security;
  connect_data_.service_name = ssid;
  connect_data_.passphrase = passphrase;
  connect_data_.save_credentials = save_credentials;
  connect_data_.profile_type = shared ? PROFILE_SHARED : PROFILE_USER;
  if (security == SECURITY_8021X) {
    DCHECK(eap_config);
    connect_data_.service_name = ssid;
    connect_data_.eap_method = eap_config->method;
    connect_data_.eap_auth = eap_config->auth;
    connect_data_.server_ca_cert_nss_nickname =
        eap_config->server_ca_cert_nss_nickname;
    connect_data_.eap_use_system_cas = eap_config->use_system_cas;
    connect_data_.client_cert_pkcs11_id =
        eap_config->client_cert_pkcs11_id;
    connect_data_.eap_identity = eap_config->identity;
    connect_data_.eap_anonymous_identity = eap_config->anonymous_identity;
  }

  CallRequestWifiNetworkAndConnect(ssid, security);
}

// 1. Connect to a virtual network with a PSK.
void NetworkLibraryImplBase::ConnectToVirtualNetworkPSK(
    const std::string& service_name,
    const std::string& server_hostname,
    const std::string& psk,
    const std::string& username,
    const std::string& user_passphrase) {
  // Store the connection data to be used by the callback.
  connect_data_.service_name = service_name;
  connect_data_.psk_key = psk;
  connect_data_.server_hostname = server_hostname;
  connect_data_.psk_username = username;
  connect_data_.passphrase = user_passphrase;
  CallRequestVirtualNetworkAndConnect(
      service_name, server_hostname,
      VirtualNetwork::PROVIDER_TYPE_L2TP_IPSEC_PSK);
}

// 1. Connect to a virtual network with a user cert.
void NetworkLibraryImplBase::ConnectToVirtualNetworkCert(
    const std::string& service_name,
    const std::string& server_hostname,
    const std::string& server_ca_cert_nss_nickname,
    const std::string& client_cert_pkcs11_id,
    const std::string& username,
    const std::string& user_passphrase) {
  // Store the connection data to be used by the callback.
  connect_data_.service_name = service_name;
  connect_data_.server_hostname = server_hostname;
  connect_data_.server_ca_cert_nss_nickname = server_ca_cert_nss_nickname;
  connect_data_.client_cert_pkcs11_id = client_cert_pkcs11_id;
  connect_data_.psk_username = username;
  connect_data_.passphrase = user_passphrase;
  CallRequestVirtualNetworkAndConnect(
      service_name, server_hostname,
      VirtualNetwork::PROVIDER_TYPE_L2TP_IPSEC_USER_CERT);
}

// 2. Requests a WiFi Network by SSID and security.
// Calls ConnectToWifiNetworkUsingConnectData if network request succeeds.
// virtual void CallRequestWifiNetworkAndConnect(
//     const std::string& ssid, ConnectionSecurity security) = 0;

// 2. Requests a Virtual Network by service name, etc.
// Calls ConnectToVirtualNetworkUsingConnectData if network request succeeds.
// virtual void CallRequestVirtualNetworkAndConnect(
//     const std::string& service_name,
//     const std::string& server_hostname,
//     VirtualNetwork::ProviderType provider_type) = 0;

// 3. Sets network properties stored in ConnectData and calls
// NetworkConnectStart.
void NetworkLibraryImplBase::ConnectToWifiNetworkUsingConnectData(
    WifiNetwork* wifi) {
  ConnectData& data = connect_data_;
  if (wifi->name() != data.service_name) {
    LOG(WARNING) << "WiFi network name does not match ConnectData: "
                 << wifi->name() << " != " << data.service_name;
    return;
  }
  wifi->set_added(true);
  if (data.security == SECURITY_8021X) {
    // Enterprise 802.1X EAP network.
    wifi->SetEAPMethod(data.eap_method);
    wifi->SetEAPPhase2Auth(data.eap_auth);
    wifi->SetEAPServerCaCertNssNickname(data.server_ca_cert_nss_nickname);
    wifi->SetEAPUseSystemCAs(data.eap_use_system_cas);
    wifi->SetEAPClientCertPkcs11Id(data.client_cert_pkcs11_id);
    wifi->SetEAPIdentity(data.eap_identity);
    wifi->SetEAPAnonymousIdentity(data.eap_anonymous_identity);
    wifi->SetEAPPassphrase(data.passphrase);
    wifi->SetSaveCredentials(data.save_credentials);
  } else {
    // Ordinary, non-802.1X network.
    wifi->SetPassphrase(data.passphrase);
  }

  NetworkConnectStartWifi(wifi, data.profile_type);
}

// 3. Sets network properties stored in ConnectData and calls
// ConnectToVirtualNetwork.
void NetworkLibraryImplBase::ConnectToVirtualNetworkUsingConnectData(
    VirtualNetwork* vpn) {
  ConnectData& data = connect_data_;
  if (vpn->name() != data.service_name) {
    LOG(WARNING) << "Virtual network name does not match ConnectData: "
                 << vpn->name() << " != " << data.service_name;
    return;
  }

  vpn->set_added(true);
  if (!data.server_hostname.empty())
    vpn->set_server_hostname(data.server_hostname);
  vpn->SetCACertNSS(data.server_ca_cert_nss_nickname);
  vpn->SetClientCertID(data.client_cert_pkcs11_id);
  vpn->SetPSKPassphrase(data.psk_key);
  vpn->SetUsername(data.psk_username);
  vpn->SetUserPassphrase(data.passphrase);

  NetworkConnectStartVPN(vpn);
}

/////////////////////////////////////////////////////////////////////////////

// Called from DisconnectFromNetwork().
void NetworkLibraryImplBase::NetworkDisconnectCompleted(
    const std::string& service_path) {
  VLOG(1) << "Disconnect from network: " << service_path;
  Network* network = FindNetworkByPath(service_path);
  if (network) {
    network->set_connected(false);
    if (network == active_wifi_)
      active_wifi_ = NULL;
    else if (network == active_cellular_)
      active_cellular_ = NULL;
    else if (network == active_virtual_)
      active_virtual_ = NULL;
    NotifyNetworkManagerChanged(true);  // Forced update.
  }
}

void NetworkLibraryImplBase::ForgetNetwork(const std::string& service_path) {
  // Remove network from remembered list and notify observers.
  DeleteRememberedNetwork(service_path);
  NotifyNetworkManagerChanged(true);  // Forced update.
}

/////////////////////////////////////////////////////////////////////////////

void NetworkLibraryImplBase::EnableEthernetNetworkDevice(bool enable) {
  if (is_locked_)
    return;
  CallEnableNetworkDeviceType(TYPE_ETHERNET, enable);
}

void NetworkLibraryImplBase::EnableWifiNetworkDevice(bool enable) {
  if (is_locked_)
    return;
  CallEnableNetworkDeviceType(TYPE_WIFI, enable);
}

void NetworkLibraryImplBase::EnableCellularNetworkDevice(bool enable) {
  if (is_locked_)
    return;
  CallEnableNetworkDeviceType(TYPE_CELLULAR, enable);
}

void NetworkLibraryImplBase::SwitchToPreferredNetwork() {
  // If current network (if any) is not preferred, check network service list to
  // see if the first not connected network is preferred and set to autoconnect.
  // If so, connect to it.
  if (!wifi_enabled() || (active_wifi_ && active_wifi_->preferred()))
    return;
  for (WifiNetworkVector::const_iterator it = wifi_networks_.begin();
      it != wifi_networks_.end(); ++it) {
    WifiNetwork* wifi = *it;
    if (wifi->connected() || wifi->connecting())  // Skip connected/connecting.
      continue;
    if (!wifi->preferred())  // All preferred networks are sorted in front.
      break;
    if (wifi->auto_connect()) {
      ConnectToWifiNetwork(wifi);
      break;
    }
  }
}


////////////////////////////////////////////////////////////////////////////
// Network list management functions.

// Note: sometimes flimflam still returns networks when the device type is
// disabled. Always check the appropriate enabled() state before adding
// networks to a list or setting an active network so that we do not show them
// in the UI.

// This relies on services being requested from flimflam in priority order,
// and the updates getting processed and received in order.
void NetworkLibraryImplBase::UpdateActiveNetwork(Network* network) {
  ConnectionType type(network->type());
  if (type == TYPE_ETHERNET) {
    if (ethernet_enabled()) {
      // Set ethernet_ to the first connected ethernet service, or the first
      // disconnected ethernet service if none are connected.
      if (ethernet_ == NULL || !ethernet_->connected())
        ethernet_ = static_cast<EthernetNetwork*>(network);
    }
  } else if (type == TYPE_WIFI) {
    if (wifi_enabled()) {
      // Set active_wifi_ to the first connected or connecting wifi service.
      if (active_wifi_ == NULL && network->connecting_or_connected())
        active_wifi_ = static_cast<WifiNetwork*>(network);
    }
  } else if (type == TYPE_CELLULAR) {
    if (cellular_enabled()) {
      // Set active_cellular_ to first connected/connecting celluar service.
      if (active_cellular_ == NULL && network->connecting_or_connected())
        active_cellular_ = static_cast<CellularNetwork*>(network);
    }
  } else if (type == TYPE_VPN) {
    // Set active_virtual_ to the first connected or connecting vpn service.
    if (active_virtual_ == NULL && network->connecting_or_connected())
      active_virtual_ = static_cast<VirtualNetwork*>(network);
  }
}

void NetworkLibraryImplBase::AddNetwork(Network* network) {
  std::pair<NetworkMap::iterator, bool> result =
      network_map_.insert(std::make_pair(network->service_path(), network));
  DCHECK(result.second);  // Should only get called with new network.
  VLOG(2) << "Adding Network: " << network->service_path()
          << " (" << network->name() << ")";
  ConnectionType type(network->type());
  if (type == TYPE_WIFI) {
    if (wifi_enabled())
      wifi_networks_.push_back(static_cast<WifiNetwork*>(network));
  } else if (type == TYPE_CELLULAR) {
    if (cellular_enabled())
      cellular_networks_.push_back(static_cast<CellularNetwork*>(network));
  } else if (type == TYPE_VPN) {
    virtual_networks_.push_back(static_cast<VirtualNetwork*>(network));
  }
  // Do not set the active network here. Wait until we parse the network.
}

// Deletes a network. It must already be removed from any lists.
void NetworkLibraryImplBase::DeleteNetwork(Network* network) {
  CHECK(network_map_.find(network->service_path()) == network_map_.end());
  if (network->type() == TYPE_CELLULAR) {
    // Find and delete any existing data plans associated with |service_path|.
    CellularDataPlanMap::iterator found =
        data_plan_map_.find(network->service_path());
    if (found != data_plan_map_.end()) {
      CellularDataPlanVector* data_plans = found->second;
      delete data_plans;
      data_plan_map_.erase(found);
    }
  }
  delete network;
}

void NetworkLibraryImplBase::AddRememberedNetwork(Network* network) {
  std::pair<NetworkMap::iterator, bool> result =
      remembered_network_map_.insert(
          std::make_pair(network->service_path(), network));
  DCHECK(result.second);  // Should only get called with new network.
  if (network->type() == TYPE_WIFI) {
    remembered_wifi_networks_.push_back(
        static_cast<WifiNetwork*>(network));
  } else if (network->type() == TYPE_VPN) {
    remembered_virtual_networks_.push_back(
        static_cast<VirtualNetwork*>(network));
  } else {
    NOTREACHED();
  }
  // Find service path in profiles. Flimflam does not set the Profile
  // property for remembered networks, only active networks.
  for (NetworkProfileList::iterator iter = profile_list_.begin();
       iter != profile_list_.end(); ++iter) {
    NetworkProfile& profile = *iter;
    if (profile.services.find(network->service_path()) !=
        profile.services.end()) {
      network->set_profile_path(profile.path);
      network->set_profile_type(profile.type);
      VLOG(1) << "AddRememberedNetwork: " << network->service_path()
              << " profile: " << profile.path;
      break;
    }
  }
  DCHECK(!network->profile_path().empty())
      << "Service path not in any profile: " << network->service_path();
}

void NetworkLibraryImplBase::DeleteRememberedNetwork(
    const std::string& service_path) {
  NetworkMap::iterator found = remembered_network_map_.find(service_path);
  if (found == remembered_network_map_.end()) {
    LOG(WARNING) << "Attempt to delete non-existant remembered network: "
                 << service_path;
    return;
  }

  // Delete remembered network from lists.
  Network* remembered_network = found->second;
  remembered_network_map_.erase(found);

  if (remembered_network->type() == TYPE_WIFI) {
    WifiNetworkVector::iterator iter = std::find(
        remembered_wifi_networks_.begin(),
        remembered_wifi_networks_.end(),
        remembered_network);
    if (iter != remembered_wifi_networks_.end())
      remembered_wifi_networks_.erase(iter);
  } else if (remembered_network->type() == TYPE_VPN) {
    VirtualNetworkVector::iterator iter = std::find(
        remembered_virtual_networks_.begin(),
        remembered_virtual_networks_.end(),
        remembered_network);
    if (iter != remembered_virtual_networks_.end())
      remembered_virtual_networks_.erase(iter);
  }
  // Delete remembered network from all profiles it is in.
  for (NetworkProfileList::iterator iter = profile_list_.begin();
       iter != profile_list_.end(); ++iter) {
    NetworkProfile& profile = *iter;
    NetworkProfile::ServiceList::iterator found =
        profile.services.find(remembered_network->service_path());
    if (found != profile.services.end()) {
      VLOG(1) << "Deleting: " << remembered_network->service_path()
              << " From: " << profile.path;
      CallDeleteRememberedNetwork(profile.path,
                                  remembered_network->service_path());
      profile.services.erase(found);
    }
  }

  // Update any associated visible network.
  Network* network = FindNetworkFromRemembered(remembered_network);
  if (network) {
    // Clear the stored credentials for any visible forgotten networks.
    network->EraseCredentials();
    SetProfileType(network, PROFILE_NONE);
  } else {
    // Network is not in visible list.
    VLOG(2) << "Remembered Network not visible: "
            << remembered_network->unique_id();
  }

  delete remembered_network;
}

Network* NetworkLibraryImplBase::CreateNewNetwork(
    ConnectionType type, const std::string& service_path) {
  switch (type) {
    case TYPE_ETHERNET: {
      EthernetNetwork* ethernet = new EthernetNetwork(service_path);
      return ethernet;
    }
    case TYPE_WIFI: {
      WifiNetwork* wifi = new WifiNetwork(service_path);
      return wifi;
    }
    case TYPE_CELLULAR: {
      CellularNetwork* cellular = new CellularNetwork(service_path);
      return cellular;
    }
    case TYPE_VPN: {
      VirtualNetwork* vpn = new VirtualNetwork(service_path);
      return vpn;
    }
    default: {
      LOG(WARNING) << "Unknown service type: " << type;
      return new Network(service_path, type);
    }
  }
}

////////////////////////////////////////////////////////////////////////////

void NetworkLibraryImplBase::ClearNetworks() {
  network_map_.clear();
  network_unique_id_map_.clear();
  ethernet_ = NULL;
  active_wifi_ = NULL;
  active_cellular_ = NULL;
  active_virtual_ = NULL;
  wifi_networks_.clear();
  cellular_networks_.clear();
  virtual_networks_.clear();
}

void NetworkLibraryImplBase::ClearRememberedNetworks() {
  remembered_network_map_.clear();
  remembered_wifi_networks_.clear();
  remembered_virtual_networks_.clear();
}

void NetworkLibraryImplBase::DeleteNetworks() {
  STLDeleteValues(&network_map_);
  ClearNetworks();
}

void NetworkLibraryImplBase::DeleteRememberedNetworks() {
  STLDeleteValues(&remembered_network_map_);
  ClearRememberedNetworks();
}

void NetworkLibraryImplBase::DeleteDevice(const std::string& device_path) {
  NetworkDeviceMap::iterator found = device_map_.find(device_path);
  if (found == device_map_.end()) {
    LOG(WARNING) << "Attempt to delete non-existant device: "
                 << device_path;
    return;
  }
  VLOG(2) << " Deleting device: " << device_path;
  NetworkDevice* device = found->second;
  device_map_.erase(found);
  delete device;
  DeleteDeviceFromDeviceObserversMap(device_path);
}

////////////////////////////////////////////////////////////////////////////

void NetworkLibraryImplBase::SetProfileType(
    Network* network, NetworkProfileType type) {
  if (type == PROFILE_NONE) {
    network->SetProfilePath(std::string());
    network->set_profile_type(PROFILE_NONE);
  } else {
    std::string profile_path = GetProfilePath(type);
    if (!profile_path.empty()) {
      network->SetProfilePath(profile_path);
      network->set_profile_type(type);
    } else {
      LOG(WARNING) << "Profile type not found: " << type;
      network->set_profile_type(PROFILE_NONE);
    }
  }
}

void NetworkLibraryImplBase::SetProfileTypeFromPath(Network* network) {
  if (network->profile_path().empty()) {
    network->set_profile_type(PROFILE_NONE);
    return;
  }
  for (NetworkProfileList::iterator iter = profile_list_.begin();
       iter != profile_list_.end(); ++iter) {
    NetworkProfile& profile = *iter;
    if (profile.path == network->profile_path()) {
      network->set_profile_type(profile.type);
      return;
    }
  }
  NOTREACHED() << "Profile path not found: " << network->profile_path();
  network->set_profile_type(PROFILE_NONE);
}

std::string NetworkLibraryImplBase::GetProfilePath(NetworkProfileType type) {
  std::string profile_path;
  for (NetworkProfileList::iterator iter = profile_list_.begin();
       iter != profile_list_.end(); ++iter) {
    NetworkProfile& profile = *iter;
    if (profile.type == type)
      profile_path = profile.path;
  }
  return profile_path;
}

////////////////////////////////////////////////////////////////////////////
// Notifications.

// We call this any time something in NetworkLibrary changes.
// TODO(stevenjb): We should consider breaking this into multiple
// notifications, e.g. connection state, devices, services, etc.
void NetworkLibraryImplBase::NotifyNetworkManagerChanged(bool force_update) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Cancel any pending signals.
  if (notify_task_) {
    notify_task_->Cancel();
    notify_task_ = NULL;
  }
  if (force_update) {
    // Signal observers now.
    SignalNetworkManagerObservers();
  } else {
    // Schedule a deleayed signal to limit the frequency of notifications.
    notify_task_ = NewRunnableMethod(
        this, &NetworkLibraryImplBase::SignalNetworkManagerObservers);
    BrowserThread::PostDelayedTask(BrowserThread::UI, FROM_HERE, notify_task_,
                                   kNetworkNotifyDelayMs);
  }
}

void NetworkLibraryImplBase::SignalNetworkManagerObservers() {
  notify_task_ = NULL;
  FOR_EACH_OBSERVER(NetworkManagerObserver,
                    network_manager_observers_,
                    OnNetworkManagerChanged(this));
  // Clear notification flags.
  for (NetworkMap::iterator iter = network_map_.begin();
       iter != network_map_.end(); ++iter) {
    iter->second->set_notify_failure(false);
  }
}

void NetworkLibraryImplBase::NotifyNetworkChanged(Network* network) {
  DCHECK(network);
  VLOG(2) << "Network changed: " << network->name();
  NetworkObserverMap::const_iterator iter = network_observers_.find(
      network->service_path());
  if (iter != network_observers_.end()) {
    FOR_EACH_OBSERVER(NetworkObserver,
                      *(iter->second),
                      OnNetworkChanged(this, network));
  } else {
    LOG(ERROR) << "Unexpected signal for unobserved network: "
               << network->name();
  }
}

void NetworkLibraryImplBase::NotifyNetworkDeviceChanged(
    NetworkDevice* device, PropertyIndex index) {
  DCHECK(device);
  VLOG(2) << "Network device changed: " << device->name();
  NetworkDeviceObserverMap::const_iterator iter =
      network_device_observers_.find(device->device_path());
  if (iter != network_device_observers_.end()) {
    NetworkDeviceObserverList* device_observer_list = iter->second;
    if (index == PROPERTY_INDEX_FOUND_NETWORKS) {
      FOR_EACH_OBSERVER(NetworkDeviceObserver,
                        *device_observer_list,
                        OnNetworkDeviceFoundNetworks(this, device));
    }
    FOR_EACH_OBSERVER(NetworkDeviceObserver,
                      *device_observer_list,
                      OnNetworkDeviceChanged(this, device));
  } else {
    LOG(ERROR) << "Unexpected signal for unobserved device: "
               << device->name();
  }
}

void NetworkLibraryImplBase::NotifyCellularDataPlanChanged() {
  FOR_EACH_OBSERVER(CellularDataPlanObserver,
                    data_plan_observers_,
                    OnCellularDataPlanChanged(this));
}

void NetworkLibraryImplBase::NotifyPinOperationCompleted(
    PinOperationError error) {
  FOR_EACH_OBSERVER(PinOperationObserver,
                    pin_operation_observers_,
                    OnPinOperationCompleted(this, error));
  sim_operation_ = SIM_OPERATION_NONE;
}

void NetworkLibraryImplBase::NotifyUserConnectionInitiated(
    const Network* network) {
  FOR_EACH_OBSERVER(UserActionObserver,
                    user_action_observers_,
                    OnConnectionInitiated(this, network));
}

//////////////////////////////////////////////////////////////////////////////
// Pin related functions.

void NetworkLibraryImplBase::GetTpmInfo() {
  // Avoid making multiple synchronous D-Bus calls to cryptohome by caching
  // the TPM PIN, which does not change during a session.
  if (tpm_pin_.empty()) {
    if (crypto::IsTPMTokenReady()) {
      std::string tpm_label;
      crypto::GetTPMTokenInfo(&tpm_label, &tpm_pin_);
      // VLOG(1) << "TPM Label: " << tpm_label << ", PIN: " << tpm_pin_;
      // TODO(stevenjb): GetTPMTokenInfo returns a label, but the network
      // code expects a slot ID. See chromium-os:17998.
      // For now, use a hard coded, well known slot instead.
      const char kHardcodedTpmSlot[] = "0";
      tpm_slot_ = kHardcodedTpmSlot;
    } else {
      LOG(WARNING) << "TPM token not ready";
    }
  }
}

const std::string& NetworkLibraryImplBase::GetTpmSlot() {
  GetTpmInfo();
  return tpm_slot_;
}

const std::string& NetworkLibraryImplBase::GetTpmPin() {
  GetTpmInfo();
  return tpm_pin_;
}

void NetworkLibraryImplBase::FlipSimPinRequiredStateIfNeeded() {
  if (sim_operation_ != SIM_OPERATION_CHANGE_REQUIRE_PIN)
    return;

  const NetworkDevice* cellular = FindCellularDevice();
  if (cellular) {
    NetworkDevice* device = FindNetworkDeviceByPath(cellular->device_path());
    if (device->sim_pin_required() == SIM_PIN_NOT_REQUIRED)
      device->sim_pin_required_ = SIM_PIN_REQUIRED;
    else if (device->sim_pin_required() == SIM_PIN_REQUIRED)
      device->sim_pin_required_ = SIM_PIN_NOT_REQUIRED;
  }
}

////////////////////////////////////////////////////////////////////////////

class NetworkLibraryImplCros : public NetworkLibraryImplBase  {
 public:
  NetworkLibraryImplCros();
  virtual ~NetworkLibraryImplCros();

  virtual void Init() OVERRIDE;
  virtual bool IsCros() const OVERRIDE { return true; }

  //////////////////////////////////////////////////////////////////////////////
  // NetworkLibraryImplBase implementation.

  virtual void MonitorNetworkStart(const std::string& service_path) OVERRIDE;
  virtual void MonitorNetworkStop(const std::string& service_path) OVERRIDE;
  virtual void MonitorNetworkDeviceStart(
      const std::string& device_path) OVERRIDE;
  virtual void MonitorNetworkDeviceStop(
      const std::string& device_path) OVERRIDE;

  virtual void CallConnectToNetwork(Network* network) OVERRIDE;
  virtual void CallRequestWifiNetworkAndConnect(
      const std::string& ssid, ConnectionSecurity security) OVERRIDE;
  virtual void CallRequestVirtualNetworkAndConnect(
      const std::string& service_name,
      const std::string& server_hostname,
      VirtualNetwork::ProviderType provider_type) OVERRIDE;
  virtual void CallDeleteRememberedNetwork(
      const std::string& profile_path,
      const std::string& service_path) OVERRIDE;

  //////////////////////////////////////////////////////////////////////////////
  // NetworkLibrary implementation.

  virtual void ChangePin(const std::string& old_pin,
                         const std::string& new_pin) OVERRIDE;
  virtual void ChangeRequirePin(bool require_pin,
                                const std::string& pin) OVERRIDE;
  virtual void EnterPin(const std::string& pin) OVERRIDE;
  virtual void UnblockPin(const std::string& puk,
                          const std::string& new_pin) OVERRIDE;
  virtual void RequestCellularScan() OVERRIDE;
  virtual void RequestCellularRegister(const std::string& network_id) OVERRIDE;
  virtual void SetCellularDataRoamingAllowed(bool new_value) OVERRIDE;
  virtual void RequestNetworkScan() OVERRIDE;
  virtual bool GetWifiAccessPoints(WifiAccessPointVector* result) OVERRIDE;

  virtual void DisconnectFromNetwork(const Network* network) OVERRIDE;
  virtual void CallEnableNetworkDeviceType(
      ConnectionType device, bool enable) OVERRIDE;
  virtual void EnableOfflineMode(bool enable) OVERRIDE;

  virtual NetworkIPConfigVector GetIPConfigs(
      const std::string& device_path,
      std::string* hardware_address,
      HardwareAddressFormat format) OVERRIDE;
  virtual void SetIPConfig(const NetworkIPConfig& ipconfig) OVERRIDE;

  //////////////////////////////////////////////////////////////////////////////
  // Calbacks.
  static void NetworkStatusChangedHandler(
      void* object, const char* path, const char* key, const Value* value);
  void UpdateNetworkStatus(
      const char* path, const char* key, const Value* value);

  static void NetworkDevicePropertyChangedHandler(
      void* object, const char* path, const char* key, const Value* value);
  void UpdateNetworkDeviceStatus(
      const char* path, const char* key, const Value* value);

  static void PinOperationCallback(void* object,
                                   const char* path,
                                   NetworkMethodErrorType error,
                                   const char* error_message);

  static void CellularRegisterCallback(void* object,
                                       const char* path,
                                       NetworkMethodErrorType error,
                                       const char* error_message);

  static void NetworkConnectCallback(void* object,
                                     const char* service_path,
                                     NetworkMethodErrorType error,
                                     const char* error_message);

  static void WifiServiceUpdateAndConnect(
      void* object, const char* service_path, const Value* info);
  static void VPNServiceUpdateAndConnect(
      void* object, const char* service_path, const Value* info);

  static void NetworkManagerStatusChangedHandler(
      void* object, const char* path, const char* key, const Value* value);
  static void NetworkManagerUpdate(
      void* object, const char* manager_path, const Value* info);

  static void DataPlanUpdateHandler(void* object,
                                    const char* modem_service_path,
                                    const CellularDataPlanList* dataplan);

  static void NetworkServiceUpdate(
      void* object, const char* service_path, const Value* info);
  static void RememberedNetworkServiceUpdate(
      void* object, const char* service_path, const Value* info);
  static void ProfileUpdate(
      void* object, const char* profile_path, const Value* info);
  static void NetworkDeviceUpdate(
      void* object, const char* device_path, const Value* info);

 private:
  // This processes all Manager update messages.
  void NetworkManagerStatusChanged(const char* key, const Value* value);
  void ParseNetworkManager(const DictionaryValue* dict);
  void UpdateTechnologies(const ListValue* technologies, int* bitfieldp);
  void UpdateAvailableTechnologies(const ListValue* technologies);
  void UpdateEnabledTechnologies(const ListValue* technologies);
  void UpdateConnectedTechnologies(const ListValue* technologies);

  // Update network lists.
  void UpdateNetworkServiceList(const ListValue* services);
  void UpdateWatchedNetworkServiceList(const ListValue* services);
  Network* ParseNetwork(const std::string& service_path,
                        const DictionaryValue* info);

  void UpdateRememberedNetworks(const ListValue* profiles);
  void RequestRememberedNetworksUpdate();
  void UpdateRememberedServiceList(const char* profile_path,
                                   const ListValue* profile_entries);
  Network* ParseRememberedNetwork(const std::string& service_path,
                                  const DictionaryValue* info);

  // NetworkDevice list management functions.
  void UpdateNetworkDeviceList(const ListValue* devices);
  void ParseNetworkDevice(const std::string& device_path,
                          const DictionaryValue* info);

  // Empty device observer to ensure that device property updates are received.
  class NetworkLibraryDeviceObserver : public NetworkDeviceObserver {
   public:
    virtual ~NetworkLibraryDeviceObserver() {}
    virtual void OnNetworkDeviceChanged(
        NetworkLibrary* cros, const NetworkDevice* device) OVERRIDE {}
  };

  typedef std::map<std::string, chromeos::PropertyChangeMonitor>
          PropertyChangeMonitorMap;

  // For monitoring network manager status changes.
  PropertyChangeMonitor network_manager_monitor_;

  // For monitoring data plan changes to the connected cellular network.
  DataPlanUpdateMonitor data_plan_monitor_;

  // Network device observer.
  scoped_ptr<NetworkLibraryDeviceObserver> network_device_observer_;

  // Map of monitored networks.
  PropertyChangeMonitorMap montitored_networks_;

  // Map of monitored devices.
  PropertyChangeMonitorMap montitored_devices_;

  DISALLOW_COPY_AND_ASSIGN(NetworkLibraryImplCros);
};

////////////////////////////////////////////////////////////////////////////

NetworkLibraryImplCros::NetworkLibraryImplCros()
    : NetworkLibraryImplBase(),
      network_manager_monitor_(NULL),
      data_plan_monitor_(NULL) {
}

NetworkLibraryImplCros::~NetworkLibraryImplCros() {
  if (network_manager_monitor_)
    chromeos::DisconnectPropertyChangeMonitor(network_manager_monitor_);
  if (data_plan_monitor_)
    chromeos::DisconnectDataPlanUpdateMonitor(data_plan_monitor_);
  for (PropertyChangeMonitorMap::iterator iter = montitored_networks_.begin();
       iter != montitored_networks_.end(); ++iter) {
    chromeos::DisconnectPropertyChangeMonitor(iter->second);
  }
  for (PropertyChangeMonitorMap::iterator iter = montitored_devices_.begin();
       iter != montitored_devices_.end(); ++iter) {
    chromeos::DisconnectPropertyChangeMonitor(iter->second);
  }
}

void NetworkLibraryImplCros::Init() {
  // First, get the currently available networks. This data is cached
  // on the connman side, so the call should be quick.
  VLOG(1) << "Requesting initial network manager info from libcros.";
  chromeos::RequestNetworkManagerInfo(&NetworkManagerUpdate, this);
  network_manager_monitor_ =
      chromeos::MonitorNetworkManager(&NetworkManagerStatusChangedHandler,
                                      this);
  data_plan_monitor_ =
      chromeos::MonitorCellularDataPlan(&DataPlanUpdateHandler, this);
  // Always have at least one device obsever so that device updates are
  // always received.
  network_device_observer_.reset(new NetworkLibraryDeviceObserver());
}

//////////////////////////////////////////////////////////////////////////////
// NetworkLibraryImplBase implementation.

void NetworkLibraryImplCros::MonitorNetworkStart(
    const std::string& service_path) {
  if (montitored_networks_.find(service_path) == montitored_networks_.end()) {
    chromeos::PropertyChangeMonitor monitor =
        chromeos::MonitorNetworkService(&NetworkStatusChangedHandler,
                                        service_path.c_str(),
                                        this);
    montitored_networks_[service_path] = monitor;
  }
}

void NetworkLibraryImplCros::MonitorNetworkStop(
    const std::string& service_path) {
  PropertyChangeMonitorMap::iterator iter =
      montitored_networks_.find(service_path);
  if (iter != montitored_networks_.end()) {
    chromeos::DisconnectPropertyChangeMonitor(iter->second);
    montitored_networks_.erase(iter);
  }
}

void NetworkLibraryImplCros::MonitorNetworkDeviceStart(
    const std::string& device_path) {
  if (montitored_devices_.find(device_path) == montitored_devices_.end()) {
    chromeos::PropertyChangeMonitor monitor =
        chromeos::MonitorNetworkDevice(&NetworkDevicePropertyChangedHandler,
                                       device_path.c_str(),
                                       this);
    montitored_devices_[device_path] = monitor;
  }
}

void NetworkLibraryImplCros::MonitorNetworkDeviceStop(
    const std::string& device_path) {
  PropertyChangeMonitorMap::iterator iter =
      montitored_devices_.find(device_path);
  if (iter != montitored_devices_.end()) {
    chromeos::DisconnectPropertyChangeMonitor(iter->second);
    montitored_devices_.erase(iter);
  }
}

// static callback
void NetworkLibraryImplCros::NetworkStatusChangedHandler(
    void* object, const char* path, const char* key, const Value* value) {
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  networklib->UpdateNetworkStatus(path, key, value);
}

void NetworkLibraryImplCros::UpdateNetworkStatus(
    const char* path, const char* key, const Value* value) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (key == NULL || value == NULL)
    return;
  Network* network = FindNetworkByPath(path);
  if (network) {
    VLOG(2) << "UpdateNetworkStatus: " << network->name() << "." << key;
    bool prev_connected = network->connected();
    // Note: ParseValue is virtual.
    int index = property_index_parser().Get(std::string(key));
    if (!network->ParseValue(index, value)) {
      LOG(WARNING) << "UpdateNetworkStatus: Error parsing: "
                   << path << "." << key;
    }
    // If we just connected, this may have been added to remembered list.
    if (!prev_connected && network->connected())
      RequestRememberedNetworksUpdate();
    NotifyNetworkChanged(network);
    // Anything observing the manager needs to know about any service change.
    NotifyNetworkManagerChanged(false);  // Not forced.
  }
}

// static callback
void NetworkLibraryImplCros::NetworkDevicePropertyChangedHandler(
    void* object, const char* path, const char* key, const Value* value) {
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  networklib->UpdateNetworkDeviceStatus(path, key, value);
}

void NetworkLibraryImplCros::UpdateNetworkDeviceStatus(
    const char* path, const char* key, const Value* value) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (key == NULL || value == NULL)
    return;
  NetworkDevice* device = FindNetworkDeviceByPath(path);
  if (device) {
    VLOG(2) << "UpdateNetworkDeviceStatus: " << device->name() << "." << key;
    PropertyIndex index = property_index_parser().Get(std::string(key));
    if (!device->ParseValue(index, value)) {
      VLOG(1) << "UpdateNetworkDeviceStatus: Failed to parse: "
              << path << "." << key;
    } else if (index == PROPERTY_INDEX_CELLULAR_ALLOW_ROAMING) {
      bool settings_value =
          UserCrosSettingsProvider::cached_data_roaming_enabled();
      if (device->data_roaming_allowed() != settings_value) {
        // Switch back to signed settings value.
        SetCellularDataRoamingAllowed(settings_value);
        return;
      }
    }
    // Notify only observers on device property change.
    NotifyNetworkDeviceChanged(device, index);
    // If a device's power state changes, new properties may become defined.
    if (index == PROPERTY_INDEX_POWERED)
      chromeos::RequestNetworkDeviceInfo(path, &NetworkDeviceUpdate, this);
  }
}

/////////////////////////////////////////////////////////////////////////////
// NetworkLibraryImplBase connect implementation.

// static callback
void NetworkLibraryImplCros::NetworkConnectCallback(
    void* object,
    const char* service_path,
    NetworkMethodErrorType error,
    const char* error_message) {
  NetworkConnectStatus status;
  if (error == NETWORK_METHOD_ERROR_NONE) {
    status = CONNECT_SUCCESS;
  } else {
    LOG(WARNING) << "Error from ServiceConnect callback for: "
                 << service_path
                 << " Error: " << error << " Message: " << error_message;
    if (error_message &&
        strcmp(error_message, kErrorPassphraseRequiredMsg) == 0) {
      status = CONNECT_BAD_PASSPHRASE;
    } else {
      status = CONNECT_FAILED;
    }
  }
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  Network* network = networklib->FindNetworkByPath(service_path);
  if (!network) {
    LOG(ERROR) << "No network for path: " << service_path;
    return;
  }
  networklib->NetworkConnectCompleted(network, status);
}

void NetworkLibraryImplCros::CallConnectToNetwork(Network* network) {
  DCHECK(network);
  chromeos::RequestNetworkServiceConnect(network->service_path().c_str(),
                                         NetworkConnectCallback, this);
}

// static callback
void NetworkLibraryImplCros::WifiServiceUpdateAndConnect(
    void* object, const char* service_path, const Value* info) {
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  if (service_path && info) {
    DCHECK_EQ(info->GetType(), Value::TYPE_DICTIONARY);
    const DictionaryValue* dict = static_cast<const DictionaryValue*>(info);
    Network* network =
        networklib->ParseNetwork(std::string(service_path), dict);
    DCHECK_EQ(network->type(), TYPE_WIFI);
    networklib->ConnectToWifiNetworkUsingConnectData(
        static_cast<WifiNetwork*>(network));
  }
}

void NetworkLibraryImplCros::CallRequestWifiNetworkAndConnect(
    const std::string& ssid, ConnectionSecurity security) {
  // Asynchronously request service properties and call
  // WifiServiceUpdateAndConnect.
  chromeos::RequestHiddenWifiNetwork(ssid.c_str(),
                                     SecurityToString(security),
                                     WifiServiceUpdateAndConnect,
                                     this);
}

// static callback
void NetworkLibraryImplCros::VPNServiceUpdateAndConnect(
    void* object, const char* service_path, const Value* info) {
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  if (service_path && info) {
    VLOG(1) << "Connecting to new VPN Service: " << service_path;
    DCHECK_EQ(info->GetType(), Value::TYPE_DICTIONARY);
    const DictionaryValue* dict = static_cast<const DictionaryValue*>(info);
    Network* network =
        networklib->ParseNetwork(std::string(service_path), dict);
    DCHECK_EQ(network->type(), TYPE_VPN);
    networklib->ConnectToVirtualNetworkUsingConnectData(
        static_cast<VirtualNetwork*>(network));
  } else {
    LOG(WARNING) << "Unable to create VPN Service: " << service_path;
  }
}

void NetworkLibraryImplCros::CallRequestVirtualNetworkAndConnect(
    const std::string& service_name,
    const std::string& server_hostname,
    VirtualNetwork::ProviderType provider_type) {
  chromeos::RequestVirtualNetwork(service_name.c_str(),
                                  server_hostname.c_str(),
                                  ProviderTypeToString(provider_type),
                                  VPNServiceUpdateAndConnect,
                                  this);
}

void NetworkLibraryImplCros::CallDeleteRememberedNetwork(
    const std::string& profile_path,
    const std::string& service_path) {
  chromeos::DeleteServiceFromProfile(
      profile_path.c_str(), service_path.c_str());
}

//////////////////////////////////////////////////////////////////////////////
// NetworkLibrary implementation.

void NetworkLibraryImplCros::ChangePin(const std::string& old_pin,
                                       const std::string& new_pin) {
  const NetworkDevice* cellular = FindCellularDevice();
  if (!cellular) {
    NOTREACHED() << "Calling ChangePin method w/o cellular device.";
    return;
  }
  sim_operation_ = SIM_OPERATION_CHANGE_PIN;
  chromeos::RequestChangePin(cellular->device_path().c_str(),
                             old_pin.c_str(), new_pin.c_str(),
                             PinOperationCallback, this);
}

void NetworkLibraryImplCros::ChangeRequirePin(bool require_pin,
                                              const std::string& pin) {
  VLOG(1) << "ChangeRequirePin require_pin: " << require_pin
          << " pin: " << pin;
  const NetworkDevice* cellular = FindCellularDevice();
  if (!cellular) {
    NOTREACHED() << "Calling ChangeRequirePin method w/o cellular device.";
    return;
  }
  sim_operation_ = SIM_OPERATION_CHANGE_REQUIRE_PIN;
  chromeos::RequestRequirePin(cellular->device_path().c_str(),
                              pin.c_str(), require_pin,
                              PinOperationCallback, this);
}

void NetworkLibraryImplCros::EnterPin(const std::string& pin) {
  const NetworkDevice* cellular = FindCellularDevice();
  if (!cellular) {
    NOTREACHED() << "Calling EnterPin method w/o cellular device.";
    return;
  }
  sim_operation_ = SIM_OPERATION_ENTER_PIN;
  chromeos::RequestEnterPin(cellular->device_path().c_str(),
                            pin.c_str(),
                            PinOperationCallback, this);
}

void NetworkLibraryImplCros::UnblockPin(const std::string& puk,
                                        const std::string& new_pin) {
  const NetworkDevice* cellular = FindCellularDevice();
  if (!cellular) {
    NOTREACHED() << "Calling UnblockPin method w/o cellular device.";
    return;
  }
  sim_operation_ = SIM_OPERATION_UNBLOCK_PIN;
  chromeos::RequestUnblockPin(cellular->device_path().c_str(),
                              puk.c_str(), new_pin.c_str(),
                              PinOperationCallback, this);
}

// static callback
void NetworkLibraryImplCros::PinOperationCallback(
    void* object,
    const char* path,
    NetworkMethodErrorType error,
    const char* error_message) {
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  PinOperationError pin_error;
  VLOG(1) << "PinOperationCallback, error: " << error
          << " error_msg: " << error_message;
  if (error == chromeos::NETWORK_METHOD_ERROR_NONE) {
    pin_error = PIN_ERROR_NONE;
    VLOG(1) << "Pin operation completed successfuly";
    // TODO(nkostylev): Might be cleaned up and exposed in flimflam API.
    // http://crosbug.com/14253
    // Since this option state is not exposed we have to update it manually.
    networklib->FlipSimPinRequiredStateIfNeeded();
  } else {
    if (error_message &&
        (strcmp(error_message, kErrorIncorrectPinMsg) == 0 ||
         strcmp(error_message, kErrorPinRequiredMsg) == 0)) {
      pin_error = PIN_ERROR_INCORRECT_CODE;
    } else if (error_message &&
               strcmp(error_message, kErrorPinBlockedMsg) == 0) {
      pin_error = PIN_ERROR_BLOCKED;
    } else {
      pin_error = PIN_ERROR_UNKNOWN;
      NOTREACHED() << "Unknown PIN error: " << error_message;
    }
  }
  networklib->NotifyPinOperationCompleted(pin_error);
}

void NetworkLibraryImplCros::RequestCellularScan() {
  const NetworkDevice* cellular = FindCellularDevice();
  if (!cellular) {
    NOTREACHED() << "Calling RequestCellularScan method w/o cellular device.";
    return;
  }
  chromeos::ProposeScan(cellular->device_path().c_str());
}

void NetworkLibraryImplCros::RequestCellularRegister(
    const std::string& network_id) {
  const NetworkDevice* cellular = FindCellularDevice();
  if (!cellular) {
    NOTREACHED() << "Calling CellularRegister method w/o cellular device.";
    return;
  }
  chromeos::RequestCellularRegister(cellular->device_path().c_str(),
                                    network_id.c_str(),
                                    CellularRegisterCallback,
                                    this);
}

// static callback
void NetworkLibraryImplCros::CellularRegisterCallback(
    void* object,
    const char* path,
    NetworkMethodErrorType error,
    const char* error_message) {
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  // TODO(dpolukhin): Notify observers about network registration status
  // but not UI doesn't assume such notification so just ignore result.
}

void NetworkLibraryImplCros::SetCellularDataRoamingAllowed(bool new_value) {
  const NetworkDevice* cellular = FindCellularDevice();
  if (!cellular) {
    NOTREACHED() << "Calling SetCellularDataRoamingAllowed method "
        "w/o cellular device.";
    return;
  }
  scoped_ptr<Value> value(Value::CreateBooleanValue(new_value));
  chromeos::SetNetworkDeviceProperty(cellular->device_path().c_str(),
                                     kCellularAllowRoamingProperty,
                                     value.get());
}

void NetworkLibraryImplCros::RequestNetworkScan() {
  if (wifi_enabled()) {
    wifi_scanning_ = true;  // Cleared when updates are received.
    chromeos::RequestNetworkScan(kTypeWifi);
  }
  if (cellular_network())
    cellular_network()->RefreshDataPlansIfNeeded();
  // Make sure all Manager info is up to date. This will also update
  // remembered networks and visible services.
  chromeos::RequestNetworkManagerInfo(&NetworkManagerUpdate, this);
}

bool NetworkLibraryImplCros::GetWifiAccessPoints(
    WifiAccessPointVector* result) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DeviceNetworkList* network_list = chromeos::GetDeviceNetworkList();
  if (network_list == NULL)
    return false;
  result->clear();
  result->reserve(network_list->network_size);
  const base::Time now = base::Time::Now();
  for (size_t i = 0; i < network_list->network_size; ++i) {
    DCHECK(network_list->networks[i].address);
    DCHECK(network_list->networks[i].name);
    WifiAccessPoint ap;
    ap.mac_address = SafeString(network_list->networks[i].address);
    ap.name = SafeString(network_list->networks[i].name);
    ap.timestamp = now -
        base::TimeDelta::FromSeconds(network_list->networks[i].age_seconds);
    ap.signal_strength = network_list->networks[i].strength;
    ap.channel = network_list->networks[i].channel;
    result->push_back(ap);
  }
  chromeos::FreeDeviceNetworkList(network_list);
  return true;
}

void NetworkLibraryImplCros::DisconnectFromNetwork(const Network* network) {
  DCHECK(network);
  if (chromeos::DisconnectFromNetwork(network->service_path().c_str()))
    NetworkDisconnectCompleted(network->service_path());
}

void NetworkLibraryImplCros::CallEnableNetworkDeviceType(
    ConnectionType device, bool enable) {
  chromeos::RequestNetworkDeviceEnable(
      ConnectionTypeToString(device), enable);
}

void NetworkLibraryImplCros::EnableOfflineMode(bool enable) {
  // If network device is already enabled/disabled, then don't do anything.
  if (chromeos::SetOfflineMode(enable))
    offline_mode_ = enable;
}

NetworkIPConfigVector NetworkLibraryImplCros::GetIPConfigs(
    const std::string& device_path,
    std::string* hardware_address,
    HardwareAddressFormat format) {
  DCHECK(hardware_address);
  hardware_address->clear();
  NetworkIPConfigVector ipconfig_vector;
  if (!device_path.empty()) {
    IPConfigStatus* ipconfig_status = ListIPConfigs(device_path.c_str());
    if (ipconfig_status) {
      for (int i = 0; i < ipconfig_status->size; ++i) {
        IPConfig ipconfig = ipconfig_status->ips[i];
        ipconfig_vector.push_back(
            NetworkIPConfig(device_path, ipconfig.type, ipconfig.address,
                            ipconfig.netmask, ipconfig.gateway,
                            ipconfig.name_servers));
      }
      *hardware_address = ipconfig_status->hardware_address;
      FreeIPConfigStatus(ipconfig_status);
    }
  }

  for (size_t i = 0; i < hardware_address->size(); ++i)
    (*hardware_address)[i] = toupper((*hardware_address)[i]);
  if (format == FORMAT_COLON_SEPARATED_HEX) {
    if (hardware_address->size() % 2 == 0) {
      std::string output;
      for (size_t i = 0; i < hardware_address->size(); ++i) {
        if ((i != 0) && (i % 2 == 0))
          output.push_back(':');
        output.push_back((*hardware_address)[i]);
      }
      *hardware_address = output;
    }
  } else {
    DCHECK_EQ(format, FORMAT_RAW_HEX);
  }
  return ipconfig_vector;
}

void NetworkLibraryImplCros::SetIPConfig(const NetworkIPConfig& ipconfig) {
  if (ipconfig.device_path.empty())
    return;

  IPConfig* ipconfig_dhcp = NULL;
  IPConfig* ipconfig_static = NULL;

  IPConfigStatus* ipconfig_status =
      chromeos::ListIPConfigs(ipconfig.device_path.c_str());
  if (ipconfig_status) {
    for (int i = 0; i < ipconfig_status->size; ++i) {
      if (ipconfig_status->ips[i].type == chromeos::IPCONFIG_TYPE_DHCP)
        ipconfig_dhcp = &ipconfig_status->ips[i];
      else if (ipconfig_status->ips[i].type == chromeos::IPCONFIG_TYPE_IPV4)
        ipconfig_static = &ipconfig_status->ips[i];
    }
  }

  IPConfigStatus* ipconfig_status2 = NULL;
  if (ipconfig.type == chromeos::IPCONFIG_TYPE_DHCP) {
    // If switching from static to dhcp, create new dhcp ip config.
    if (!ipconfig_dhcp)
      chromeos::AddIPConfig(ipconfig.device_path.c_str(),
                            chromeos::IPCONFIG_TYPE_DHCP);
    // User wants DHCP now. So delete the static ip config.
    if (ipconfig_static)
      chromeos::RemoveIPConfig(ipconfig_static);
  } else if (ipconfig.type == chromeos::IPCONFIG_TYPE_IPV4) {
    // If switching from dhcp to static, create new static ip config.
    if (!ipconfig_static) {
      chromeos::AddIPConfig(ipconfig.device_path.c_str(),
                            chromeos::IPCONFIG_TYPE_IPV4);
      // Now find the newly created IP config.
      ipconfig_status2 =
          chromeos::ListIPConfigs(ipconfig.device_path.c_str());
      if (ipconfig_status2) {
        for (int i = 0; i < ipconfig_status2->size; ++i) {
          if (ipconfig_status2->ips[i].type == chromeos::IPCONFIG_TYPE_IPV4)
            ipconfig_static = &ipconfig_status2->ips[i];
        }
      }
    }
    if (ipconfig_static) {
      // Save any changed details.
      if (ipconfig.address != ipconfig_static->address) {
        scoped_ptr<Value> value(Value::CreateStringValue(ipconfig.address));
        chromeos::SetNetworkIPConfigProperty(ipconfig_static->path,
                                             kAddressProperty,
                                             value.get());
      }
      if (ipconfig.netmask != ipconfig_static->netmask) {
        int32 prefixlen = ipconfig.GetPrefixLength();
        if (prefixlen == -1) {
          VLOG(1) << "IP config prefixlen is invalid for netmask "
                  << ipconfig.netmask;
        } else {
          scoped_ptr<Value> value(Value::CreateIntegerValue(prefixlen));
          chromeos::SetNetworkIPConfigProperty(ipconfig_static->path,
                                               kPrefixlenProperty,
                                               value.get());
        }
      }
      if (ipconfig.gateway != ipconfig_static->gateway) {
        scoped_ptr<Value> value(Value::CreateStringValue(ipconfig.gateway));
        chromeos::SetNetworkIPConfigProperty(ipconfig_static->path,
                                             kGatewayProperty,
                                             value.get());
      }
      if (ipconfig.name_servers != ipconfig_static->name_servers) {
        scoped_ptr<Value> value(
            Value::CreateStringValue(ipconfig.name_servers));
        chromeos::SetNetworkIPConfigProperty(ipconfig_static->path,
                                             kNameServersProperty,
                                             value.get());
      }
      // Remove dhcp ip config if there is one.
      if (ipconfig_dhcp)
        chromeos::RemoveIPConfig(ipconfig_dhcp);
    }
  }

  if (ipconfig_status)
    chromeos::FreeIPConfigStatus(ipconfig_status);
  if (ipconfig_status2)
    chromeos::FreeIPConfigStatus(ipconfig_status2);
}

/////////////////////////////////////////////////////////////////////////////
// Network Manager functions.

// static
void NetworkLibraryImplCros::NetworkManagerStatusChangedHandler(
    void* object, const char* path, const char* key, const Value* value) {
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  networklib->NetworkManagerStatusChanged(key, value);
}

// This processes all Manager update messages.
void NetworkLibraryImplCros::NetworkManagerStatusChanged(
    const char* key, const Value* value) {
  CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  base::TimeTicks start = base::TimeTicks::Now();
  if (!key)
    return;
  VLOG(1) << "NetworkManagerStatusChanged: KEY=" << key;
  int index = property_index_parser().Get(std::string(key));
  switch (index) {
    case PROPERTY_INDEX_STATE:
      // Currently we ignore the network manager state.
      break;
    case PROPERTY_INDEX_AVAILABLE_TECHNOLOGIES: {
      DCHECK_EQ(value->GetType(), Value::TYPE_LIST);
      const ListValue* vlist = static_cast<const ListValue*>(value);
      UpdateAvailableTechnologies(vlist);
      break;
    }
    case PROPERTY_INDEX_ENABLED_TECHNOLOGIES: {
      DCHECK_EQ(value->GetType(), Value::TYPE_LIST);
      const ListValue* vlist = static_cast<const ListValue*>(value);
      UpdateEnabledTechnologies(vlist);
      break;
    }
    case PROPERTY_INDEX_CONNECTED_TECHNOLOGIES: {
      DCHECK_EQ(value->GetType(), Value::TYPE_LIST);
      const ListValue* vlist = static_cast<const ListValue*>(value);
      UpdateConnectedTechnologies(vlist);
      break;
    }
    case PROPERTY_INDEX_DEFAULT_TECHNOLOGY:
      // Currently we ignore DefaultTechnology.
      break;
    case PROPERTY_INDEX_OFFLINE_MODE: {
      DCHECK_EQ(value->GetType(), Value::TYPE_BOOLEAN);
      value->GetAsBoolean(&offline_mode_);
      NotifyNetworkManagerChanged(false);  // Not forced.
      break;
    }
    case PROPERTY_INDEX_ACTIVE_PROFILE: {
      std::string prev = active_profile_path_;
      DCHECK_EQ(value->GetType(), Value::TYPE_STRING);
      value->GetAsString(&active_profile_path_);
      VLOG(1) << "Active Profile: " << active_profile_path_;
      if (active_profile_path_ != prev &&
          active_profile_path_ != kSharedProfilePath)
        SwitchToPreferredNetwork();
      break;
    }
    case PROPERTY_INDEX_PROFILES: {
      DCHECK_EQ(value->GetType(), Value::TYPE_LIST);
      const ListValue* vlist = static_cast<const ListValue*>(value);
      UpdateRememberedNetworks(vlist);
      RequestRememberedNetworksUpdate();
      break;
    }
    case PROPERTY_INDEX_SERVICES: {
      DCHECK_EQ(value->GetType(), Value::TYPE_LIST);
      const ListValue* vlist = static_cast<const ListValue*>(value);
      UpdateNetworkServiceList(vlist);
      break;
    }
    case PROPERTY_INDEX_SERVICE_WATCH_LIST: {
      DCHECK_EQ(value->GetType(), Value::TYPE_LIST);
      const ListValue* vlist = static_cast<const ListValue*>(value);
      UpdateWatchedNetworkServiceList(vlist);
      break;
    }
    case PROPERTY_INDEX_DEVICE:
    case PROPERTY_INDEX_DEVICES: {
      DCHECK_EQ(value->GetType(), Value::TYPE_LIST);
      const ListValue* vlist = static_cast<const ListValue*>(value);
      UpdateNetworkDeviceList(vlist);
      break;
    }
    case PROPERTY_INDEX_PORTAL_URL:
    case PROPERTY_INDEX_CHECK_PORTAL_LIST:
      // Currently we ignore PortalURL and CheckPortalList.
      break;
    default:
      LOG(WARNING) << "Manager: Unhandled key: " << key;
      break;
  }
  base::TimeDelta delta = base::TimeTicks::Now() - start;
  VLOG(2) << "NetworkManagerStatusChanged: time: "
          << delta.InMilliseconds() << " ms.";
  HISTOGRAM_TIMES("CROS_NETWORK_UPDATE", delta);
}

// static
void NetworkLibraryImplCros::NetworkManagerUpdate(
    void* object, const char* manager_path, const Value* info) {
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  if (!info) {
    LOG(ERROR) << "Error retrieving manager properties: " << manager_path;
    return;
  }
  VLOG(1) << "Received NetworkManagerUpdate.";
  DCHECK_EQ(info->GetType(), Value::TYPE_DICTIONARY);
  const DictionaryValue* dict = static_cast<const DictionaryValue*>(info);
  networklib->ParseNetworkManager(dict);
}

void NetworkLibraryImplCros::ParseNetworkManager(const DictionaryValue* dict) {
  for (DictionaryValue::key_iterator iter = dict->begin_keys();
       iter != dict->end_keys(); ++iter) {
    const std::string& key = *iter;
    Value* value;
    bool res = dict->GetWithoutPathExpansion(key, &value);
    CHECK(res);
    NetworkManagerStatusChanged(key.c_str(), value);
  }
  // If there is no Profiles entry, request remembered networks here.
  if (!dict->HasKey(kProfilesProperty))
    RequestRememberedNetworksUpdate();
}

// static
void NetworkLibraryImplCros::DataPlanUpdateHandler(
    void* object,
    const char* modem_service_path,
    const CellularDataPlanList* dataplan) {
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  if (modem_service_path && dataplan) {
    networklib->UpdateCellularDataPlan(std::string(modem_service_path),
                                       dataplan);
  }
}

////////////////////////////////////////////////////////////////////////////

void NetworkLibraryImplCros::UpdateTechnologies(
    const ListValue* technologies, int* bitfieldp) {
  DCHECK(bitfieldp);
  if (!technologies)
    return;
  int bitfield = 0;
  for (ListValue::const_iterator iter = technologies->begin();
       iter != technologies->end(); ++iter) {
    std::string technology;
    (*iter)->GetAsString(&technology);
    if (!technology.empty()) {
      ConnectionType type = ParseType(technology);
      bitfield |= 1 << type;
    }
  }
  *bitfieldp = bitfield;
  NotifyNetworkManagerChanged(false);  // Not forced.
}

void NetworkLibraryImplCros::UpdateAvailableTechnologies(
    const ListValue* technologies) {
  UpdateTechnologies(technologies, &available_devices_);
}

void NetworkLibraryImplCros::UpdateEnabledTechnologies(
    const ListValue* technologies) {
  UpdateTechnologies(technologies, &enabled_devices_);
  if (!ethernet_enabled())
    ethernet_ = NULL;
  if (!wifi_enabled()) {
    active_wifi_ = NULL;
    wifi_networks_.clear();
  }
  if (!cellular_enabled()) {
    active_cellular_ = NULL;
    cellular_networks_.clear();
  }
}

void NetworkLibraryImplCros::UpdateConnectedTechnologies(
    const ListValue* technologies) {
  UpdateTechnologies(technologies, &connected_devices_);
}

////////////////////////////////////////////////////////////////////////////

// Update all network lists, and request associated service updates.
void NetworkLibraryImplCros::UpdateNetworkServiceList(
    const ListValue* services) {
  // TODO(stevenjb): Wait for wifi_scanning_ to be false.
  // Copy the list of existing networks to "old" and clear the map and lists.
  NetworkMap old_network_map = network_map_;
  ClearNetworks();
  // Clear the list of update requests.
  int network_priority_order = 0;
  network_update_requests_.clear();
  // wifi_scanning_ will remain false unless we request a network update.
  wifi_scanning_ = false;
  // |services| represents a complete list of visible networks.
  for (ListValue::const_iterator iter = services->begin();
       iter != services->end(); ++iter) {
    std::string service_path;
    (*iter)->GetAsString(&service_path);
    if (!service_path.empty()) {
      // If we find the network in "old", add it immediately to the map
      // and lists. Otherwise it will get added when NetworkServiceUpdate
      // calls ParseNetwork.
      NetworkMap::iterator found = old_network_map.find(service_path);
      if (found != old_network_map.end()) {
        AddNetwork(found->second);
        old_network_map.erase(found);
      }
      // Always request network updates.
      // TODO(stevenjb): Investigate why we are missing updates then
      // rely on watched network updates and only request updates here for
      // new networks.
      // Use update_request map to store network priority.
      network_update_requests_[service_path] = network_priority_order++;
      wifi_scanning_ = true;
      chromeos::RequestNetworkServiceInfo(service_path.c_str(),
                                          &NetworkServiceUpdate,
                                          this);
    }
  }
  // Iterate through list of remaining networks that are no longer in the
  // list and delete them or update their status and re-add them to the list.
  for (NetworkMap::iterator iter = old_network_map.begin();
       iter != old_network_map.end(); ++iter) {
    Network* network = iter->second;
    if (network->failed() && network->notify_failure()) {
      // We have not notified observers of a connection failure yet.
      AddNetwork(network);
    } else if (network->connecting()) {
      // Network was in connecting state; set state to failed.
      network->SetState(STATE_FAILURE);
      AddNetwork(network);
    } else {
      VLOG(2) << "Deleting non-existant Network: " << network->name()
              << " State = " << network->GetStateString();
      DeleteNetwork(network);
    }
  }
  // If the last network has disappeared, nothing else will
  // have notified observers, so do it now.
  if (services->empty())
    NotifyNetworkManagerChanged(true);  // Forced update
}

// Request updates for watched networks. Does not affect network lists.
// Existing networks will be updated. There should not be any new networks
// in this list, but if there are they will be added appropriately.
void NetworkLibraryImplCros::UpdateWatchedNetworkServiceList(
    const ListValue* services) {
  for (ListValue::const_iterator iter = services->begin();
       iter != services->end(); ++iter) {
    std::string service_path;
    (*iter)->GetAsString(&service_path);
    if (!service_path.empty()) {
      VLOG(1) << "Watched Service: " << service_path;
      chromeos::RequestNetworkServiceInfo(service_path.c_str(),
                                          &NetworkServiceUpdate,
                                          this);
    }
  }
}

// static
void NetworkLibraryImplCros::NetworkServiceUpdate(
    void* object, const char* service_path, const Value* info) {
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  if (service_path) {
    if (!info)
      return;  // Network no longer in visible list, ignore.
    DCHECK_EQ(info->GetType(), Value::TYPE_DICTIONARY);
    const DictionaryValue* dict = static_cast<const DictionaryValue*>(info);
    networklib->ParseNetwork(std::string(service_path), dict);
  }
}

// Called from NetworkServiceUpdate and WifiServiceUpdateAndConnect.
Network* NetworkLibraryImplCros::ParseNetwork(
    const std::string& service_path, const DictionaryValue* info) {
  Network* network = FindNetworkByPath(service_path);
  if (!network) {
    ConnectionType type = ParseTypeFromDictionary(info);
    network = CreateNewNetwork(type, service_path);
    AddNetwork(network);
  }

  // Erase entry from network_unique_id_map_ in case unique id changes.
  if (!network->unique_id().empty())
    network_unique_id_map_.erase(network->unique_id());

  network->ParseInfo(info);  // virtual.

  if (!network->unique_id().empty())
    network_unique_id_map_[network->unique_id()] = network;

  SetProfileTypeFromPath(network);

  UpdateActiveNetwork(network);

  // Copy remembered credentials if required.
  Network* remembered = FindRememberedFromNetwork(network);
  if (remembered)
    network->CopyCredentialsFromRemembered(remembered);

  // Find and erase entry in update_requests, and set network priority.
  PriorityMap::iterator found2 = network_update_requests_.find(service_path);
  if (found2 != network_update_requests_.end()) {
    network->priority_order_ = found2->second;
    network_update_requests_.erase(found2);
    if (network_update_requests_.empty()) {
      // Clear wifi_scanning_ when we have no pending requests.
      wifi_scanning_ = false;
    }
  } else {
    // TODO(stevenjb): Enable warning once UpdateNetworkServiceList is fixed.
    // LOG(WARNING) << "ParseNetwork called with no update request entry: "
    //              << service_path;
  }

  VLOG(1) << "ParseNetwork: " << network->name()
          << " path: " << network->service_path()
          << " profile: " << network->profile_path_;
  NotifyNetworkManagerChanged(false);  // Not forced.
  return network;
}

////////////////////////////////////////////////////////////////////////////

void NetworkLibraryImplCros::UpdateRememberedNetworks(
    const ListValue* profiles) {
  VLOG(1) << "UpdateRememberedNetworks";
  // Update the list of profiles.
  profile_list_.clear();
  for (ListValue::const_iterator iter = profiles->begin();
       iter != profiles->end(); ++iter) {
    std::string profile_path;
    (*iter)->GetAsString(&profile_path);
    if (profile_path.empty()) {
      LOG(WARNING) << "Bad or empty profile path.";
      continue;
    }
    NetworkProfileType profile_type;
    if (profile_path == kSharedProfilePath)
      profile_type = PROFILE_SHARED;
    else
      profile_type = PROFILE_USER;
    profile_list_.push_back(NetworkProfile(profile_path, profile_type));
    // Check to see if we connected to any networks before a user profile was
    // available (i.e. before login), but unchecked the "Share" option (i.e.
    // the desired pofile is the user profile). Move these networks to the
    // user profile when it becomes available.
    if (profile_type == PROFILE_USER && !user_networks_.empty()) {
      for (std::list<std::string>::iterator iter2 = user_networks_.begin();
           iter2 != user_networks_.end(); ++iter2) {
        Network* network = FindNetworkByPath(*iter2);
        if (network && network->profile_path() != profile_path)
          network->SetProfilePath(profile_path);
      }
      user_networks_.clear();
    }
  }
}

void NetworkLibraryImplCros::RequestRememberedNetworksUpdate() {
  VLOG(1) << "RequestRememberedNetworksUpdate";
  // Delete all remembered networks. We delete them because
  // RequestNetworkProfile is asynchronous and may invoke
  // UpdateRememberedServiceList multiple times (once per profile).
  // We can do this safely because we do not save any local state for
  // remembered networks. This list updates infrequently.
  DeleteRememberedNetworks();
  // Request remembered networks from each profile. Later entries will
  // override earlier entries, so default/local entries will override
  // user entries (as desired).
  for (NetworkProfileList::iterator iter = profile_list_.begin();
       iter != profile_list_.end(); ++iter) {
    NetworkProfile& profile = *iter;
    VLOG(1) << " Requesting Profile: " << profile.path;
    chromeos::RequestNetworkProfile(
        profile.path.c_str(), &ProfileUpdate, this);
  }
}

// static
void NetworkLibraryImplCros::ProfileUpdate(
    void* object, const char* profile_path, const Value* info) {
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  if (!info) {
    LOG(ERROR) << "Error retrieving profile: " << profile_path;
    return;
  }
  VLOG(1) << "Received ProfileUpdate for: " << profile_path;
  DCHECK_EQ(info->GetType(), Value::TYPE_DICTIONARY);
  const DictionaryValue* dict = static_cast<const DictionaryValue*>(info);
  ListValue* entries(NULL);
  dict->GetList(kEntriesProperty, &entries);
  DCHECK(entries);
  networklib->UpdateRememberedServiceList(profile_path, entries);
}

void NetworkLibraryImplCros::UpdateRememberedServiceList(
    const char* profile_path, const ListValue* profile_entries) {
  DCHECK(profile_path);
  VLOG(1) << "UpdateRememberedServiceList for path: " << profile_path;
  NetworkProfileList::iterator iter1;
  for (iter1 = profile_list_.begin(); iter1 != profile_list_.end(); ++iter1) {
    if (iter1->path == profile_path)
      break;
  }
  if (iter1 == profile_list_.end()) {
    NOTREACHED() << "Profile not in list: " << profile_path;
    return;
  }
  NetworkProfile& profile = *iter1;
  // |profile_entries| is a list of remembered networks from |profile_path|.
  profile.services.clear();
  for (ListValue::const_iterator iter2 = profile_entries->begin();
       iter2 != profile_entries->end(); ++iter2) {
    std::string service_path;
    (*iter2)->GetAsString(&service_path);
    if (service_path.empty()) {
      LOG(WARNING) << "Empty service path in profile.";
      continue;
    }
    VLOG(1) << " Remembered service: " << service_path;
    // Add service to profile list.
    profile.services.insert(service_path);
    // Request update for remembered network.
    chromeos::RequestNetworkProfileEntry(profile_path,
                                         service_path.c_str(),
                                         &RememberedNetworkServiceUpdate,
                                         this);
  }
}

// static
void NetworkLibraryImplCros::RememberedNetworkServiceUpdate(
    void* object, const char* service_path, const Value* info) {
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  if (service_path) {
    if (!info) {
      // Remembered network no longer exists.
      networklib->DeleteRememberedNetwork(std::string(service_path));
    } else {
      DCHECK_EQ(info->GetType(), Value::TYPE_DICTIONARY);
      const DictionaryValue* dict = static_cast<const DictionaryValue*>(info);
      networklib->ParseRememberedNetwork(std::string(service_path), dict);
    }
  }
}

// Returns NULL if |service_path| refers to a network that is not a
// remembered type. Called from RememberedNetworkServiceUpdate.
Network* NetworkLibraryImplCros::ParseRememberedNetwork(
    const std::string& service_path, const DictionaryValue* info) {
  Network* remembered;
  NetworkMap::iterator found = remembered_network_map_.find(service_path);
  if (found != remembered_network_map_.end()) {
    remembered = found->second;
  } else {
    ConnectionType type = ParseTypeFromDictionary(info);
    if (type == TYPE_WIFI || type == TYPE_VPN) {
      remembered = CreateNewNetwork(type, service_path);
      AddRememberedNetwork(remembered);
    } else {
      VLOG(1) << "Ignoring remembered network: " << service_path
              << " Type: " << ConnectionTypeToString(type);
      return NULL;
    }
  }
  remembered->ParseInfo(info);  // virtual.

  SetProfileTypeFromPath(remembered);

  VLOG(1) << "ParseRememberedNetwork: " << remembered->name()
          << " path: " << remembered->service_path()
          << " profile: " << remembered->profile_path_;
  NotifyNetworkManagerChanged(false);  // Not forced.

  if (remembered->type() == TYPE_VPN) {
    // VPNs are only stored in profiles. If we don't have a network for it,
    // request one.
    if (!FindNetworkFromRemembered(remembered)) {
      VirtualNetwork* vpn = static_cast<VirtualNetwork*>(remembered);
      std::string provider_type = ProviderTypeToString(vpn->provider_type());
      VLOG(1) << "Requesting VPN: " << vpn->name()
              << " Server: " << vpn->server_hostname()
              << " Type: " << provider_type;
      chromeos::RequestVirtualNetwork(vpn->name().c_str(),
                                      vpn->server_hostname().c_str(),
                                      provider_type.c_str(),
                                      NetworkServiceUpdate,
                                      this);
    }
  }

  return remembered;
}

////////////////////////////////////////////////////////////////////////////
// NetworkDevice list management functions.

// Update device list, and request associated device updates.
// |devices| represents a complete list of devices.
void NetworkLibraryImplCros::UpdateNetworkDeviceList(const ListValue* devices) {
  NetworkDeviceMap old_device_map = device_map_;
  device_map_.clear();
  VLOG(2) << "Updating Device List.";
  for (ListValue::const_iterator iter = devices->begin();
       iter != devices->end(); ++iter) {
    std::string device_path;
    (*iter)->GetAsString(&device_path);
    if (!device_path.empty()) {
      NetworkDeviceMap::iterator found = old_device_map.find(device_path);
      if (found != old_device_map.end()) {
        VLOG(2) << " Adding existing device: " << device_path;
        device_map_[device_path] = found->second;
        old_device_map.erase(found);
      }
      chromeos::RequestNetworkDeviceInfo(device_path.c_str(),
                                         &NetworkDeviceUpdate,
                                         this);
    }
  }
  // Delete any old devices that no longer exist.
  for (NetworkDeviceMap::iterator iter = old_device_map.begin();
       iter != old_device_map.end(); ++iter) {
    DeleteDeviceFromDeviceObserversMap(iter->first);
    // Delete device.
    delete iter->second;
  }
}

// static
void NetworkLibraryImplCros::NetworkDeviceUpdate(
    void* object, const char* device_path, const Value* info) {
  NetworkLibraryImplCros* networklib =
      static_cast<NetworkLibraryImplCros*>(object);
  DCHECK(networklib);
  if (device_path) {
    if (!info) {
      // device no longer exists.
      networklib->DeleteDevice(std::string(device_path));
    } else {
      DCHECK_EQ(info->GetType(), Value::TYPE_DICTIONARY);
      const DictionaryValue* dict = static_cast<const DictionaryValue*>(info);
      networklib->ParseNetworkDevice(std::string(device_path), dict);
    }
  }
}

void NetworkLibraryImplCros::ParseNetworkDevice(
    const std::string& device_path, const DictionaryValue* info) {
  NetworkDeviceMap::iterator found = device_map_.find(device_path);
  NetworkDevice* device;
  if (found != device_map_.end()) {
    device = found->second;
  } else {
    device = new NetworkDevice(device_path);
    VLOG(2) << " Adding device: " << device_path;
    device_map_[device_path] = device;
  }
  device->ParseInfo(info);
  VLOG(1) << "ParseNetworkDevice:" << device->name();
  NotifyNetworkManagerChanged(false);  // Not forced.
  AddNetworkDeviceObserver(device_path, network_device_observer_.get());
}

////////////////////////////////////////////////////////////////////////////

class NetworkLibraryImplStub : public NetworkLibraryImplBase {
 public:
  NetworkLibraryImplStub();
  virtual ~NetworkLibraryImplStub();

  virtual void Init() OVERRIDE;
  virtual bool IsCros() const OVERRIDE { return false; }

  // NetworkLibraryImplBase implementation.

  virtual void MonitorNetworkStart(const std::string& service_path) OVERRIDE {}
  virtual void MonitorNetworkStop(const std::string& service_path) OVERRIDE {}
  virtual void MonitorNetworkDeviceStart(
      const std::string& device_path) OVERRIDE {}
  virtual void MonitorNetworkDeviceStop(
      const std::string& device_path) OVERRIDE {}

  virtual void CallConnectToNetwork(Network* network) OVERRIDE;
  virtual void CallRequestWifiNetworkAndConnect(
      const std::string& ssid, ConnectionSecurity security) OVERRIDE;
  virtual void CallRequestVirtualNetworkAndConnect(
      const std::string& service_name,
      const std::string& server_hostname,
      VirtualNetwork::ProviderType provider_type) OVERRIDE;

  virtual void CallDeleteRememberedNetwork(
      const std::string& profile_path,
      const std::string& service_path) OVERRIDE {}

  // NetworkLibrary implementation.

  virtual void ChangePin(const std::string& old_pin,
                         const std::string& new_pin) OVERRIDE;
  virtual void ChangeRequirePin(bool require_pin,
                                const std::string& pin) OVERRIDE;
  virtual void EnterPin(const std::string& pin) OVERRIDE;
  virtual void UnblockPin(const std::string& puk,
                          const std::string& new_pin) OVERRIDE;

  virtual void RequestCellularScan() OVERRIDE {}
  virtual void RequestCellularRegister(
      const std::string& network_id) OVERRIDE {}
  virtual void SetCellularDataRoamingAllowed(bool new_value) OVERRIDE {}
  virtual void RequestNetworkScan() OVERRIDE {}

  virtual bool GetWifiAccessPoints(WifiAccessPointVector* result) OVERRIDE;

  virtual void DisconnectFromNetwork(const Network* network) OVERRIDE;

  virtual void CallEnableNetworkDeviceType(
      ConnectionType device, bool enable) OVERRIDE {}
  virtual void EnableOfflineMode(bool enable) OVERRIDE {
    offline_mode_ = enable;
  }

  virtual NetworkIPConfigVector GetIPConfigs(
      const std::string& device_path,
      std::string* hardware_address,
      HardwareAddressFormat format) OVERRIDE;
  virtual void SetIPConfig(const NetworkIPConfig& ipconfig) OVERRIDE;

 private:
  std::string ip_address_;
  std::string hardware_address_;
  NetworkIPConfigVector ip_configs_;
  std::string pin_;
  bool pin_required_;
  bool pin_entered_;

  DISALLOW_COPY_AND_ASSIGN(NetworkLibraryImplStub);
};

////////////////////////////////////////////////////////////////////////////

NetworkLibraryImplStub::NetworkLibraryImplStub()
    : ip_address_("1.1.1.1"),
      hardware_address_("01:23:45:67:89:ab"),
      pin_(""),
      pin_required_(false),
      pin_entered_(false) {
}

NetworkLibraryImplStub::~NetworkLibraryImplStub() {
}

void NetworkLibraryImplStub::Init() {
  is_locked_ = false;

  // Devices
  int devices =
      (1 << TYPE_ETHERNET) | (1 << TYPE_WIFI) | (1 << TYPE_CELLULAR);
  available_devices_ = devices;
  enabled_devices_ = devices;
  connected_devices_ = devices;

  NetworkDevice* cellular = new NetworkDevice("cellular");
  scoped_ptr<Value> cellular_type(Value::CreateStringValue(kTypeCellular));
  cellular->ParseValue(PROPERTY_INDEX_TYPE, cellular_type.get());
  cellular->IMSI_ = "123456789012345";
  device_map_["cellular"] = cellular;

  // Networks
  DeleteNetworks();

  ethernet_ = new EthernetNetwork("eth1");
  ethernet_->set_connected(true);
  AddNetwork(ethernet_);

  WifiNetwork* wifi1 = new WifiNetwork("fw1");
  wifi1->set_name("Fake WiFi Connected");
  wifi1->set_strength(90);
  wifi1->set_connected(false);
  wifi1->set_connecting(true);
  wifi1->set_active(true);
  wifi1->set_encryption(SECURITY_NONE);
  wifi1->set_profile_type(PROFILE_SHARED);
  AddNetwork(wifi1);

  WifiNetwork* wifi2 = new WifiNetwork("fw2");
  wifi2->set_name("Fake WiFi");
  wifi2->set_strength(70);
  wifi2->set_connected(false);
  wifi2->set_encryption(SECURITY_NONE);
  wifi2->set_profile_type(PROFILE_SHARED);
  AddNetwork(wifi2);

  WifiNetwork* wifi3 = new WifiNetwork("fw3");
  wifi3->set_name("Fake WiFi Encrypted");
  wifi3->set_strength(60);
  wifi3->set_connected(false);
  wifi3->set_encryption(SECURITY_WEP);
  wifi3->set_passphrase_required(true);
  wifi3->set_profile_type(PROFILE_USER);
  AddNetwork(wifi3);

  WifiNetwork* wifi4 = new WifiNetwork("fw4");
  wifi4->set_name("Fake WiFi 802.1x");
  wifi4->set_strength(50);
  wifi4->set_connected(false);
  wifi4->set_connectable(false);
  wifi4->set_encryption(SECURITY_8021X);
  wifi4->SetEAPMethod(EAP_METHOD_PEAP);
  wifi4->SetEAPIdentity("nobody@google.com");
  wifi4->SetEAPPassphrase("password");
  AddNetwork(wifi4);

  WifiNetwork* wifi5 = new WifiNetwork("fw5");
  wifi5->set_name("Fake WiFi UTF-8 SSID ");
  wifi5->SetSsid("Fake WiFi UTF-8 SSID \u3042\u3044\u3046");
  wifi5->set_strength(25);
  wifi5->set_connected(false);
  AddNetwork(wifi5);

  WifiNetwork* wifi6 = new WifiNetwork("fw6");
  wifi6->set_name("Fake WiFi latin-1 SSID ");
  wifi6->SetSsid("Fake WiFi latin-1 SSID \xc0\xcb\xcc\xd6\xfb");
  wifi6->set_strength(20);
  wifi6->set_connected(false);
  AddNetwork(wifi6);

  active_wifi_ = wifi1;

  CellularNetwork* cellular1 = new CellularNetwork("fc1");
  cellular1->set_name("Fake Cellular");
  cellular1->set_strength(70);
  cellular1->set_connected(false);
  cellular1->set_connecting(true);
  cellular1->set_active(true);
  cellular1->set_activation_state(ACTIVATION_STATE_ACTIVATED);
  cellular1->set_payment_url(std::string("http://www.google.com"));
  cellular1->set_usage_url(std::string("http://www.google.com"));
  cellular1->set_network_technology(NETWORK_TECHNOLOGY_EVDO);
  cellular1->set_roaming_state(ROAMING_STATE_ROAMING);

  CellularDataPlan* base_plan = new CellularDataPlan();
  base_plan->plan_name = "Base plan";
  base_plan->plan_type = CELLULAR_DATA_PLAN_METERED_BASE;
  base_plan->plan_data_bytes = 100ll * 1024 * 1024;
  base_plan->data_bytes_used = 75ll * 1024 * 1024;
  CellularDataPlanVector* data_plans = new CellularDataPlanVector();
  data_plan_map_[cellular1->service_path()] = data_plans;
  data_plans->push_back(base_plan);

  CellularDataPlan* paid_plan = new CellularDataPlan();
  paid_plan->plan_name = "Paid plan";
  paid_plan->plan_type = CELLULAR_DATA_PLAN_METERED_PAID;
  paid_plan->plan_data_bytes = 5ll * 1024 * 1024 * 1024;
  paid_plan->data_bytes_used = 3ll * 1024 * 1024 * 1024;
  data_plans->push_back(paid_plan);

  AddNetwork(cellular1);
  active_cellular_ = cellular1;

  CellularNetwork* cellular2 = new CellularNetwork("fc2");
  cellular2->set_name("Fake Cellular 2");
  cellular2->set_strength(70);
  cellular2->set_connected(true);
  cellular2->set_activation_state(ACTIVATION_STATE_ACTIVATED);
  cellular2->set_network_technology(NETWORK_TECHNOLOGY_UMTS);
  AddNetwork(cellular2);

  // VPNs
  VirtualNetwork* vpn1 = new VirtualNetwork("fv1");
  vpn1->set_name("Fake VPN Provider 1");
  vpn1->set_server_hostname("vpn1server.fake.com");
  vpn1->set_provider_type(VirtualNetwork::PROVIDER_TYPE_L2TP_IPSEC_PSK);
  vpn1->set_username("VPN User 1");
  vpn1->set_connected(false);
  AddNetwork(vpn1);

  VirtualNetwork* vpn2 = new VirtualNetwork("fv2");
  vpn2->set_name("Fake VPN Provider 2");
  vpn2->set_server_hostname("vpn2server.fake.com");
  vpn2->set_provider_type(VirtualNetwork::PROVIDER_TYPE_L2TP_IPSEC_USER_CERT);
  vpn2->set_username("VPN User 2");
  vpn2->set_connected(true);
  AddNetwork(vpn2);

  VirtualNetwork* vpn3 = new VirtualNetwork("fv3");
  vpn3->set_name("Fake VPN Provider 3");
  vpn3->set_server_hostname("vpn3server.fake.com");
  vpn3->set_provider_type(VirtualNetwork::PROVIDER_TYPE_OPEN_VPN);
  vpn3->set_connected(false);
  AddNetwork(vpn3);

  active_virtual_ = vpn2;

  // Remembered Networks
  DeleteRememberedNetworks();
  NetworkProfile profile("default", PROFILE_SHARED);
  profile.services.insert("fw2");
  profile.services.insert("fv2");
  profile_list_.push_back(profile);
  WifiNetwork* remembered_wifi2 = new WifiNetwork("fw2");
  remembered_wifi2->set_name("Fake WiFi 2");
  remembered_wifi2->set_encryption(SECURITY_WEP);
  AddRememberedNetwork(remembered_wifi2);
  VirtualNetwork* remembered_vpn2 = new VirtualNetwork("fv2");
  remembered_vpn2->set_name("Fake VPN Provider 2");
  remembered_vpn2->set_server_hostname("vpn2server.fake.com");
  remembered_vpn2->set_provider_type(
      VirtualNetwork::PROVIDER_TYPE_L2TP_IPSEC_USER_CERT);
  remembered_vpn2->set_connected(true);
  AddRememberedNetwork(remembered_vpn2);

  wifi_scanning_ = false;
  offline_mode_ = false;
}

//////////////////////////////////////////////////////////////////////////////
// NetworkLibraryImplBase implementation.

void NetworkLibraryImplStub::CallConnectToNetwork(Network* network) {
  NetworkConnectCompleted(network, CONNECT_SUCCESS);
}

void NetworkLibraryImplStub::CallRequestWifiNetworkAndConnect(
    const std::string& ssid, ConnectionSecurity security) {
  WifiNetwork* wifi = new WifiNetwork(ssid);
  wifi->set_encryption(security);
  AddNetwork(wifi);
  ConnectToWifiNetworkUsingConnectData(wifi);
}

void NetworkLibraryImplStub::CallRequestVirtualNetworkAndConnect(
    const std::string& service_name,
    const std::string& server_hostname,
    VirtualNetwork::ProviderType provider_type) {
  VirtualNetwork* vpn = new VirtualNetwork(service_name);
  vpn->set_server_hostname(server_hostname);
  vpn->set_provider_type(provider_type);
  AddNetwork(vpn);
  ConnectToVirtualNetworkUsingConnectData(vpn);
}

/////////////////////////////////////////////////////////////////////////////
// NetworkLibrary implementation.

void NetworkLibraryImplStub::ChangePin(const std::string& old_pin,
                                       const std::string& new_pin) {
  sim_operation_ = SIM_OPERATION_CHANGE_PIN;
  if (!pin_required_ || old_pin == pin_) {
    pin_ = new_pin;
    NotifyPinOperationCompleted(PIN_ERROR_NONE);
  } else {
    NotifyPinOperationCompleted(PIN_ERROR_INCORRECT_CODE);
  }
}

void NetworkLibraryImplStub::ChangeRequirePin(bool require_pin,
                                              const std::string& pin) {
  sim_operation_ = SIM_OPERATION_CHANGE_REQUIRE_PIN;
  if (!pin_required_ || pin == pin_) {
    pin_required_ = require_pin;
    FlipSimPinRequiredStateIfNeeded();
    NotifyPinOperationCompleted(PIN_ERROR_NONE);
  } else {
    NotifyPinOperationCompleted(PIN_ERROR_INCORRECT_CODE);
  }
}

void NetworkLibraryImplStub::EnterPin(const std::string& pin) {
  sim_operation_ = SIM_OPERATION_ENTER_PIN;
  if (!pin_required_ || pin == pin_) {
    pin_entered_ = true;
    NotifyPinOperationCompleted(PIN_ERROR_NONE);
  } else {
    NotifyPinOperationCompleted(PIN_ERROR_INCORRECT_CODE);
  }
}

void NetworkLibraryImplStub::UnblockPin(const std::string& puk,
                                        const std::string& new_pin) {
  sim_operation_ = SIM_OPERATION_UNBLOCK_PIN;
  // TODO(stevenjb): something?
  NotifyPinOperationCompleted(PIN_ERROR_NONE);
}

bool NetworkLibraryImplStub::GetWifiAccessPoints(
    WifiAccessPointVector* result) {
  *result = WifiAccessPointVector();
  return true;
}

void NetworkLibraryImplStub::DisconnectFromNetwork(const Network* network) {
  NetworkDisconnectCompleted(network->service_path());
}

NetworkIPConfigVector NetworkLibraryImplStub::GetIPConfigs(
    const std::string& device_path,
    std::string* hardware_address,
    HardwareAddressFormat format) {
  *hardware_address = hardware_address_;
  return ip_configs_;
}

void NetworkLibraryImplStub::SetIPConfig(const NetworkIPConfig& ipconfig) {
    ip_configs_.push_back(ipconfig);
}

/////////////////////////////////////////////////////////////////////////////

// static
NetworkLibrary* NetworkLibrary::GetImpl(bool stub) {
  // Use static global to avoid recursive GetImpl() call from EnsureCrosLoaded.
  static NetworkLibrary* network_library = NULL;
  if (network_library == NULL) {
    if (stub || !CrosLibrary::Get()->EnsureLoaded())
      network_library = new NetworkLibraryImplStub();
    else
      network_library = new NetworkLibraryImplCros();
    network_library->Init();
  }
  return network_library;
}

/////////////////////////////////////////////////////////////////////////////

}  // namespace chromeos

// Allows InvokeLater without adding refcounting. This class is a Singleton and
// won't be deleted until it's last InvokeLater is run.
DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::NetworkLibraryImplBase);
