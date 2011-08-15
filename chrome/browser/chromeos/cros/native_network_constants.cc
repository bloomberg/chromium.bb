// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/native_network_constants.h"

namespace chromeos {
// Format of the Carrier ID: <carrier name> (<carrier country>).
const char kCarrierIdFormat[] = "%s (%s)";

// Path of the default (shared) flimflam profile.
const char kSharedProfilePath[] = "/profile/default";

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
const char kPortalUrlProperty[] = "PortalURL";
const char kCheckPortalListProperty[] = "CheckPortalList";
const char kArpGatewayProperty[] = "ArpGateway";

// Flimflam service properties.
const char kSecurityProperty[] = "Security";
const char kPassphraseProperty[] = "Passphrase";
const char kIdentityProperty[] = "Identity";
const char kPassphraseRequiredProperty[] = "PassphraseRequired";
const char kSaveCredentialsProperty[] = "SaveCredentials";
const char kSignalStrengthProperty[] = "Strength";
const char kNameProperty[] = "Name";
const char kGuidProperty[] = "GUID";
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
const char kPaymentUrlProperty[] = "Cellular.OlpUrl";
const char kUsageUrlProperty[] = "Cellular.UsageUrl";
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
const char kSimLockStatusProperty[] = "Cellular.SIMLockStatus";
const char kSimLockTypeProperty[] = "LockType";
const char kSimLockRetriesLeftProperty[] = "RetriesLeft";

// Flimflam property names for Cellular.FoundNetworks.
const char kLongNameProperty[] = "long_name";
const char kStatusProperty[] = "status";
const char kShortNameProperty[] = "short_name";
const char kTechnologyProperty[] = "technology";
const char kNetworkIdProperty[] = "network_id";

// Flimflam SIMLock status types.
const char kSimLockPin[] = "sim-pin";
const char kSimLockPuk[] = "sim-puk";

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
const char kModelIdProperty[] = "Cellular.ModelID";
const char kManufacturerProperty[] = "Cellular.Manufacturer";
const char kFirmwareRevisionProperty[] = "Cellular.FirmwareRevision";
const char kHardwareRevisionProperty[] = "Cellular.HardwareRevision";
const char kPrlVersionProperty[] = "Cellular.PRLVersion"; // (INT16)
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
const char kTypeVpn[] = "vpn";

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
const char kL2tpIpsecCaCertNssProperty[] = "L2TPIPsec.CACertNSS";
const char kL2tpIpsecClientCertIdProperty[] = "L2TPIPsec.ClientCertID";
const char kL2tpIpsecClientCertSlotProp[] = "L2TPIPsec.ClientCertSlot";
const char kL2tpIpsecPinProperty[] = "L2TPIPsec.PIN";
const char kL2tpIpsecPskProperty[] = "L2TPIPsec.PSK";
const char kL2tpIpsecUserProperty[] = "L2TPIPsec.User";
const char kL2tpIpsecPasswordProperty[] = "L2TPIPsec.Password";

// Flimflam EAP property names.
// See src/third_party/flimflam/doc/service-api.txt.
const char kEapIdentityProperty[] = "EAP.Identity";
const char kEapMethodProperty[] = "EAP.EAP";
const char kEapPhase2AuthProperty[] = "EAP.InnerEAP";
const char kEapAnonymousIdentityProperty[] = "EAP.AnonymousIdentity";
const char kEapClientCertProperty[] = "EAP.ClientCert";  // path
const char kEapCertIdProperty[] = "EAP.CertID";  // PKCS#11 ID
const char kEapClientCertNssProperty[] = "EAP.ClientCertNSS";  // NSS nickname
const char kEapPrivateKeyProperty[] = "EAP.PrivateKey";
const char kEapPrivateKeyPasswordProperty[] = "EAP.PrivateKeyPassword";
const char kEapKeyIdProperty[] = "EAP.KeyID";
const char kEapCaCertProperty[] = "EAP.CACert";  // server CA cert path
const char kEapCaCertIdProperty[] = "EAP.CACertID";  // server CA PKCS#11 ID
const char kEapCaCertNssProperty[] = "EAP.CACertNSS";  // server CA NSS nickname
const char kEapUseSystemCasProperty[] = "EAP.UseSystemCAs";
const char kEapPinProperty[] = "EAP.PIN";
const char kEapPasswordProperty[] = "EAP.Password";
const char kEapKeyMgmtProperty[] = "EAP.KeyMgmt";

// Flimflam EAP method options.
const char kEapMethodPeap[] = "PEAP";
const char kEapMethodTls[] = "TLS";
const char kEapMethodTtls[] = "TTLS";
const char kEapMethodLeap[] = "LEAP";

// Flimflam EAP phase 2 auth options.
const char kEapPhase2AuthPeapMd5[] = "auth=MD5";
const char kEapPhase2AuthPeapMschap2[] = "auth=MSCHAPV2";
const char kEapPhase2AuthTtlsMd5[] = "autheap=MD5";
const char kEapPhase2AuthTtlsMschapV2[] = "autheap=MSCHAPV2";
const char kEapPhase2AuthTtlsMschap[] = "autheap=MSCHAP";
const char kEapPhase2AuthTtlsPap[] = "autheap=PAP";
const char kEapPhase2AuthTtlsChap[] = "autheap=CHAP";

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
const char kErrorBadWepKey[] = "bad-wepkey";
const char kErrorActivationFailed[] = "activation-failed";
const char kErrorNeedEvdo[] = "need-evdo";
const char kErrorNeedHomeNetwork[] = "need-home-network";
const char kErrorOtaspFailed[] = "otasp-failed";
const char kErrorAaaFailed[] = "aaa-failed";
const char kErrorInternal[] = "internal-error";
const char kErrorDnsLookupFailed[] = "dns-lookup-failed";
const char kErrorHttpGetFailed[] = "http-get-failed";

// Flimflam error messages.
const char kErrorPassphraseRequiredMsg[] = "Passphrase required";
const char kErrorIncorrectPinMsg[] = "org.chromium.flimflam.Error.IncorrectPin";
const char kErrorPinBlockedMsg[] = "org.chromium.flimflam.Error.PinBlocked";
const char kErrorPinRequiredMsg[] = "org.chromium.flimflam.Error.PinRequired";

const char kUnknownString[] = "UNKNOWN";

////////////////////////////////////////////////////////////////////////////

const char* ConnectionTypeToString(ConnectionType type) {
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
      return kTypeVpn;
  }
  LOG(ERROR) << "ConnectionTypeToString called with unknown type: " << type;
  return kUnknownString;
}

const char* SecurityToString(ConnectionSecurity security) {
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

const char* ProviderTypeToString(ProviderType type) {
  switch (type) {
    case PROVIDER_TYPE_L2TP_IPSEC_PSK:
    case PROVIDER_TYPE_L2TP_IPSEC_USER_CERT:
      return kProviderL2tpIpsec;
    case PROVIDER_TYPE_OPEN_VPN:
      return kProviderOpenVpn;
    case PROVIDER_TYPE_MAX:
      break;
  }
  LOG(ERROR) << "ProviderTypeToString called with unknown type: " << type;
  return kUnknownString;
}

}  // namespace chromeos
