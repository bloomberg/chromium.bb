// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/network_library.h"

#include <algorithm>
#include <map>

#include "base/i18n/icu_encoding_detection.h"
#include "base/i18n/icu_string_conversions.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram.h"
#include "base/stl_util-inl.h"
#include "base/string_number_conversions.h"
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
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

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
//  WirelessNetwork: a Wifi or Cellular Network.
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
// NetworkObserverList: A monitor and list of observers of a network.
// network_monitor_: a handle to the libcros network Service handler.
// UpdateNetworkStatus: This handles changes to a monitored service, typically
//     changes to transient states like Strength. (Note: also updates State).
//
// AddNetworkDeviceObserver: Adds an observer for a specific device.
//                           Will be called on any device property change.
// NetworkDeviceObserverList: A monitor and list of observers of a device.
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
const char* kCarrierIdFormat = "%s (%s)";

// Type of a pending SIM operation.
enum SimOperationType {
  SIM_OPERATION_NONE               = 0,
  SIM_OPERATION_CHANGE_PIN         = 1,
  SIM_OPERATION_CHANGE_REQUIRE_PIN = 2,
  SIM_OPERATION_ENTER_PIN          = 3,
  SIM_OPERATION_UNBLOCK_PIN        = 4,
};

// D-Bus interface string constants.

// Flimflam property names.
const char* kSecurityProperty = "Security";
const char* kPassphraseProperty = "Passphrase";
const char* kIdentityProperty = "Identity";
const char* kCertPathProperty = "CertPath";
const char* kPassphraseRequiredProperty = "PassphraseRequired";
const char* kSaveCredentialsProperty = "SaveCredentials";
const char* kProfilesProperty = "Profiles";
const char* kServicesProperty = "Services";
const char* kServiceWatchListProperty = "ServiceWatchList";
const char* kAvailableTechnologiesProperty = "AvailableTechnologies";
const char* kEnabledTechnologiesProperty = "EnabledTechnologies";
const char* kConnectedTechnologiesProperty = "ConnectedTechnologies";
const char* kDefaultTechnologyProperty = "DefaultTechnology";
const char* kOfflineModeProperty = "OfflineMode";
const char* kSignalStrengthProperty = "Strength";
const char* kNameProperty = "Name";
const char* kStateProperty = "State";
const char* kConnectivityStateProperty = "ConnectivityState";
const char* kTypeProperty = "Type";
const char* kDeviceProperty = "Device";
const char* kActivationStateProperty = "Cellular.ActivationState";
const char* kNetworkTechnologyProperty = "Cellular.NetworkTechnology";
const char* kRoamingStateProperty = "Cellular.RoamingState";
const char* kOperatorNameProperty = "Cellular.OperatorName";
const char* kOperatorCodeProperty = "Cellular.OperatorCode";
const char* kServingOperatorProperty = "Cellular.ServingOperator";
const char* kPaymentURLProperty = "Cellular.OlpUrl";
const char* kUsageURLProperty = "Cellular.UsageUrl";
const char* kCellularApnProperty = "Cellular.APN";
const char* kCellularLastGoodApnProperty = "Cellular.LastGoodAPN";
const char* kWifiHexSsid = "WiFi.HexSSID";
const char* kWifiFrequency = "WiFi.Frequency";
const char* kWifiHiddenSsid = "WiFi.HiddenSSID";
const char* kWifiPhyMode = "WiFi.PhyMode";
const char* kFavoriteProperty = "Favorite";
const char* kConnectableProperty = "Connectable";
const char* kAutoConnectProperty = "AutoConnect";
const char* kIsActiveProperty = "IsActive";
const char* kModeProperty = "Mode";
const char* kErrorProperty = "Error";
const char* kActiveProfileProperty = "ActiveProfile";
const char* kEntriesProperty = "Entries";
const char* kDevicesProperty = "Devices";
const char* kProviderProperty = "Provider";
const char* kHostProperty = "Host";

// Flimflam property names for SIMLock status.
const char* kSIMLockStatusProperty = "Cellular.SIMLockStatus";
const char* kSIMLockTypeProperty = "LockType";
const char* kSIMLockRetriesLeftProperty = "RetriesLeft";

// Flimflam property names for Cellular.FoundNetworks.
const char* kLongNameProperty = "long_name";
const char* kStatusProperty = "status";
const char* kShortNameProperty = "short_name";
const char* kTechnologyProperty = "technology";

// Flimflam SIMLock status types.
const char* kSIMLockPin = "sim-pin";
const char* kSIMLockPuk = "sim-puk";

// APN info property names.
const char* kApnProperty = "apn";
const char* kNetworkIdProperty = "network_id";
const char* kUsernameProperty = "username";
const char* kPasswordProperty = "password";

// Operator info property names.
const char* kOperatorNameKey = "name";
const char* kOperatorCodeKey = "code";
const char* kOperatorCountryKey = "country";

// Flimflam device info property names.
const char* kScanningProperty = "Scanning";
const char* kCarrierProperty = "Cellular.Carrier";
const char* kCellularAllowRoamingProperty = "Cellular.AllowRoaming";
const char* kHomeProviderProperty = "Cellular.HomeProvider";
const char* kMeidProperty = "Cellular.MEID";
const char* kImeiProperty = "Cellular.IMEI";
const char* kImsiProperty = "Cellular.IMSI";
const char* kEsnProperty = "Cellular.ESN";
const char* kMdnProperty = "Cellular.MDN";
const char* kMinProperty = "Cellular.MIN";
const char* kModelIDProperty = "Cellular.ModelID";
const char* kManufacturerProperty = "Cellular.Manufacturer";
const char* kFirmwareRevisionProperty = "Cellular.FirmwareRevision";
const char* kHardwareRevisionProperty = "Cellular.HardwareRevision";
const char* kPoweredProperty = "Powered";
const char* kPRLVersionProperty = "Cellular.PRLVersion"; // (INT16)
const char* kSelectedNetworkProperty = "Cellular.SelectedNetwork";
const char* kSupportNetworkScanProperty = "Cellular.SupportNetworkScan";
const char* kFoundNetworksProperty = "Cellular.FoundNetworks";

// Flimflam type options.
const char* kTypeEthernet = "ethernet";
const char* kTypeWifi = "wifi";
const char* kTypeWimax = "wimax";
const char* kTypeBluetooth = "bluetooth";
const char* kTypeCellular = "cellular";
const char* kTypeVPN = "vpn";

// Flimflam mode options.
const char* kModeManaged = "managed";
const char* kModeAdhoc = "adhoc";

// Flimflam security options.
const char* kSecurityWpa = "wpa";
const char* kSecurityWep = "wep";
const char* kSecurityRsn = "rsn";
const char* kSecurity8021x = "802_1x";
const char* kSecurityPsk = "psk";
const char* kSecurityNone = "none";

// Flimflam L2TPIPsec property names.
const char* kL2TPIPSecCACertProperty = "L2TPIPsec.CACert";
const char* kL2TPIPSecCertProperty = "L2TPIPsec.Cert";
const char* kL2TPIPSecKeyProperty = "L2TPIPsec.Key";
const char* kL2TPIPSecPSKProperty = "L2TPIPsec.PSK";
const char* kL2TPIPSecUserProperty = "L2TPIPsec.User";
const char* kL2TPIPSecPasswordProperty = "L2TPIPsec.Password";

// Flimflam EAP property names.
// See src/third_party/flimflam/doc/service-api.txt.
const char* kEapIdentityProperty = "EAP.Identity";
const char* kEapMethodProperty = "EAP.EAP";
const char* kEapPhase2AuthProperty = "EAP.InnerEAP";
const char* kEapAnonymousIdentityProperty = "EAP.AnonymousIdentity";
const char* kEapClientCertProperty = "EAP.ClientCert";  // path
const char* kEapCertIDProperty = "EAP.CertID";  // PKCS#11 ID
const char* kEapClientCertNssProperty = "EAP.ClientCertNSS";  // NSS nickname
const char* kEapPrivateKeyProperty = "EAP.PrivateKey";
const char* kEapPrivateKeyPasswordProperty = "EAP.PrivateKeyPassword";
const char* kEapKeyIDProperty = "EAP.KeyID";
const char* kEapCaCertProperty = "EAP.CACert";  // server CA cert path
const char* kEapCaCertIDProperty = "EAP.CACertID";  // server CA PKCS#11 ID
const char* kEapCaCertNssProperty = "EAP.CACertNSS";  // server CA NSS nickname
const char* kEapUseSystemCAsProperty = "EAP.UseSystemCAs";
const char* kEapPinProperty = "EAP.PIN";
const char* kEapPasswordProperty = "EAP.Password";
const char* kEapKeyMgmtProperty = "EAP.KeyMgmt";

// Flimflam EAP method options.
const std::string& kEapMethodPEAP = "PEAP";
const std::string& kEapMethodTLS = "TLS";
const std::string& kEapMethodTTLS = "TTLS";
const std::string& kEapMethodLEAP = "LEAP";

// Flimflam EAP phase 2 auth options.
const std::string& kEapPhase2AuthPEAPMD5 = "auth=MD5";
const std::string& kEapPhase2AuthPEAPMSCHAPV2 = "auth=MSCHAPV2";
const std::string& kEapPhase2AuthTTLSMD5 = "autheap=MD5";
const std::string& kEapPhase2AuthTTLSMSCHAPV2 = "autheap=MSCHAPV2";
const std::string& kEapPhase2AuthTTLSMSCHAP = "autheap=MSCHAP";
const std::string& kEapPhase2AuthTTLSPAP = "autheap=PAP";
const std::string& kEapPhase2AuthTTLSCHAP = "autheap=CHAP";

// Flimflam VPN provider types.
const char* kProviderL2tpIpsec = "l2tpipsec";
const char* kProviderOpenVpn = "openvpn";


// Flimflam state options.
const char* kStateIdle = "idle";
const char* kStateCarrier = "carrier";
const char* kStateAssociation = "association";
const char* kStateConfiguration = "configuration";
const char* kStateReady = "ready";
const char* kStateDisconnect = "disconnect";
const char* kStateFailure = "failure";
const char* kStateActivationFailure = "activation-failure";

// Flimflam connectivity state options.
const char* kConnStateUnrestricted = "unrestricted";
const char* kConnStateRestricted = "restricted";
const char* kConnStateNone = "none";

// Flimflam network technology options.
const char* kNetworkTechnology1Xrtt = "1xRTT";
const char* kNetworkTechnologyEvdo = "EVDO";
const char* kNetworkTechnologyGprs = "GPRS";
const char* kNetworkTechnologyEdge = "EDGE";
const char* kNetworkTechnologyUmts = "UMTS";
const char* kNetworkTechnologyHspa = "HSPA";
const char* kNetworkTechnologyHspaPlus = "HSPA+";
const char* kNetworkTechnologyLte = "LTE";
const char* kNetworkTechnologyLteAdvanced = "LTE Advanced";

// Flimflam roaming state options
const char* kRoamingStateHome = "home";
const char* kRoamingStateRoaming = "roaming";
const char* kRoamingStateUnknown = "unknown";

// Flimflam activation state options
const char* kActivationStateActivated = "activated";
const char* kActivationStateActivating = "activating";
const char* kActivationStateNotActivated = "not-activated";
const char* kActivationStatePartiallyActivated = "partially-activated";
const char* kActivationStateUnknown = "unknown";

// Flimflam error options.
const char* kErrorOutOfRange = "out-of-range";
const char* kErrorPinMissing = "pin-missing";
const char* kErrorDhcpFailed = "dhcp-failed";
const char* kErrorConnectFailed = "connect-failed";
const char* kErrorBadPassphrase = "bad-passphrase";
const char* kErrorBadWEPKey = "bad-wepkey";
const char* kErrorActivationFailed = "activation-failed";
const char* kErrorNeedEvdo = "need-evdo";
const char* kErrorNeedHomeNetwork = "need-home-network";
const char* kErrorOtaspFailed = "otasp-failed";
const char* kErrorAaaFailed = "aaa-failed";

// Flimflam error messages.
const char* kErrorPassphraseRequiredMsg = "Passphrase required";
const char* kErrorIncorrectPinMsg = "org.chromium.flimflam.Device.IncorrectPin";
const char* kErrorPinBlockedMsg = "org.chromium.flimflam.Device.PinBlocked";
const char* kErrorPinRequiredMsg = "org.chromium.flimflam.Device.PinRequired";

const char* kUnknownString = "UNKNOWN";

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

// TODO(stevenjb/njw): Deprecate in favor of setting EAP properties.
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

  explicit StringToEnum(const Pair* list, size_t num_entries, Type unknown)
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
  PROPERTY_INDEX_CELLULAR_LAST_GOOD_APN,
  PROPERTY_INDEX_CERT_PATH,
  PROPERTY_INDEX_CONNECTABLE,
  PROPERTY_INDEX_CONNECTED_TECHNOLOGIES,
  PROPERTY_INDEX_CONNECTIVITY_STATE,
  PROPERTY_INDEX_DEFAULT_TECHNOLOGY,
  PROPERTY_INDEX_DEVICE,
  PROPERTY_INDEX_DEVICES,
  PROPERTY_INDEX_EAP_IDENTITY,
  PROPERTY_INDEX_EAP_METHOD,
  PROPERTY_INDEX_EAP_PHASE_2_AUTH,
  PROPERTY_INDEX_EAP_ANONYMOUS_IDENTITY,
  PROPERTY_INDEX_EAP_CLIENT_CERT,
  PROPERTY_INDEX_EAP_CERT_ID,
  PROPERTY_INDEX_EAP_CLIENT_CERT_NSS,
  PROPERTY_INDEX_EAP_PRIVATE_KEY,
  PROPERTY_INDEX_EAP_PRIVATE_KEY_PASSWORD,
  PROPERTY_INDEX_EAP_KEY_ID,
  PROPERTY_INDEX_EAP_CA_CERT,
  PROPERTY_INDEX_EAP_CA_CERT_ID,
  PROPERTY_INDEX_EAP_CA_CERT_NSS,
  PROPERTY_INDEX_EAP_USE_SYSTEM_CAS,
  PROPERTY_INDEX_EAP_PIN,
  PROPERTY_INDEX_EAP_PASSWORD,
  PROPERTY_INDEX_EAP_KEY_MGMT,
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
  PROPERTY_INDEX_L2TPIPSEC_CA_CERT,
  PROPERTY_INDEX_L2TPIPSEC_CERT,
  PROPERTY_INDEX_L2TPIPSEC_KEY,
  PROPERTY_INDEX_L2TPIPSEC_PASSWORD,
  PROPERTY_INDEX_L2TPIPSEC_PSK,
  PROPERTY_INDEX_L2TPIPSEC_USER,
  PROPERTY_INDEX_MANUFACTURER,
  PROPERTY_INDEX_MDN,
  PROPERTY_INDEX_MEID,
  PROPERTY_INDEX_MIN,
  PROPERTY_INDEX_MODE,
  PROPERTY_INDEX_MODEL_ID,
  PROPERTY_INDEX_NAME,
  PROPERTY_INDEX_NETWORK_TECHNOLOGY,
  PROPERTY_INDEX_OFFLINE_MODE,
  PROPERTY_INDEX_OPERATOR_CODE,
  PROPERTY_INDEX_OPERATOR_NAME,
  PROPERTY_INDEX_PASSPHRASE,
  PROPERTY_INDEX_PASSPHRASE_REQUIRED,
  PROPERTY_INDEX_PAYMENT_URL,
  PROPERTY_INDEX_POWERED,
  PROPERTY_INDEX_PRL_VERSION,
  PROPERTY_INDEX_PROFILES,
  PROPERTY_INDEX_PROVIDER,
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
  PROPERTY_INDEX_TYPE,
  PROPERTY_INDEX_UNKNOWN,
  PROPERTY_INDEX_USAGE_URL,
  PROPERTY_INDEX_WIFI_FREQUENCY,
  PROPERTY_INDEX_WIFI_HEX_SSID,
  PROPERTY_INDEX_WIFI_HIDDEN_SSID,
  PROPERTY_INDEX_WIFI_PHY_MODE,
};

StringToEnum<PropertyIndex>::Pair property_index_table[] = {
  { kActivationStateProperty, PROPERTY_INDEX_ACTIVATION_STATE },
  { kActiveProfileProperty, PROPERTY_INDEX_ACTIVE_PROFILE },
  { kAutoConnectProperty, PROPERTY_INDEX_AUTO_CONNECT },
  { kAvailableTechnologiesProperty, PROPERTY_INDEX_AVAILABLE_TECHNOLOGIES },
  { kCellularAllowRoamingProperty, PROPERTY_INDEX_CELLULAR_ALLOW_ROAMING },
  { kCellularApnProperty, PROPERTY_INDEX_CELLULAR_APN },
  { kCellularLastGoodApnProperty, PROPERTY_INDEX_CELLULAR_LAST_GOOD_APN },
  { kCarrierProperty, PROPERTY_INDEX_CARRIER },
  { kCertPathProperty, PROPERTY_INDEX_CERT_PATH },
  { kConnectableProperty, PROPERTY_INDEX_CONNECTABLE },
  { kConnectedTechnologiesProperty, PROPERTY_INDEX_CONNECTED_TECHNOLOGIES },
  { kConnectivityStateProperty, PROPERTY_INDEX_CONNECTIVITY_STATE },
  { kDefaultTechnologyProperty, PROPERTY_INDEX_DEFAULT_TECHNOLOGY },
  { kDeviceProperty, PROPERTY_INDEX_DEVICE },
  { kDevicesProperty, PROPERTY_INDEX_DEVICES },
  { kEapIdentityProperty, PROPERTY_INDEX_EAP_IDENTITY },
  { kEapMethodProperty, PROPERTY_INDEX_EAP_METHOD },
  { kEapPhase2AuthProperty, PROPERTY_INDEX_EAP_PHASE_2_AUTH },
  { kEapAnonymousIdentityProperty, PROPERTY_INDEX_EAP_ANONYMOUS_IDENTITY },
  { kEapClientCertProperty, PROPERTY_INDEX_EAP_CLIENT_CERT },
  { kEapCertIDProperty, PROPERTY_INDEX_EAP_CERT_ID },
  { kEapClientCertNssProperty, PROPERTY_INDEX_EAP_CLIENT_CERT_NSS },
  { kEapPrivateKeyProperty, PROPERTY_INDEX_EAP_PRIVATE_KEY },
  { kEapPrivateKeyPasswordProperty, PROPERTY_INDEX_EAP_PRIVATE_KEY_PASSWORD },
  { kEapKeyIDProperty, PROPERTY_INDEX_EAP_KEY_ID },
  { kEapCaCertProperty, PROPERTY_INDEX_EAP_CA_CERT },
  { kEapCaCertIDProperty, PROPERTY_INDEX_EAP_CA_CERT_ID },
  { kEapCaCertNssProperty, PROPERTY_INDEX_EAP_CA_CERT_NSS },
  { kEapUseSystemCAsProperty, PROPERTY_INDEX_EAP_USE_SYSTEM_CAS },
  { kEapPinProperty, PROPERTY_INDEX_EAP_PIN },
  { kEapPasswordProperty, PROPERTY_INDEX_EAP_PASSWORD },
  { kEapKeyMgmtProperty, PROPERTY_INDEX_EAP_KEY_MGMT },
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
  { kL2TPIPSecCACertProperty, PROPERTY_INDEX_L2TPIPSEC_CA_CERT },
  { kL2TPIPSecCertProperty, PROPERTY_INDEX_L2TPIPSEC_CERT },
  { kL2TPIPSecKeyProperty, PROPERTY_INDEX_L2TPIPSEC_KEY },
  { kL2TPIPSecPasswordProperty, PROPERTY_INDEX_L2TPIPSEC_PASSWORD },
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
  { kOfflineModeProperty, PROPERTY_INDEX_OFFLINE_MODE },
  { kOperatorCodeProperty, PROPERTY_INDEX_OPERATOR_CODE },
  { kOperatorNameProperty, PROPERTY_INDEX_OPERATOR_NAME },
  { kPRLVersionProperty, PROPERTY_INDEX_PRL_VERSION },
  { kPassphraseProperty, PROPERTY_INDEX_PASSPHRASE },
  { kPassphraseRequiredProperty, PROPERTY_INDEX_PASSPHRASE_REQUIRED },
  { kPaymentURLProperty, PROPERTY_INDEX_PAYMENT_URL },
  { kPoweredProperty, PROPERTY_INDEX_POWERED },
  { kProfilesProperty, PROPERTY_INDEX_PROFILES },
  { kProviderProperty, PROPERTY_INDEX_PROVIDER },
  { kRoamingStateProperty, PROPERTY_INDEX_ROAMING_STATE },
  { kSaveCredentialsProperty, PROPERTY_INDEX_SAVE_CREDENTIALS },
  { kScanningProperty, PROPERTY_INDEX_SCANNING },
  { kSecurityProperty, PROPERTY_INDEX_SECURITY },
  { kSelectedNetworkProperty, PROPERTY_INDEX_SELECTED_NETWORK },
  { kServiceWatchListProperty, PROPERTY_INDEX_SERVICE_WATCH_LIST },
  { kServicesProperty, PROPERTY_INDEX_SERVICES },
  { kServingOperatorProperty, PROPERTY_INDEX_SERVING_OPERATOR },
  { kSignalStrengthProperty, PROPERTY_INDEX_SIGNAL_STRENGTH },
  { kSIMLockStatusProperty, PROPERTY_INDEX_SIM_LOCK },
  { kStateProperty, PROPERTY_INDEX_STATE },
  { kSupportNetworkScanProperty, PROPERTY_INDEX_SUPPORT_NETWORK_SCAN },
  { kTypeProperty, PROPERTY_INDEX_TYPE },
  { kUsageURLProperty, PROPERTY_INDEX_USAGE_URL },
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
  };
  static StringToEnum<ConnectionError> parser(
      table, arraysize(table), ERROR_UNKNOWN);
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

static ConnectivityState ParseConnectivityState(const std::string& state) {
  static StringToEnum<ConnectivityState>::Pair table[] = {
    { kConnStateUnrestricted, CONN_STATE_UNRESTRICTED },
    { kConnStateRestricted, CONN_STATE_RESTRICTED },
    { kConnStateNone, CONN_STATE_NONE },
  };
  static StringToEnum<ConnectivityState> parser(
      table, arraysize(table), CONN_STATE_UNKNOWN);
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
  DCHECK(parsed_state != SIM_UNKNOWN) << "Unknown SIMLock state encountered";
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
    { kEapMethodPEAP.c_str(), EAP_METHOD_PEAP },
    { kEapMethodTLS.c_str(), EAP_METHOD_TLS },
    { kEapMethodTTLS.c_str(), EAP_METHOD_TTLS },
    { kEapMethodLEAP.c_str(), EAP_METHOD_LEAP },
  };
  static StringToEnum<EAPMethod> parser(
      table, arraysize(table), EAP_METHOD_UNKNOWN);
  return parser.Get(method);
}

static EAPPhase2Auth ParseEAPPhase2Auth(const std::string& auth) {
  static StringToEnum<EAPPhase2Auth>::Pair table[] = {
    { kEapPhase2AuthPEAPMD5.c_str(), EAP_PHASE_2_AUTH_MD5 },
    { kEapPhase2AuthPEAPMSCHAPV2.c_str(), EAP_PHASE_2_AUTH_MSCHAPV2 },
    { kEapPhase2AuthTTLSMD5.c_str(), EAP_PHASE_2_AUTH_MD5 },
    { kEapPhase2AuthTTLSMSCHAPV2.c_str(), EAP_PHASE_2_AUTH_MSCHAPV2 },
    { kEapPhase2AuthTTLSMSCHAP.c_str(), EAP_PHASE_2_AUTH_MSCHAP },
    { kEapPhase2AuthTTLSPAP.c_str(), EAP_PHASE_2_AUTH_PAP },
    { kEapPhase2AuthTTLSCHAP.c_str(), EAP_PHASE_2_AUTH_CHAP },
  };
  static StringToEnum<EAPPhase2Auth> parser(
      table, arraysize(table), EAP_PHASE_2_AUTH_AUTO);
  return parser.Get(auth);
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
  if (!CrosLibrary::Get()->EnsureLoaded()) {
    return false;
  } else {
    if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      LOG(ERROR) << "chromeos_library calls made from non UI thread!";
      NOTREACHED();
    }
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
    case PROPERTY_INDEX_FOUND_NETWORKS:
      if (value->IsType(Value::TYPE_LIST)) {
        return ParseFoundNetworksFromList(
            static_cast<const ListValue*>(value),
            &found_cellular_networks_);
      }
      break;
    case PROPERTY_INDEX_HOME_PROVIDER: {
      if (value->IsType(Value::TYPE_DICTIONARY)) {
        const DictionaryValue *dict =
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
      error_(ERROR_UNKNOWN),
      connectable_(true),
      is_active_(false),
      favorite_(false),
      auto_connect_(false),
      connectivity_state_(CONN_STATE_UNKNOWN),
      priority_order_(0),
      added_(false),
      notify_failure_(false),
      service_path_(service_path),
      type_(type) {}

Network::~Network() {}

void Network::SetState(ConnectionState new_state) {
  if (new_state == state_)
    return;
  state_ = new_state;
  if (new_state == STATE_FAILURE) {
    // The user needs to be notified of this failure.
    notify_failure_ = true;
  } else {
    // State changed, so refresh IP address.
    // Note: blocking DBus call. TODO(stevenjb): refactor this.
    InitIPAddress();
  }
  VLOG(1) << " " << name() << ".State = " << GetStateString();
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
      return value->GetAsBoolean(&favorite_);
    case PROPERTY_INDEX_AUTO_CONNECT:
      return value->GetAsBoolean(&auto_connect_);
    case PROPERTY_INDEX_CONNECTIVITY_STATE: {
      std::string connectivity_state_string;
      if (value->GetAsString(&connectivity_state_string)) {
        connectivity_state_ = ParseConnectivityState(connectivity_state_string);
        return true;
      }
      break;
    }
    default:
      break;
  }
  return false;
}

void Network::ParseInfo(const DictionaryValue* info) {
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
}

void Network::SetValueProperty(const char* prop, Value* val) {
  DCHECK(prop);
  DCHECK(val);
  if (!EnsureCrosLoaded())
    return;
  SetNetworkServiceProperty(service_path_.c_str(), prop, val);
}

void Network::ClearProperty(const char* prop) {
  DCHECK(prop);
  if (!EnsureCrosLoaded())
    return;
  ClearNetworkServiceProperty(service_path_.c_str(), prop);
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

void Network::SetAutoConnect(bool auto_connect) {
  SetBooleanProperty(kAutoConnectProperty, auto_connect, &auto_connect_);
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
  }
  return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_UNRECOGNIZED);
}

std::string Network::GetErrorString() const {
  switch (error_) {
    case ERROR_UNKNOWN:
      return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_ERROR_UNKNOWN);
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
  }
  return l10n_util::GetStringUTF8(IDS_CHROMEOS_NETWORK_STATE_UNRECOGNIZED);
}

void Network::InitIPAddress() {
  ip_address_.clear();
  // If connected, get ip config.
  if (EnsureCrosLoaded() && connected() && !device_path_.empty()) {
    IPConfigStatus* ipconfig_status = ListIPConfigs(device_path_.c_str());
    if (ipconfig_status) {
      for (int i = 0; i < ipconfig_status->size; i++) {
        IPConfig ipconfig = ipconfig_status->ips[i];
        if (strlen(ipconfig.address) > 0) {
          ip_address_ = ipconfig.address;
          break;
        }
      }
      FreeIPConfigStatus(ipconfig_status);
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
    case PROPERTY_INDEX_L2TPIPSEC_CA_CERT:
      return value->GetAsString(&ca_cert_);
    case PROPERTY_INDEX_L2TPIPSEC_PSK:
      return value->GetAsString(&psk_passphrase_);
    case PROPERTY_INDEX_L2TPIPSEC_CERT:
      return value->GetAsString(&user_cert_);
    case PROPERTY_INDEX_L2TPIPSEC_KEY:
      return value->GetAsString(&user_cert_key_);
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
          << " Type: " << ProviderTypeToString(provider_type());
  if (provider_type_ == PROVIDER_TYPE_L2TP_IPSEC_PSK) {
    if (!user_cert_.empty())
      provider_type_ = PROVIDER_TYPE_L2TP_IPSEC_USER_CERT;
  }
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
      if (user_cert_.empty())
        return true;
      break;
    case PROVIDER_TYPE_MAX:
      break;
  }
  return false;
}

void VirtualNetwork::SetCACert(const std::string& ca_cert) {
  SetStringProperty(kL2TPIPSecCACertProperty, ca_cert, &ca_cert_);
}

void VirtualNetwork::SetPSKPassphrase(const std::string& psk_passphrase) {
  SetStringProperty(kL2TPIPSecPSKProperty, psk_passphrase,
                           &psk_passphrase_);
}

void VirtualNetwork::SetUserCert(const std::string& user_cert) {
  SetStringProperty(kL2TPIPSecCertProperty, user_cert, &user_cert_);
}

void VirtualNetwork::SetUserCertKey(const std::string& key) {
  SetStringProperty(kL2TPIPSecKeyProperty, key, &user_cert_key_);
}

void VirtualNetwork::SetUsername(const std::string& username) {
  SetStringProperty(kL2TPIPSecUserProperty, username, &username_);
}

void VirtualNetwork::SetUserPassphrase(const std::string& user_passphrase) {
  SetStringProperty(kL2TPIPSecPasswordProperty, user_passphrase,
                    &user_passphrase_);
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
                FormatBytes(plan_data_bytes,
                            GetByteDisplayUnits(plan_data_bytes),
                            true),
                base::TimeFormatFriendlyDate(plan_start_time));
    }
    case chromeos::CELLULAR_DATA_PLAN_METERED_BASE: {
      return l10n_util::GetStringFUTF16(
                IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_RECEIVED_FREE_DATA,
                FormatBytes(plan_data_bytes,
                            GetByteDisplayUnits(plan_data_bytes),
                            true),
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
      return FormatBytes(remaining_bytes,
          GetByteDisplayUnits(remaining_bytes),
          true);
    }
    case chromeos::CELLULAR_DATA_PLAN_METERED_BASE: {
      return FormatBytes(remaining_bytes,
          GetByteDisplayUnits(remaining_bytes),
          true);
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

////////////////////////////////////////////////////////////////////////////////
// CellularNetwork::Apn

CellularNetwork::Apn::Apn() {}

CellularNetwork::Apn::Apn(
    const std::string& apn, const std::string& network_id,
    const std::string& username, const std::string& password)
    : apn(apn), network_id(network_id),
      username(username), password(password) {
}

CellularNetwork::Apn::~Apn() {}

void CellularNetwork::Apn::Set(const DictionaryValue& dict) {
  if (!dict.GetStringWithoutPathExpansion(kApnProperty, &apn))
    apn.clear();
  if (!dict.GetStringWithoutPathExpansion(kNetworkIdProperty, &network_id))
    network_id.clear();
  if (!dict.GetStringWithoutPathExpansion(kUsernameProperty, &username))
    username.clear();
  if (!dict.GetStringWithoutPathExpansion(kPasswordProperty, &password))
    password.clear();
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
        const DictionaryValue *dict =
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
    case PROPERTY_INDEX_CONNECTIVITY_STATE: {
      // Save previous state before calling WirelessNetwork::ParseValue.
      ConnectivityState prev_state = connectivity_state_;
      if (WirelessNetwork::ParseValue(index, value)) {
        if (connectivity_state_ != prev_state)
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
  return ActivateCellularModem(service_path().c_str(), NULL);
}

void CellularNetwork::RefreshDataPlansIfNeeded() const {
  if (!EnsureCrosLoaded())
    return;
  if (connected() && activated())
    RequestCellularDataPlanUpdate(service_path().c_str());
}

void CellularNetwork::SetApn(const Apn& apn) {
  if (!EnsureCrosLoaded())
    return;

  if (!apn.apn.empty()) {
    DictionaryValue value;
    value.SetString(kApnProperty, apn.apn);
    value.SetString(kNetworkIdProperty, apn.network_id);
    value.SetString(kUsernameProperty, apn.username);
    value.SetString(kPasswordProperty, apn.password);
    SetValueProperty(kCellularApnProperty, &value);
  } else {
    ClearProperty(kCellularApnProperty);
  }
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
    default:
      return l10n_util::GetStringUTF8(
          IDS_CHROMEOS_NETWORK_CELLULAR_TECHNOLOGY_UNKNOWN);
      break;
  }
}

std::string CellularNetwork::GetConnectivityStateString() const {
  // These strings do not appear in the UI, so no need to localize them
  switch (connectivity_state_) {
    case CONN_STATE_UNRESTRICTED:
      return "unrestricted";
      break;
    case CONN_STATE_RESTRICTED:
      return "restricted";
      break;
    case CONN_STATE_NONE:
      return "none";
      break;
    case CONN_STATE_UNKNOWN:
    default:
      return "unknown";
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
      eap_use_system_cas_(true),
      save_credentials_(false) {
}

WifiNetwork::~WifiNetwork() {}

// Called from ParseNetwork after calling ParseInfo.
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
    LOG(ERROR) << "Iligal hex char is found in WiFi.HexSSID.";
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
    case PROPERTY_INDEX_SAVE_CREDENTIALS:
      return value->GetAsBoolean(&save_credentials_);
    case PROPERTY_INDEX_IDENTITY:
      return value->GetAsString(&identity_);
    case PROPERTY_INDEX_CERT_PATH:
      return value->GetAsString(&cert_path_);
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

void WifiNetwork::ParseInfo(const DictionaryValue* info) {
  Network::ParseInfo(info);
  CalculateUniqueId();
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

void WifiNetwork::SetSaveCredentials(bool save_credentials) {
  SetBooleanProperty(kSaveCredentialsProperty, save_credentials,
                     &save_credentials_);
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

void WifiNetwork::SetCertPath(const std::string& cert_path) {
  SetStringProperty(kCertPathProperty, cert_path, &cert_path_);
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
    case SECURITY_8021X:
      return "8021X";
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

// Parse 'path' to determine if the certificate is stored in a pkcs#11 device.
// flimflam recognizes the string "SETTINGS:" to specify authentication
// parameters. 'key_id=' indicates that the certificate is stored in a pkcs#11
// device. See src/third_party/flimflam/files/doc/service-api.txt.
bool WifiNetwork::IsCertificateLoaded() const {
  static const std::string settings_string("SETTINGS:");
  static const std::string pkcs11_key("key_id");
  if (cert_path_.find(settings_string) == 0) {
    std::string::size_type idx = cert_path_.find(pkcs11_key);
    if (idx != std::string::npos)
      idx = cert_path_.find_first_not_of(kWhitespaceASCII,
                                         idx + pkcs11_key.length());
    if (idx != std::string::npos && cert_path_[idx] == '=')
      return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkLibrary

class NetworkLibraryImpl : public NetworkLibrary  {
 public:
  NetworkLibraryImpl()
      : network_manager_monitor_(NULL),
        data_plan_monitor_(NULL),
        ethernet_(NULL),
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
    if (EnsureCrosLoaded()) {
      Init();
      network_manager_monitor_ =
          MonitorNetworkManager(&NetworkManagerStatusChangedHandler,
                                this);
      data_plan_monitor_ = MonitorCellularDataPlan(&DataPlanUpdateHandler,
                                                   this);
      network_login_observer_.reset(new NetworkLoginObserver(this));
    } else {
      InitTestData();
    }
  }

  virtual ~NetworkLibraryImpl() {
    network_manager_observers_.Clear();
    if (network_manager_monitor_)
      DisconnectPropertyChangeMonitor(network_manager_monitor_);
    data_plan_observers_.Clear();
    pin_operation_observers_.Clear();
    user_action_observers_.Clear();
    if (data_plan_monitor_)
      DisconnectDataPlanUpdateMonitor(data_plan_monitor_);
    STLDeleteValues(&network_observers_);
    STLDeleteValues(&network_device_observers_);
    ClearNetworks(true /*delete networks*/);
    ClearRememberedNetworks(true /*delete networks*/);
    STLDeleteValues(&data_plan_map_);
  }

  virtual void AddNetworkManagerObserver(NetworkManagerObserver* observer) {
    if (!network_manager_observers_.HasObserver(observer))
      network_manager_observers_.AddObserver(observer);
  }

  virtual void RemoveNetworkManagerObserver(NetworkManagerObserver* observer) {
    network_manager_observers_.RemoveObserver(observer);
  }

  virtual void AddNetworkObserver(const std::string& service_path,
                                  NetworkObserver* observer) {
    DCHECK(observer);
    if (!EnsureCrosLoaded())
      return;
    // First, add the observer to the callback map.
    NetworkObserverMap::iterator iter = network_observers_.find(service_path);
    NetworkObserverList* oblist;
    if (iter != network_observers_.end()) {
      oblist = iter->second;
    } else {
      oblist = new NetworkObserverList(this, service_path);
      network_observers_[service_path] = oblist;
    }
    if (!oblist->HasObserver(observer))
      oblist->AddObserver(observer);
  }

  virtual void RemoveNetworkObserver(const std::string& service_path,
                                     NetworkObserver* observer) {
    DCHECK(observer);
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
  }

  virtual void AddNetworkDeviceObserver(const std::string& device_path,
                                        NetworkDeviceObserver* observer) {
    DCHECK(observer);
    if (!EnsureCrosLoaded())
      return;
    // First, add the observer to the callback map.
    NetworkDeviceObserverMap::iterator iter =
        network_device_observers_.find(device_path);
    NetworkDeviceObserverList* oblist;
    if (iter != network_device_observers_.end()) {
      oblist = iter->second;
      if (!oblist->HasObserver(observer))
        oblist->AddObserver(observer);
    } else {
      LOG(WARNING) << "No NetworkDeviceObserverList found for "
                   << device_path;
    }
  }

  virtual void RemoveNetworkDeviceObserver(const std::string& device_path,
                                           NetworkDeviceObserver* observer) {
    DCHECK(observer);
    DCHECK(device_path.size());
    NetworkDeviceObserverMap::iterator map_iter =
        network_device_observers_.find(device_path);
    if (map_iter != network_device_observers_.end()) {
      map_iter->second->RemoveObserver(observer);
    }
  }

  virtual void RemoveObserverForAllNetworks(NetworkObserver* observer) {
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

  virtual void Lock() {
    if (is_locked_)
      return;
    is_locked_ = true;
    NotifyNetworkManagerChanged(true);  // Forced update.
  }

  virtual void Unlock() {
    DCHECK(is_locked_);
    if (!is_locked_)
      return;
    is_locked_ = false;
    NotifyNetworkManagerChanged(true);  // Forced update.
  }

  virtual bool IsLocked() {
    return is_locked_;
  }

  virtual void AddCellularDataPlanObserver(CellularDataPlanObserver* observer) {
    if (!data_plan_observers_.HasObserver(observer))
      data_plan_observers_.AddObserver(observer);
  }

  virtual void RemoveCellularDataPlanObserver(
      CellularDataPlanObserver* observer) {
    data_plan_observers_.RemoveObserver(observer);
  }

  virtual void AddPinOperationObserver(PinOperationObserver* observer) {
    if (!pin_operation_observers_.HasObserver(observer))
      pin_operation_observers_.AddObserver(observer);
  }

  virtual void RemovePinOperationObserver(PinOperationObserver* observer) {
    pin_operation_observers_.RemoveObserver(observer);
  }

  virtual void AddUserActionObserver(UserActionObserver* observer) {
    if (!user_action_observers_.HasObserver(observer))
      user_action_observers_.AddObserver(observer);
  }

  virtual void RemoveUserActionObserver(UserActionObserver* observer) {
    user_action_observers_.RemoveObserver(observer);
  }

  virtual const EthernetNetwork* ethernet_network() const { return ethernet_; }
  virtual bool ethernet_connecting() const {
    return ethernet_ ? ethernet_->connecting() : false;
  }
  virtual bool ethernet_connected() const {
    return ethernet_ ? ethernet_->connected() : false;
  }

  virtual const WifiNetwork* wifi_network() const { return active_wifi_; }
  virtual bool wifi_connecting() const {
    return active_wifi_ ? active_wifi_->connecting() : false;
  }
  virtual bool wifi_connected() const {
    return active_wifi_ ? active_wifi_->connected() : false;
  }

  virtual const CellularNetwork* cellular_network() const {
    return active_cellular_;
  }
  virtual bool cellular_connecting() const {
    return active_cellular_ ? active_cellular_->connecting() : false;
  }
  virtual bool cellular_connected() const {
    return active_cellular_ ? active_cellular_->connected() : false;
  }
  virtual const VirtualNetwork* virtual_network() const {
    return active_virtual_;
  }
  virtual bool virtual_network_connecting() const {
    return active_virtual_ ? active_virtual_->connecting() : false;
  }
  virtual bool virtual_network_connected() const {
    return active_virtual_ ? active_virtual_->connected() : false;
  }

  bool Connected() const {
    return ethernet_connected() || wifi_connected() || cellular_connected();
  }

  bool Connecting() const {
    return ethernet_connecting() || wifi_connecting() || cellular_connecting();
  }

  const std::string& IPAddress() const {
    // Returns IP address for the active network.
    // TODO(stevenjb): Fix this for VPNs. See chromium-os:13972.
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

  virtual const WifiNetworkVector& wifi_networks() const {
    return wifi_networks_;
  }

  virtual const WifiNetworkVector& remembered_wifi_networks() const {
    return remembered_wifi_networks_;
  }

  virtual const CellularNetworkVector& cellular_networks() const {
    return cellular_networks_;
  }

  virtual const VirtualNetworkVector& virtual_networks() const {
    return virtual_networks_;
  }

  /////////////////////////////////////////////////////////////////////////////

  virtual const NetworkDevice* FindNetworkDeviceByPath(
      const std::string& path) const {
    NetworkDeviceMap::const_iterator iter = device_map_.find(path);
    if (iter != device_map_.end())
      return iter->second;
    LOG(WARNING) << "Device path not found: " << path;
    return NULL;
  }

  virtual const NetworkDevice* FindCellularDevice() const {
    for (NetworkDeviceMap::const_iterator iter = device_map_.begin();
         iter != device_map_.end(); ++iter) {
      if (iter->second->type() == TYPE_CELLULAR)
        return iter->second;
    }
    return NULL;
  }

  virtual const NetworkDevice* FindEthernetDevice() const {
    for (NetworkDeviceMap::const_iterator iter = device_map_.begin();
         iter != device_map_.end(); ++iter) {
      if (iter->second->type() == TYPE_ETHERNET)
        return iter->second;
    }
    return NULL;
  }

  virtual const NetworkDevice* FindWifiDevice() const {
    for (NetworkDeviceMap::const_iterator iter = device_map_.begin();
         iter != device_map_.end(); ++iter) {
      if (iter->second->type() == TYPE_WIFI)
        return iter->second;
    }
    return NULL;
  }

  virtual Network* FindNetworkByPath(const std::string& path) const {
    NetworkMap::const_iterator iter = network_map_.find(path);
    if (iter != network_map_.end())
      return iter->second;
    return NULL;
  }

  WirelessNetwork* FindWirelessNetworkByPath(const std::string& path) const {
    Network* network = FindNetworkByPath(path);
    if (network &&
        (network->type() == TYPE_WIFI || network->type() == TYPE_CELLULAR))
      return static_cast<WirelessNetwork*>(network);
    return NULL;
  }

  virtual WifiNetwork* FindWifiNetworkByPath(const std::string& path) const {
    Network* network = FindNetworkByPath(path);
    if (network && network->type() == TYPE_WIFI)
      return static_cast<WifiNetwork*>(network);
    return NULL;
  }

  virtual CellularNetwork* FindCellularNetworkByPath(
      const std::string& path) const {
    Network* network = FindNetworkByPath(path);
    if (network && network->type() == TYPE_CELLULAR)
      return static_cast<CellularNetwork*>(network);
    return NULL;
  }

  virtual VirtualNetwork* FindVirtualNetworkByPath(
      const std::string& path) const {
    Network* network = FindNetworkByPath(path);
    if (network && network->type() == TYPE_VPN)
      return static_cast<VirtualNetwork*>(network);
    return NULL;
  }

  virtual Network* FindNetworkFromRemembered(
      const Network* remembered) const {
    NetworkMap::const_iterator found =
        network_unique_id_map_.find(remembered->unique_id());
    if (found != network_unique_id_map_.end())
      return found->second;
    return NULL;
  }

  virtual const CellularDataPlanVector* GetDataPlans(
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

  virtual const CellularDataPlan* GetSignificantDataPlan(
      const std::string& path) const {
    const CellularDataPlanVector* plans = GetDataPlans(path);
    if (plans)
      return GetSignificantDataPlanFromVector(plans);
    return NULL;
  }

  /////////////////////////////////////////////////////////////////////////////

  virtual void ChangePin(const std::string& old_pin,
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

  virtual void ChangeRequirePin(bool require_pin,
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

  virtual void EnterPin(const std::string& pin) {
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

  virtual void UnblockPin(const std::string& puk,
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

  static void PinOperationCallback(void* object,
                                   const char* path,
                                   NetworkMethodErrorType error,
                                   const char* error_message) {
    NetworkLibraryImpl* networklib = static_cast<NetworkLibraryImpl*>(object);
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

  virtual void RequestCellularScan() {
    const NetworkDevice* cellular = FindCellularDevice();
    if (!cellular) {
      NOTREACHED() << "Calling RequestCellularScan method w/o cellular device.";
      return;
    }
    chromeos::ProposeScan(cellular->device_path().c_str());
  }

  virtual void RequestCellularRegister(const std::string& network_id) {
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

  static void CellularRegisterCallback(void* object,
                                       const char* path,
                                       NetworkMethodErrorType error,
                                       const char* error_message) {
    NetworkLibraryImpl* networklib = static_cast<NetworkLibraryImpl*>(object);
    DCHECK(networklib);
    // TODO(dpolukhin): Notify observers about network registration status
    // but not UI doesn't assume such notification so just ignore result.
  }

  virtual void SetCellularDataRoamingAllowed(bool new_value) {
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

  /////////////////////////////////////////////////////////////////////////////

  virtual void RequestNetworkScan() {
    if (EnsureCrosLoaded()) {
      if (wifi_enabled()) {
        wifi_scanning_ = true;  // Cleared when updates are received.
        chromeos::RequestNetworkScan(kTypeWifi);
        RequestRememberedNetworksUpdate();
      }
      if (cellular_network())
        cellular_network()->RefreshDataPlansIfNeeded();
    }
  }

  virtual bool GetWifiAccessPoints(WifiAccessPointVector* result) {
    if (!EnsureCrosLoaded())
      return false;
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    DeviceNetworkList* network_list = GetDeviceNetworkList();
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
    FreeDeviceNetworkList(network_list);
    return true;
  }

  static void NetworkConnectCallback(void *object,
                                     const char *path,
                                     NetworkMethodErrorType error,
                                     const char* error_message) {
    NetworkLibraryImpl* networklib = static_cast<NetworkLibraryImpl*>(object);
    DCHECK(networklib);

    Network* network = networklib->FindNetworkByPath(path);
    if (!network) {
      LOG(ERROR) << "No network for path: " << path;
      return;
    }

    if (error != NETWORK_METHOD_ERROR_NONE) {
      LOG(WARNING) << "Error from ServiceConnect callback for: "
                   << network->name()
                   << " Error: " << error << " Message: " << error_message;
      // This will trigger the connection failed notification.
      // TODO(stevenjb): Remove if chromium-os:13203 gets fixed.
      network->SetState(STATE_FAILURE);
      if (error_message &&
          strcmp(error_message, kErrorPassphraseRequiredMsg) == 0) {
        network->set_error(ERROR_BAD_PASSPHRASE);
      } else {
        network->set_error(ERROR_CONNECT_FAILED);
      }
      networklib->NotifyNetworkManagerChanged(true);  // Forced update.
      return;
    }

    VLOG(1) << "Connected to service: " << network->name();

    // Update local cache and notify listeners.
    if (network->type() == TYPE_WIFI) {
      WifiNetwork* wifi = static_cast<WifiNetwork *>(network);
      // If the user asked not to save credentials, flimflam will have
      // forgotten them.  Wipe our cache as well.
      if (!wifi->save_credentials()) {
        wifi->EraseCredentials();
      }
      networklib->active_wifi_ = wifi;
    } else if (network->type() == TYPE_CELLULAR) {
      networklib->active_cellular_ = static_cast<CellularNetwork *>(network);
    } else if (network->type() == TYPE_VPN) {
      networklib->active_virtual_ = static_cast<VirtualNetwork *>(network);
    } else {
      LOG(ERROR) << "Network of unexpected type: " << network->type();
    }

    // If we succeed, this network will be remembered; request an update.
    // TODO(stevenjb): flimflam should do this automatically.
    networklib->RequestRememberedNetworksUpdate();
    // Notify observers.
    networklib->NotifyNetworkManagerChanged(true);  // Forced update.
    networklib->NotifyUserConnectionInitiated(network);
  }


  void CallConnectToNetwork(Network* network) {
    DCHECK(network);
    if (!EnsureCrosLoaded() || !network)
      return;
    // In order to be certain to trigger any notifications, set the connecting
    // state locally and notify observers. Otherwise there might be a state
    // change without a forced notify.
    network->set_connecting(true);
    NotifyNetworkManagerChanged(true);  // Forced update.
    VLOG(1) << "Requesting connect to network: " << network->service_path();
    RequestNetworkServiceConnect(network->service_path().c_str(),
                                 NetworkConnectCallback, this);
  }

  virtual void ConnectToWifiNetwork(WifiNetwork* wifi) {
    // This will happen if a network resets or gets out of range.
    if (wifi->user_passphrase_ != wifi->passphrase_ ||
        wifi->passphrase_required())
      wifi->SetPassphrase(wifi->user_passphrase_);
    CallConnectToNetwork(wifi);
  }

  // Use this to connect to a wifi network by service path.
  virtual void ConnectToWifiNetwork(const std::string& service_path) {
    WifiNetwork* wifi = FindWifiNetworkByPath(service_path);
    if (!wifi) {
      LOG(WARNING) << "Attempt to connect to non existing network: "
                   << service_path;
      return;
    }
    ConnectToWifiNetwork(wifi);
  }

  // Use this to connect to an unlisted wifi network.
  // This needs to request information about the named service.
  // The connection attempt will occur in the callback.
  virtual void ConnectToWifiNetwork(const std::string& ssid,
                                    ConnectionSecurity security,
                                    const std::string& passphrase) {
    DCHECK(security != SECURITY_8021X);
    if (!EnsureCrosLoaded())
      return;
    // Store the connection data to be used by the callback.
    connect_data_.security = security;
    connect_data_.service_name = ssid;
    connect_data_.passphrase = passphrase;
    // Asynchronously request service properties and call
    // WifiServiceUpdateAndConnect.
    RequestHiddenWifiNetwork(ssid.c_str(),
                             SecurityToString(security),
                             WifiServiceUpdateAndConnect,
                             this);
  }

  // As above, but for 802.1X EAP networks.
  virtual void ConnectToWifiNetwork8021x(
      const std::string& ssid,
      EAPMethod eap_method,
      EAPPhase2Auth eap_auth,
      const std::string& eap_server_ca_cert_nss_nickname,
      bool eap_use_system_cas,
      const std::string& eap_client_cert_pkcs11_id,
      const std::string& eap_identity,
      const std::string& eap_anonymous_identity,
      const std::string& passphrase,
      bool save_credentials) {
    if (!EnsureCrosLoaded())
      return;
    connect_data_.security = SECURITY_8021X;
    connect_data_.service_name = ssid;
    connect_data_.eap_method = eap_method;
    connect_data_.eap_auth = eap_auth;
    connect_data_.eap_server_ca_cert_nss_nickname =
        eap_server_ca_cert_nss_nickname;
    connect_data_.eap_use_system_cas = eap_use_system_cas;
    connect_data_.eap_client_cert_pkcs11_id = eap_client_cert_pkcs11_id;
    connect_data_.eap_identity = eap_identity;
    connect_data_.eap_anonymous_identity = eap_anonymous_identity;
    connect_data_.passphrase = passphrase;
    connect_data_.save_credentials = save_credentials;
    RequestHiddenWifiNetwork(ssid.c_str(),
                             SecurityToString(SECURITY_8021X),
                             WifiServiceUpdateAndConnect,
                             this);
  }

  // Callback
  static void WifiServiceUpdateAndConnect(void* object,
                                          const char* service_path,
                                          const Value* info) {
    NetworkLibraryImpl* networklib = static_cast<NetworkLibraryImpl*>(object);
    DCHECK(networklib);
    if (service_path && info) {
      DCHECK_EQ(info->GetType(), Value::TYPE_DICTIONARY);
      const DictionaryValue* dict = static_cast<const DictionaryValue*>(info);
      Network* network =
          networklib->ParseNetwork(std::string(service_path), dict);
      DCHECK(network->type() == TYPE_WIFI);
      networklib->ConnectToWifiNetworkUsingConnectData(
          static_cast<WifiNetwork*>(network));
    }
  }

  void ConnectToWifiNetworkUsingConnectData(WifiNetwork* wifi) {
    ConnectData& data = connect_data_;
    if (wifi->name() != data.service_name) {
      LOG(WARNING) << "Wifi network name does not match ConnectData: "
                   << wifi->name() << " != " << data.service_name;
      return;
    }
    wifi->set_added(true);
    if (data.security == SECURITY_8021X) {
      // Enterprise 802.1X EAP network.
      wifi->SetEAPMethod(data.eap_method);
      wifi->SetEAPPhase2Auth(data.eap_auth);
      wifi->SetEAPServerCaCertNssNickname(data.eap_server_ca_cert_nss_nickname);
      wifi->SetEAPUseSystemCAs(data.eap_use_system_cas);
      wifi->SetEAPClientCertPkcs11Id(data.eap_client_cert_pkcs11_id);
      wifi->SetEAPIdentity(data.eap_identity);
      wifi->SetEAPAnonymousIdentity(data.eap_anonymous_identity);
      wifi->SetEAPPassphrase(data.passphrase);
      wifi->SetSaveCredentials(data.save_credentials);
    } else {
      // Ordinary, non-802.1X network.
      wifi->SetPassphrase(data.passphrase);
    }

    ConnectToWifiNetwork(wifi);
  }

  virtual void ConnectToCellularNetwork(CellularNetwork* cellular) {
    CallConnectToNetwork(cellular);
  }

  // Records information that cellular play payment had happened.
  virtual void SignalCellularPlanPayment() {
    DCHECK(!HasRecentCellularPlanPayment());
    cellular_plan_payment_time_ = base::Time::Now();
  }

  // Returns true if cellular plan payment had been recorded recently.
  virtual bool HasRecentCellularPlanPayment() {
    return (base::Time::Now() -
            cellular_plan_payment_time_).InHours() < kRecentPlanPaymentHours;
  }

  virtual void ConnectToVirtualNetwork(VirtualNetwork* vpn) {
    CallConnectToNetwork(vpn);
  }

  virtual void ConnectToVirtualNetworkPSK(
      const std::string& service_name,
      const std::string& server,
      const std::string& psk,
      const std::string& username,
      const std::string& user_passphrase) {
    if (!EnsureCrosLoaded())
      return;
    // Store the connection data to be used by the callback.
    connect_data_.service_name = service_name;
    connect_data_.psk_key = psk;
    connect_data_.server_hostname = server;
    connect_data_.psk_username = username;
    connect_data_.passphrase = user_passphrase;
    RequestVirtualNetwork(service_name.c_str(),
                          server.c_str(),
                          kProviderL2tpIpsec,
                          VPNServiceUpdateAndConnect,
                          this);
  }

  // Callback
  static void VPNServiceUpdateAndConnect(void* object,
                                         const char* service_path,
                                         const Value* info) {
    NetworkLibraryImpl* networklib = static_cast<NetworkLibraryImpl*>(object);
    DCHECK(networklib);
    if (service_path && info) {
      VLOG(1) << "Connecting to new VPN Service: " << service_path;
      DCHECK_EQ(info->GetType(), Value::TYPE_DICTIONARY);
      const DictionaryValue* dict = static_cast<const DictionaryValue*>(info);
      Network* network =
          networklib->ParseNetwork(std::string(service_path), dict);
      DCHECK(network->type() == TYPE_VPN);
      networklib->ConnectToVirtualNetworkUsingConnectData(
          static_cast<VirtualNetwork*>(network));
    } else {
      LOG(WARNING) << "Unable to create VPN Service: " << service_path;
    }
  }

  void ConnectToVirtualNetworkUsingConnectData(VirtualNetwork* vpn) {
    ConnectData& data = connect_data_;
    if (vpn->name() != data.service_name) {
      LOG(WARNING) << "Virtual network name does not match ConnectData: "
                   << vpn->name() << " != " << data.service_name;
      return;
    }

    vpn->set_added(true);
    if (!data.server_hostname.empty())
      vpn->set_server_hostname(data.server_hostname);
    vpn->SetCACert("");
    vpn->SetUserCert("");
    vpn->SetUserCertKey("");
    vpn->SetPSKPassphrase(data.psk_key);
    vpn->SetUsername(data.psk_username);
    vpn->SetUserPassphrase(data.passphrase);

    CallConnectToNetwork(vpn);
  }

  virtual void DisconnectFromNetwork(const Network* network) {
    DCHECK(network);
    if (!EnsureCrosLoaded() || !network)
      return;
    VLOG(1) << "Disconnect from network: " << network->service_path();
    if (chromeos::DisconnectFromNetwork(network->service_path().c_str())) {
      // Update local cache and notify listeners.
      Network* found_network = FindNetworkByPath(network->service_path());
      if (found_network) {
        found_network->set_connected(false);
        if (found_network == active_wifi_)
          active_wifi_ = NULL;
        else if (found_network == active_cellular_)
          active_cellular_ = NULL;
        else if (found_network == active_virtual_)
          active_virtual_ = NULL;
      }
      NotifyNetworkManagerChanged(true);  // Forced update.
    }
  }

  virtual void ForgetWifiNetwork(const std::string& service_path) {
    if (!EnsureCrosLoaded())
      return;
    DeleteRememberedService(service_path.c_str());
    DeleteRememberedWifiNetwork(service_path);
    NotifyNetworkManagerChanged(true);  // Forced update.
  }

  virtual std::string GetCellularHomeCarrierId() const {
    std::string carrier_id;
    const NetworkDevice* cellular = FindCellularDevice();
    if (cellular) {
      return cellular->home_provider_id();

    }
    return carrier_id;
  }

  virtual bool ethernet_available() const {
    return available_devices_ & (1 << TYPE_ETHERNET);
  }
  virtual bool wifi_available() const {
    return available_devices_ & (1 << TYPE_WIFI);
  }
  virtual bool cellular_available() const {
    return available_devices_ & (1 << TYPE_CELLULAR);
  }

  virtual bool ethernet_enabled() const {
    return enabled_devices_ & (1 << TYPE_ETHERNET);
  }
  virtual bool wifi_enabled() const {
    return enabled_devices_ & (1 << TYPE_WIFI);
  }
  virtual bool cellular_enabled() const {
    return enabled_devices_ & (1 << TYPE_CELLULAR);
  }

  virtual bool wifi_scanning() const {
    return wifi_scanning_;
  }

  virtual bool offline_mode() const { return offline_mode_; }

  // Note: This does not include any virtual networks.
  virtual const Network* active_network() const {
    // Use flimflam's ordering of the services to determine what the active
    // network is (i.e. don't assume priority of network types).
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

  virtual const Network* connected_network() const {
    // Use flimflam's ordering of the services to determine what the connected
    // network is (i.e. don't assume priority of network types).
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

  virtual void EnableEthernetNetworkDevice(bool enable) {
    if (is_locked_)
      return;
    EnableNetworkDeviceType(TYPE_ETHERNET, enable);
  }

  virtual void EnableWifiNetworkDevice(bool enable) {
    if (is_locked_)
      return;
    EnableNetworkDeviceType(TYPE_WIFI, enable);
  }

  virtual void EnableCellularNetworkDevice(bool enable) {
    if (is_locked_)
      return;
    EnableNetworkDeviceType(TYPE_CELLULAR, enable);
  }

  virtual void EnableOfflineMode(bool enable) {
    if (!EnsureCrosLoaded())
      return;

    // If network device is already enabled/disabled, then don't do anything.
    if (enable && offline_mode_) {
      VLOG(1) << "Trying to enable offline mode when it's already enabled.";
      return;
    }
    if (!enable && !offline_mode_) {
      VLOG(1) << "Trying to disable offline mode when it's already disabled.";
      return;
    }

    if (SetOfflineMode(enable)) {
      offline_mode_ = enable;
    }
  }

  virtual NetworkIPConfigVector GetIPConfigs(const std::string& device_path,
                                             std::string* hardware_address,
                                             HardwareAddressFormat format) {
    DCHECK(hardware_address);
    hardware_address->clear();
    NetworkIPConfigVector ipconfig_vector;
    if (EnsureCrosLoaded() && !device_path.empty()) {
      IPConfigStatus* ipconfig_status = ListIPConfigs(device_path.c_str());
      if (ipconfig_status) {
        for (int i = 0; i < ipconfig_status->size; i++) {
          IPConfig ipconfig = ipconfig_status->ips[i];
          ipconfig_vector.push_back(
              NetworkIPConfig(device_path, ipconfig.type, ipconfig.address,
                              ipconfig.netmask, ipconfig.gateway,
                              ipconfig.name_servers));
        }
        *hardware_address = ipconfig_status->hardware_address;
        FreeIPConfigStatus(ipconfig_status);
        // Sort the list of ip configs by type.
        std::sort(ipconfig_vector.begin(), ipconfig_vector.end());
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
      DCHECK(format == FORMAT_RAW_HEX);
    }
    return ipconfig_vector;
  }

 private:

  typedef std::map<std::string, Network*> NetworkMap;
  typedef std::map<std::string, int> PriorityMap;
  typedef std::map<std::string, NetworkDevice*> NetworkDeviceMap;
  typedef std::map<std::string, CellularDataPlanVector*> CellularDataPlanMap;

  class NetworkObserverList : public ObserverList<NetworkObserver> {
   public:
    NetworkObserverList(NetworkLibraryImpl* library,
                        const std::string& service_path) {
      network_monitor_ = MonitorNetworkService(&NetworkStatusChangedHandler,
                                               service_path.c_str(),
                                               library);
    }

    virtual ~NetworkObserverList() {
      if (network_monitor_)
        DisconnectPropertyChangeMonitor(network_monitor_);
    }

   private:
    static void NetworkStatusChangedHandler(void* object,
                                            const char* path,
                                            const char* key,
                                            const Value* value) {
      NetworkLibraryImpl* networklib = static_cast<NetworkLibraryImpl*>(object);
      DCHECK(networklib);
      networklib->UpdateNetworkStatus(path, key, value);
    }
    PropertyChangeMonitor network_monitor_;
    DISALLOW_COPY_AND_ASSIGN(NetworkObserverList);
  };

  typedef std::map<std::string, NetworkObserverList*> NetworkObserverMap;

  class NetworkDeviceObserverList : public ObserverList<NetworkDeviceObserver> {
   public:
    NetworkDeviceObserverList(NetworkLibraryImpl* library,
                              const std::string& device_path) {
      device_monitor_ = MonitorNetworkDevice(
          &NetworkDevicePropertyChangedHandler,
          device_path.c_str(),
          library);
    }

    virtual ~NetworkDeviceObserverList() {
      if (device_monitor_)
        DisconnectPropertyChangeMonitor(device_monitor_);
    }

   private:
    static void NetworkDevicePropertyChangedHandler(void* object,
                                                    const char* path,
                                                    const char* key,
                                                    const Value* value) {
      NetworkLibraryImpl* networklib = static_cast<NetworkLibraryImpl*>(object);
      DCHECK(networklib);
      networklib->UpdateNetworkDeviceStatus(path, key, value);
    }
    PropertyChangeMonitor device_monitor_;
    DISALLOW_COPY_AND_ASSIGN(NetworkDeviceObserverList);
  };

  typedef std::map<std::string, NetworkDeviceObserverList*>
      NetworkDeviceObserverMap;

  ////////////////////////////////////////////////////////////////////////////
  // Callbacks.

  static void NetworkManagerStatusChangedHandler(void* object,
                                                 const char* path,
                                                 const char* key,
                                                 const Value* value) {
    NetworkLibraryImpl* networklib = static_cast<NetworkLibraryImpl*>(object);
    DCHECK(networklib);
    networklib->NetworkManagerStatusChanged(key, value);
  }

  // This processes all Manager update messages.
  void NetworkManagerStatusChanged(const char* key, const Value* value) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    base::TimeTicks start = base::TimeTicks::Now();
    VLOG(1) << "NetworkManagerStatusChanged: KEY=" << key;
    if (!key)
      return;
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
        DCHECK_EQ(value->GetType(), Value::TYPE_STRING);
        value->GetAsString(&active_profile_path_);
        RequestRememberedNetworksUpdate();
        break;
      }
      case PROPERTY_INDEX_PROFILES:
        // Currently we ignore Profiles (list of all profiles).
        break;
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
      default:
        LOG(WARNING) << "Unhandled key: " << key;
        break;
    }
    base::TimeDelta delta = base::TimeTicks::Now() - start;
    VLOG(1) << "  time: " << delta.InMilliseconds() << " ms.";
    HISTOGRAM_TIMES("CROS_NETWORK_UPDATE", delta);
  }

  static void NetworkManagerUpdate(void* object,
                                   const char* manager_path,
                                   const Value* info) {
    NetworkLibraryImpl* networklib = static_cast<NetworkLibraryImpl*>(object);
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

  void ParseNetworkManager(const DictionaryValue* dict) {
    for (DictionaryValue::key_iterator iter = dict->begin_keys();
         iter != dict->end_keys(); ++iter) {
      const std::string& key = *iter;
      Value* value;
      bool res = dict->GetWithoutPathExpansion(key, &value);
      CHECK(res);
      NetworkManagerStatusChanged(key.c_str(), value);
    }
  }

  static void ProfileUpdate(void* object,
                            const char* profile_path,
                            const Value* info) {
    NetworkLibraryImpl* networklib = static_cast<NetworkLibraryImpl*>(object);
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

  static void NetworkServiceUpdate(void* object,
                                   const char* service_path,
                                   const Value* info) {
    NetworkLibraryImpl* networklib = static_cast<NetworkLibraryImpl*>(object);
    DCHECK(networklib);
    if (service_path) {
      if (!info)
        return;  // Network no longer in visible list, ignore.
      DCHECK_EQ(info->GetType(), Value::TYPE_DICTIONARY);
      const DictionaryValue* dict = static_cast<const DictionaryValue*>(info);
      networklib->ParseNetwork(std::string(service_path), dict);
    }
  }

  static void RememberedNetworkServiceUpdate(void* object,
                                             const char* service_path,
                                             const Value* info) {
    NetworkLibraryImpl* networklib = static_cast<NetworkLibraryImpl*>(object);
    DCHECK(networklib);
    if (service_path) {
      if (!info) {
        // Remembered network no longer exists.
        networklib->DeleteRememberedWifiNetwork(std::string(service_path));
      } else {
        DCHECK_EQ(info->GetType(), Value::TYPE_DICTIONARY);
        const DictionaryValue* dict = static_cast<const DictionaryValue*>(info);
        networklib->ParseRememberedNetwork(std::string(service_path), dict);
      }
    }
  }

  static void NetworkDeviceUpdate(void* object,
                                  const char* device_path,
                                  const Value* info) {
    NetworkLibraryImpl* networklib = static_cast<NetworkLibraryImpl*>(object);
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

  static void DataPlanUpdateHandler(void* object,
                                    const char* modem_service_path,
                                    const CellularDataPlanList* dataplan) {
    NetworkLibraryImpl* networklib = static_cast<NetworkLibraryImpl*>(object);
    DCHECK(networklib);
    if (modem_service_path && dataplan) {
      networklib->UpdateCellularDataPlan(std::string(modem_service_path),
                                         dataplan);
    }
  }

  ////////////////////////////////////////////////////////////////////////////
  // Network technology functions.

  void UpdateTechnologies(const ListValue* technologies, int* bitfieldp) {
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

  void UpdateAvailableTechnologies(const ListValue* technologies) {
    UpdateTechnologies(technologies, &available_devices_);
  }

  void UpdateEnabledTechnologies(const ListValue* technologies) {
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

  void UpdateConnectedTechnologies(const ListValue* technologies) {
    UpdateTechnologies(technologies, &connected_devices_);
  }

  ////////////////////////////////////////////////////////////////////////////
  // Network list management functions.

  // Note: sometimes flimflam still returns networks when the device type is
  // disabled. Always check the appropriate enabled() state before adding
  // networks to a list or setting an active network so that we do not show them
  // in the UI.

  // This relies on services being requested from flimflam in priority order,
  // and the updates getting processed and received in order.
  void UpdateActiveNetwork(Network* network) {
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

  void AddNetwork(Network* network) {
    std::pair<NetworkMap::iterator,bool> result =
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
  void DeleteNetwork(Network* network) {
    CHECK(network_map_.find(network->service_path()) == network_map_.end());
    ConnectionType type(network->type());
    if (type == TYPE_CELLULAR) {
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

  void AddRememberedWifiNetwork(WifiNetwork* wifi) {
    std::pair<NetworkMap::iterator,bool> result =
        remembered_network_map_.insert(
            std::make_pair(wifi->service_path(), wifi));
    DCHECK(result.second);  // Should only get called with new network.
    remembered_wifi_networks_.push_back(wifi);
  }

  void DeleteRememberedWifiNetwork(const std::string& service_path) {
    NetworkMap::iterator found = remembered_network_map_.find(service_path);
    if (found == remembered_network_map_.end()) {
      LOG(WARNING) << "Attempt to delete non-existant remembered network: "
                   << service_path;
      return;
    }
    Network* remembered_network = found->second;
    remembered_network_map_.erase(found);
    WifiNetworkVector::iterator iter = std::find(
        remembered_wifi_networks_.begin(), remembered_wifi_networks_.end(),
        remembered_network);
    if (iter != remembered_wifi_networks_.end())
      remembered_wifi_networks_.erase(iter);
    Network* network = FindNetworkFromRemembered(remembered_network);
    if (network && network->type() == TYPE_WIFI) {
      // Clear the stored credentials for any visible forgotten networks.
      WifiNetwork* wifi = static_cast<WifiNetwork*>(network);
      wifi->EraseCredentials();
    } else {
      // Network is not in visible list.
      VLOG(2) << "Remembered Network not found: "
              << remembered_network->unique_id();
    }
    delete remembered_network;
  }

  // Update all network lists, and request associated service updates.
  void UpdateNetworkServiceList(const ListValue* services) {
    // TODO(stevenjb): Wait for wifi_scanning_ to be false.
    // Copy the list of existing networks to "old" and clear the map and lists.
    NetworkMap old_network_map = network_map_;
    ClearNetworks(false /*don't delete*/);
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
        RequestNetworkServiceInfo(service_path.c_str(),
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
  }

  // Request updates for watched networks. Does not affect network lists.
  // Existing networks will be updated. There should not be any new networks
  // in this list, but if there are they will be added appropriately.
  void UpdateWatchedNetworkServiceList(const ListValue* services) {
    for (ListValue::const_iterator iter = services->begin();
         iter != services->end(); ++iter) {
      std::string service_path;
      (*iter)->GetAsString(&service_path);
      if (!service_path.empty()) {
        VLOG(1) << "Watched Service: " << service_path;
        RequestNetworkServiceInfo(
            service_path.c_str(), &NetworkServiceUpdate, this);
      }
    }
  }

  // Request the active profile which lists the remembered networks.
  void RequestRememberedNetworksUpdate() {
    if (!active_profile_path_.empty()) {
      RequestNetworkProfile(
          active_profile_path_.c_str(), &ProfileUpdate, this);
    }
  }

  // Update the list of remembered (profile) networks, and request associated
  // service updates.
  void UpdateRememberedServiceList(const char* profile_path,
                                   const ListValue* profile_entries) {
    // Copy the list of existing networks to "old" and clear the map and list.
    NetworkMap old_network_map = remembered_network_map_;
    ClearRememberedNetworks(false /*don't delete*/);
    // |profile_entries| represents a complete list of remembered networks.
    for (ListValue::const_iterator iter = profile_entries->begin();
         iter != profile_entries->end(); ++iter) {
      std::string service_path;
      (*iter)->GetAsString(&service_path);
      if (!service_path.empty()) {
        // If we find the network in "old", add it immediately to the map
        // and list. Otherwise it will get added when
        // RememberedNetworkServiceUpdate calls ParseRememberedNetwork.
        NetworkMap::iterator found = old_network_map.find(service_path);
        if (found != old_network_map.end()) {
          Network* network = found->second;
          if (network->type() == TYPE_WIFI) {
            WifiNetwork* wifi = static_cast<WifiNetwork*>(network);
            AddRememberedWifiNetwork(wifi);
            old_network_map.erase(found);
          }
        }
        // Always request updates for remembered networks.
        RequestNetworkProfileEntry(profile_path,
                                   service_path.c_str(),
                                   &RememberedNetworkServiceUpdate,
                                   this);
      }
    }
    // Delete any old networks that no longer exist.
    for (NetworkMap::iterator iter = old_network_map.begin();
         iter != old_network_map.end(); ++iter) {
      delete iter->second;
    }
  }

  Network* CreateNewNetwork(ConnectionType type,
                            const std::string& service_path) {
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

  Network* ParseNetwork(const std::string& service_path,
                        const DictionaryValue* info) {
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

    UpdateActiveNetwork(network);

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
            << " Path: " << network->service_path();
    NotifyNetworkManagerChanged(false);  // Not forced.
    return network;
  }

  // Returns NULL if |service_path| refers to a network that is not a
  // remembered type.
  Network* ParseRememberedNetwork(const std::string& service_path,
                                  const DictionaryValue* info) {
    Network* network;
    NetworkMap::iterator found = remembered_network_map_.find(service_path);
    if (found != remembered_network_map_.end()) {
      network = found->second;
    } else {
      ConnectionType type = ParseTypeFromDictionary(info);
      if (type == TYPE_WIFI) {
        network = CreateNewNetwork(type, service_path);
        WifiNetwork* wifi = static_cast<WifiNetwork*>(network);
        AddRememberedWifiNetwork(wifi);
      } else {
        VLOG(1) << "Ignoring remembered network: " << service_path
                << " Type: " << ConnectionTypeToString(type);
        return NULL;
      }
    }
    network->ParseInfo(info);  // virtual.
    VLOG(1) << "ParseRememberedNetwork: " << network->name()
            << " Path: " << network->service_path();
    NotifyNetworkManagerChanged(false);  // Not forced.
    return network;
  }

  void ClearNetworks(bool delete_networks) {
    if (delete_networks)
      STLDeleteValues(&network_map_);
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

  void ClearRememberedNetworks(bool delete_networks) {
    if (delete_networks)
      STLDeleteValues(&remembered_network_map_);
    remembered_network_map_.clear();
    remembered_wifi_networks_.clear();
  }

  ////////////////////////////////////////////////////////////////////////////
  // NetworkDevice list management functions.

  // Returns pointer to device or NULL if device is not found by path.
  // Use FindNetworkDeviceByPath when you're not intending to change device.
  NetworkDevice* GetNetworkDeviceByPath(const std::string& path) {
    NetworkDeviceMap::iterator iter = device_map_.find(path);
    if (iter != device_map_.end())
      return iter->second;
    LOG(WARNING) << "Device path not found: " << path;
    return NULL;
  }

  // Update device list, and request associated device updates.
  // |devices| represents a complete list of devices.
  void UpdateNetworkDeviceList(const ListValue* devices) {
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
        RequestNetworkDeviceInfo(
            device_path.c_str(), &NetworkDeviceUpdate, this);
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

  void DeleteDeviceFromDeviceObserversMap(const std::string& device_path) {
    // Delete all device observers associated with this device.
    NetworkDeviceObserverMap::iterator map_iter =
        network_device_observers_.find(device_path);
    if (map_iter != network_device_observers_.end()) {
      delete map_iter->second;
      network_device_observers_.erase(map_iter);
    }
  }

  void DeleteDevice(const std::string& device_path) {
    NetworkDeviceMap::iterator found = device_map_.find(device_path);
    if (found == device_map_.end()) {
      LOG(WARNING) << "Attempt to delete non-existant device: "
                   << device_path;
      return;
    }
    VLOG(2) << " Deleting device: " << device_path;
    NetworkDevice* device = found->second;
    device_map_.erase(found);
    DeleteDeviceFromDeviceObserversMap(device_path);
    delete device;
  }

  void ParseNetworkDevice(const std::string& device_path,
                          const DictionaryValue* info) {
    NetworkDeviceMap::iterator found = device_map_.find(device_path);
    NetworkDevice* device;
    if (found != device_map_.end()) {
      device = found->second;
    } else {
      device = new NetworkDevice(device_path);
      VLOG(2) << " Adding device: " << device_path;
      device_map_[device_path] = device;
      if (network_device_observers_.find(device_path) ==
          network_device_observers_.end()) {
        network_device_observers_[device_path] =
            new NetworkDeviceObserverList(this, device_path);
      }
    }
    device->ParseInfo(info);
    VLOG(1) << "ParseNetworkDevice:" << device->name();
    NotifyNetworkManagerChanged(false);  // Not forced.
  }

  ////////////////////////////////////////////////////////////////////////////

  void EnableNetworkDeviceType(ConnectionType device, bool enable) {
    if (!EnsureCrosLoaded())
      return;

    // If network device is already enabled/disabled, then don't do anything.
    if (enable && (enabled_devices_ & (1 << device))) {
      LOG(WARNING) << "Trying to enable a device that's already enabled: "
                   << device;
      return;
    }
    if (!enable && !(enabled_devices_ & (1 << device))) {
      LOG(WARNING) << "Trying to disable a device that's already disabled: "
                   << device;
      return;
    }

    RequestNetworkDeviceEnable(ConnectionTypeToString(device), enable);
  }

  ////////////////////////////////////////////////////////////////////////////
  // Notifications.

  // We call this any time something in NetworkLibrary changes.
  // TODO(stevenjb): We should consider breaking this into multiple
  // notifications, e.g. connection state, devices, services, etc.
  void NotifyNetworkManagerChanged(bool force_update) {
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
          this, &NetworkLibraryImpl::SignalNetworkManagerObservers);
      BrowserThread::PostDelayedTask(BrowserThread::UI, FROM_HERE, notify_task_,
                                     kNetworkNotifyDelayMs);
    }
  }

  void SignalNetworkManagerObservers() {
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

  void NotifyNetworkChanged(Network* network) {
    VLOG(2) << "Network changed: " << network->name();
    DCHECK(network);
    NetworkObserverMap::const_iterator iter = network_observers_.find(
        network->service_path());
    if (iter != network_observers_.end()) {
      FOR_EACH_OBSERVER(NetworkObserver,
                        *(iter->second),
                        OnNetworkChanged(this, network));
    } else {
      NOTREACHED() <<
          "There weren't supposed to be any property change observers of " <<
           network->service_path();
    }
  }

  void NotifyNetworkDeviceChanged(NetworkDevice* device) {
    DCHECK(device);
    NetworkDeviceObserverMap::const_iterator iter =
        network_device_observers_.find(device->device_path());
    if (iter != network_device_observers_.end()) {
      NetworkDeviceObserverList* device_observer_list = iter->second;
      FOR_EACH_OBSERVER(NetworkDeviceObserver,
                        *device_observer_list,
                        OnNetworkDeviceChanged(this, device));
    } else {
      NOTREACHED() <<
          "There weren't supposed to be any property change observers of " <<
           device->device_path();
    }
  }

  void NotifyCellularDataPlanChanged() {
    FOR_EACH_OBSERVER(CellularDataPlanObserver,
                      data_plan_observers_,
                      OnCellularDataPlanChanged(this));
  }

  void NotifyPinOperationCompleted(PinOperationError error) {
    FOR_EACH_OBSERVER(PinOperationObserver,
                      pin_operation_observers_,
                      OnPinOperationCompleted(this, error));
    sim_operation_ = SIM_OPERATION_NONE;
  }

  void NotifyUserConnectionInitiated(const Network* network) {
    FOR_EACH_OBSERVER(UserActionObserver,
                      user_action_observers_,
                      OnConnectionInitiated(this, network));
  }

  ////////////////////////////////////////////////////////////////////////////
  // Device updates.

  void FlipSimPinRequiredStateIfNeeded() {
    if (sim_operation_ != SIM_OPERATION_CHANGE_REQUIRE_PIN)
      return;

    const NetworkDevice* cellular = FindCellularDevice();
    if (cellular) {
      NetworkDevice* device = GetNetworkDeviceByPath(cellular->device_path());
      if (device->sim_pin_required() == SIM_PIN_NOT_REQUIRED)
        device->sim_pin_required_ = SIM_PIN_REQUIRED;
      else if (device->sim_pin_required() == SIM_PIN_REQUIRED)
        device->sim_pin_required_ = SIM_PIN_NOT_REQUIRED;
    }
  }

  void UpdateNetworkDeviceStatus(const char* path,
                                 const char* key,
                                 const Value* value) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (key == NULL || value == NULL)
      return;
    NetworkDevice* device = GetNetworkDeviceByPath(path);
    if (device) {
      VLOG(1) << "UpdateNetworkDeviceStatus: " << device->name() << "." << key;
      int index = property_index_parser().Get(std::string(key));
      if (!device->ParseValue(index, value)) {
        LOG(WARNING) << "UpdateNetworkDeviceStatus: Error parsing: "
                     << path << "." << key;
      } else if (strcmp(key, kCellularAllowRoamingProperty) == 0) {
        bool settings_value =
            UserCrosSettingsProvider::cached_data_roaming_enabled();
        if (device->data_roaming_allowed() != settings_value) {
          // Switch back to signed settings value.
          SetCellularDataRoamingAllowed(settings_value);
          return;
        }
      }
      // Notify only observers on device property change.
      NotifyNetworkDeviceChanged(device);
      // If a device's power state changes, new properties may become
      // defined.
      if (strcmp(key, kPoweredProperty) == 0) {
        RequestNetworkDeviceInfo(path, &NetworkDeviceUpdate, this);
      }
    }
  }

  ////////////////////////////////////////////////////////////////////////////
  // Service updates.

  void UpdateNetworkStatus(const char* path,
                           const char* key,
                           const Value* value) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    if (key == NULL || value == NULL)
      return;
    Network* network = FindNetworkByPath(path);
    if (network) {
      VLOG(2) << "UpdateNetworkStatus: " << network->name() << "." << key;
      // Note: ParseValue is virtual.
      int index = property_index_parser().Get(std::string(key));
      if (!network->ParseValue(index, value)) {
        LOG(WARNING) << "UpdateNetworkStatus: Error parsing: "
                     << path << "." << key;
      }
      NotifyNetworkChanged(network);
      // Anything observing the manager needs to know about any service change.
      NotifyNetworkManagerChanged(false);  // Not forced.
    }
  }

  ////////////////////////////////////////////////////////////////////////////
  // Data Plans.

  const CellularDataPlan* GetSignificantDataPlanFromVector(
      const CellularDataPlanVector* plans) const {
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

  CellularNetwork::DataLeft GetDataLeft(
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

  void UpdateCellularDataPlan(const std::string& service_path,
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
    for (size_t i = 0; i < data_plan_list->plans_size; i++) {
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

  ////////////////////////////////////////////////////////////////////////////

  void Init() {
    // First, get the currently available networks. This data is cached
    // on the connman side, so the call should be quick.
    if (EnsureCrosLoaded()) {
      VLOG(1) << "Requesting initial network manager info from libcros.";
      RequestNetworkManagerInfo(&NetworkManagerUpdate, this);
    }
  }

  void InitTestData() {
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
    ClearNetworks(true /*delete networks*/);

    ethernet_ = new EthernetNetwork("eth1");
    ethernet_->set_connected(true);
    AddNetwork(ethernet_);

    WifiNetwork* wifi1 = new WifiNetwork("fw1");
    wifi1->set_name("Fake Wifi Connected");
    wifi1->set_strength(90);
    wifi1->set_connected(true);
    wifi1->set_active(true);
    wifi1->set_encryption(SECURITY_NONE);
    AddNetwork(wifi1);

    WifiNetwork* wifi2 = new WifiNetwork("fw2");
    wifi2->set_name("Fake Wifi");
    wifi2->set_strength(70);
    wifi2->set_connected(false);
    wifi2->set_encryption(SECURITY_NONE);
    AddNetwork(wifi2);

    WifiNetwork* wifi3 = new WifiNetwork("fw3");
    wifi3->set_name("Fake Wifi Encrypted");
    wifi3->set_strength(60);
    wifi3->set_connected(false);
    wifi3->set_encryption(SECURITY_WEP);
    wifi3->set_passphrase_required(true);
    AddNetwork(wifi3);

    WifiNetwork* wifi4 = new WifiNetwork("fw4");
    wifi4->set_name("Fake Wifi 802.1x");
    wifi4->set_strength(50);
    wifi4->set_connected(false);
    wifi4->set_connectable(false);
    wifi4->set_encryption(SECURITY_8021X);
    wifi4->set_identity("nobody@google.com");
    wifi4->set_cert_path("SETTINGS:key_id=3,cert_id=3,pin=111111");
    AddNetwork(wifi4);

    WifiNetwork* wifi5 = new WifiNetwork("fw5");
    wifi5->set_name("Fake Wifi UTF-8 SSID ");
    wifi5->SetSsid("Fake Wifi UTF-8 SSID \u3042\u3044\u3046");
    wifi5->set_strength(25);
    wifi5->set_connected(false);
    AddNetwork(wifi5);

    WifiNetwork* wifi6 = new WifiNetwork("fw6");
    wifi6->set_name("Fake Wifi latin-1 SSID ");
    wifi6->SetSsid("Fake Wifi latin-1 SSID \xc0\xcb\xcc\xd6\xfb");
    wifi6->set_strength(20);
    wifi6->set_connected(false);
    AddNetwork(wifi6);

    active_wifi_ = wifi1;

    CellularNetwork* cellular1 = new CellularNetwork("fc1");
    cellular1->set_name("Fake Cellular");
    cellular1->set_strength(70);
    cellular1->set_connected(true);
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

    // Remembered Networks
    ClearRememberedNetworks(true /*delete networks*/);
    WifiNetwork* remembered_wifi2 = new WifiNetwork("fw2");
    remembered_wifi2->set_name("Fake Wifi 2");
    remembered_wifi2->set_strength(70);
    remembered_wifi2->set_connected(true);
    remembered_wifi2->set_encryption(SECURITY_WEP);
    AddRememberedWifiNetwork(remembered_wifi2);

    // VPNs.
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

    wifi_scanning_ = false;
    offline_mode_ = false;
  }

  // Network manager observer list
  ObserverList<NetworkManagerObserver> network_manager_observers_;

  // Cellular data plan observer list
  ObserverList<CellularDataPlanObserver> data_plan_observers_;

  // PIN operation observer list.
  ObserverList<PinOperationObserver> pin_operation_observers_;

  // User action observer list
  ObserverList<UserActionObserver> user_action_observers_;

  // Network observer map
  NetworkObserverMap network_observers_;

  // Network device observer map.
  NetworkDeviceObserverMap network_device_observers_;

  // For monitoring network manager status changes.
  PropertyChangeMonitor network_manager_monitor_;

  // For monitoring data plan changes to the connected cellular network.
  DataPlanUpdateMonitor data_plan_monitor_;

  // Network login observer.
  scoped_ptr<NetworkLoginObserver> network_login_observer_;

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

  // Currently not implemented. TODO: implement or eliminate.
  bool offline_mode_;

  // True if access network library is locked.
  bool is_locked_;

  // Type of pending SIM operation, SIM_OPERATION_NONE otherwise.
  SimOperationType sim_operation_;

  // Delayed task to notify a network change.
  CancelableTask* notify_task_;

  // Cellular plan payment time.
  base::Time cellular_plan_payment_time_;

  // Temporary connection data for async connect calls.
  struct ConnectData {
    ConnectionSecurity security;
    std::string service_name;  // For example, SSID.
    std::string passphrase;
    EAPMethod eap_method;
    EAPPhase2Auth eap_auth;
    std::string eap_server_ca_cert_nss_nickname;
    bool eap_use_system_cas;
    std::string eap_client_cert_pkcs11_id;
    std::string eap_identity;
    std::string eap_anonymous_identity;
    bool save_credentials;
    std::string psk_key;
    std::string psk_username;
    std::string server_hostname;
  };
  ConnectData connect_data_;

  DISALLOW_COPY_AND_ASSIGN(NetworkLibraryImpl);
};

class NetworkLibraryStubImpl : public NetworkLibrary {
 public:
  NetworkLibraryStubImpl()
      : ip_address_("1.1.1.1"),
        ethernet_(new EthernetNetwork("eth0")),
        active_wifi_(NULL),
        active_cellular_(NULL) {
  }
  ~NetworkLibraryStubImpl() { if (ethernet_) delete ethernet_; }
  virtual void AddNetworkManagerObserver(NetworkManagerObserver* observer) {}
  virtual void RemoveNetworkManagerObserver(NetworkManagerObserver* observer) {}
  virtual void AddNetworkObserver(const std::string& service_path,
                                  NetworkObserver* observer) {}
  virtual void RemoveNetworkObserver(const std::string& service_path,
                                     NetworkObserver* observer) {}
  virtual void RemoveObserverForAllNetworks(NetworkObserver* observer) {}
  virtual void AddNetworkDeviceObserver(const std::string& device_path,
                                        NetworkDeviceObserver* observer) {}
  virtual void RemoveNetworkDeviceObserver(const std::string& device_path,
                                           NetworkDeviceObserver* observer) {}
  virtual void Lock() {}
  virtual void Unlock() {}
  virtual bool IsLocked() { return false; }
  virtual void AddCellularDataPlanObserver(
      CellularDataPlanObserver* observer) {}
  virtual void RemoveCellularDataPlanObserver(
      CellularDataPlanObserver* observer) {}
  virtual void AddPinOperationObserver(PinOperationObserver* observer) {}
  virtual void RemovePinOperationObserver(PinOperationObserver* observer) {}
  virtual void AddUserActionObserver(UserActionObserver* observer) {}
  virtual void RemoveUserActionObserver(UserActionObserver* observer) {}

  virtual const EthernetNetwork* ethernet_network() const {
    return ethernet_;
  }
  virtual bool ethernet_connecting() const { return false; }
  virtual bool ethernet_connected() const { return true; }

  virtual const WifiNetwork* wifi_network() const {
    return active_wifi_;
  }
  virtual bool wifi_connecting() const { return false; }
  virtual bool wifi_connected() const { return false; }

  virtual const CellularNetwork* cellular_network() const {
    return active_cellular_;
  }
  virtual bool cellular_connecting() const { return false; }
  virtual bool cellular_connected() const { return false; }

  virtual const VirtualNetwork* virtual_network() const {
    return active_virtual_;
  }
  virtual bool virtual_network_connecting() const { return false; }
  virtual bool virtual_network_connected() const { return false; }

  bool Connected() const { return true; }
  bool Connecting() const { return false; }
  const std::string& IPAddress() const { return ip_address_; }
  virtual const WifiNetworkVector& wifi_networks() const {
    return wifi_networks_;
  }
  virtual const WifiNetworkVector& remembered_wifi_networks() const {
    return wifi_networks_;
  }
  virtual const CellularNetworkVector& cellular_networks() const {
    return cellular_networks_;
  }
  virtual const VirtualNetworkVector& virtual_networks() const {
    return virtual_networks_;
  }
  virtual bool has_cellular_networks() const {
    return cellular_networks_.begin() != cellular_networks_.end();
  }
  /////////////////////////////////////////////////////////////////////////////

  virtual const NetworkDevice* FindNetworkDeviceByPath(
      const std::string& path) const { return NULL; }
  virtual const NetworkDevice* FindCellularDevice() const {
    return NULL;
  }
  virtual const NetworkDevice* FindEthernetDevice() const {
    return NULL;
  }
  virtual const NetworkDevice* FindWifiDevice() const {
    return NULL;
  }
  virtual Network* FindNetworkByPath(
      const std::string& path) const { return NULL; }
  virtual WifiNetwork* FindWifiNetworkByPath(
      const std::string& path) const { return NULL; }
  virtual CellularNetwork* FindCellularNetworkByPath(
      const std::string& path) const { return NULL; }
  virtual VirtualNetwork* FindVirtualNetworkByPath(
      const std::string& path) const { return NULL; }
  virtual Network* FindNetworkFromRemembered(
      const Network* remembered) const { return NULL; }
  virtual const CellularDataPlanVector* GetDataPlans(
      const std::string& path) const { return NULL; }
  virtual const CellularDataPlan* GetSignificantDataPlan(
      const std::string& path) const { return NULL; }

  virtual void ChangePin(const std::string& old_pin,
                         const std::string& new_pin) {}
  virtual void ChangeRequirePin(bool require_pin, const std::string& pin) {}
  virtual void EnterPin(const std::string& pin) {}
  virtual void UnblockPin(const std::string& puk, const std::string& new_pin) {}
  virtual void RequestCellularScan() {}
  virtual void RequestCellularRegister(const std::string& network_id) {}
  virtual void SetCellularDataRoamingAllowed(bool new_value) {}

  virtual void RequestNetworkScan() {}
  virtual bool GetWifiAccessPoints(WifiAccessPointVector* result) {
    return false;
  }

  virtual void ConnectToWifiNetwork(WifiNetwork* network) {}
  virtual void ConnectToWifiNetwork(const std::string& service_path) {}
  virtual void ConnectToWifiNetwork(const std::string& ssid,
                                    ConnectionSecurity security,
                                    const std::string& passphrase) {}
  virtual void ConnectToWifiNetwork8021x(
      const std::string& ssid,
      EAPMethod method,
      EAPPhase2Auth auth,
      const std::string& server_ca_cert_nss_nickname,
      bool use_system_cas,
      const std::string& client_cert_pkcs11_id,
      const std::string& identity,
      const std::string& anonymous_identity,
      const std::string& passphrase,
      bool save_credentials) {}
  virtual void ConnectToCellularNetwork(CellularNetwork* network) {}
  virtual void ConnectToVirtualNetwork(VirtualNetwork* network) {}
  virtual void ConnectToVirtualNetworkPSK(
      const std::string& service_name,
      const std::string& server,
      const std::string& psk,
      const std::string& username,
      const std::string& user_passphrase) {}
  virtual void SignalCellularPlanPayment() {}
  virtual bool HasRecentCellularPlanPayment() { return false; }
  virtual void DisconnectFromNetwork(const Network* network) {}
  virtual void ForgetWifiNetwork(const std::string& service_path) {}
  virtual std::string GetCellularHomeCarrierId() const { return std::string(); }
  virtual bool ethernet_available() const { return true; }
  virtual bool wifi_available() const { return false; }
  virtual bool cellular_available() const { return false; }
  virtual bool ethernet_enabled() const { return true; }
  virtual bool wifi_enabled() const { return false; }
  virtual bool cellular_enabled() const { return false; }
  virtual bool wifi_scanning() const { return false; }
  virtual const Network* active_network() const { return NULL; }
  virtual const Network* connected_network() const { return NULL; }
  virtual bool offline_mode() const { return false; }
  virtual void EnableEthernetNetworkDevice(bool enable) {}
  virtual void EnableWifiNetworkDevice(bool enable) {}
  virtual void EnableCellularNetworkDevice(bool enable) {}
  virtual void EnableOfflineMode(bool enable) {}
  virtual NetworkIPConfigVector GetIPConfigs(const std::string& device_path,
                                             std::string* hardware_address,
                                             HardwareAddressFormat) {
    hardware_address->clear();
    return NetworkIPConfigVector();
  }

 private:
  std::string ip_address_;
  EthernetNetwork* ethernet_;
  WifiNetwork* active_wifi_;
  CellularNetwork* active_cellular_;
  VirtualNetwork* active_virtual_;
  WifiNetworkVector wifi_networks_;
  CellularNetworkVector cellular_networks_;
  VirtualNetworkVector virtual_networks_;
};

// static
NetworkLibrary* NetworkLibrary::GetImpl(bool stub) {
  if (stub)
    return new NetworkLibraryStubImpl();
  else
    return new NetworkLibraryImpl();
}

}  // namespace chromeos

// Allows InvokeLater without adding refcounting. This class is a Singleton and
// won't be deleted until it's last InvokeLater is run.
DISABLE_RUNNABLE_METHOD_REFCOUNT(chromeos::NetworkLibraryImpl);
