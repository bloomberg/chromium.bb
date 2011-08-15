// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the command-line switches used by Chrome.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_NATIVE_NETWORK_CONSTANTS_H_
#define CHROME_BROWSER_CHROMEOS_CROS_NATIVE_NETWORK_CONSTANTS_H_
#pragma once

#include "chrome/browser/chromeos/cros/network_library.h"

namespace chromeos {

// Format of the Carrier ID.
extern const char kCarrierIdFormat[];

// Path of the default (shared) flimflam profile.
extern const char kSharedProfilePath[];

// D-Bus interface string constants, used in both the native parsing
// and network library code.

// Flimflam manager properties.
extern const char kAvailableTechnologiesProperty[];
extern const char kEnabledTechnologiesProperty[];
extern const char kConnectedTechnologiesProperty[];
extern const char kDefaultTechnologyProperty[];
extern const char kOfflineModeProperty[];
extern const char kActiveProfileProperty[];
extern const char kProfilesProperty[];
extern const char kServicesProperty[];
extern const char kServiceWatchListProperty[];
extern const char kDevicesProperty[];
extern const char kPortalUrlProperty[];
extern const char kCheckPortalListProperty[];
extern const char kArpGatewayProperty[];

// Flimflam service properties.
extern const char kSecurityProperty[];
extern const char kPassphraseProperty[];
extern const char kIdentityProperty[];
extern const char kPassphraseRequiredProperty[];
extern const char kSaveCredentialsProperty[];
extern const char kSignalStrengthProperty[];
extern const char kNameProperty[];
extern const char kGuidProperty[];
extern const char kStateProperty[];
extern const char kTypeProperty[];
extern const char kDeviceProperty[];
extern const char kProfileProperty[];
extern const char kTechnologyFamilyProperty[];
extern const char kActivationStateProperty[];
extern const char kNetworkTechnologyProperty[];
extern const char kRoamingStateProperty[];
extern const char kOperatorNameProperty[];
extern const char kOperatorCodeProperty[];
extern const char kServingOperatorProperty[];
extern const char kPaymentUrlProperty[];
extern const char kUsageUrlProperty[];
extern const char kCellularApnProperty[];
extern const char kCellularLastGoodApnProperty[];
extern const char kCellularApnListProperty[];
extern const char kWifiHexSsid[];
extern const char kWifiFrequency[];
extern const char kWifiHiddenSsid[];
extern const char kWifiPhyMode[];
extern const char kWifiAuthMode[];
extern const char kFavoriteProperty[];
extern const char kConnectableProperty[];
extern const char kPriorityProperty[];
extern const char kAutoConnectProperty[];
extern const char kIsActiveProperty[];
extern const char kModeProperty[];
extern const char kErrorProperty[];
extern const char kEntriesProperty[];
extern const char kProviderProperty[];
extern const char kHostProperty[];
extern const char kProxyConfigProperty[];

// Flimflam property names for SIMLock status.
extern const char kSimLockStatusProperty[];
extern const char kSimLockTypeProperty[];
extern const char kSimLockRetriesLeftProperty[];

// Flimflam property names for Cellular.FoundNetworks.
extern const char kLongNameProperty[];
extern const char kStatusProperty[];
extern const char kShortNameProperty[];
extern const char kTechnologyProperty[];
extern const char kNetworkIdProperty[];

// Flimflam SIMLock status types.
extern const char kSimLockPin[];
extern const char kSimLockPuk[];

// APN info property names.
extern const char kApnProperty[];
extern const char kApnNetworkIdProperty[];
extern const char kApnUsernameProperty[];
extern const char kApnPasswordProperty[];
extern const char kApnNameProperty[];
extern const char kApnLocalizedNameProperty[];
extern const char kApnLanguageProperty[];

// Operator info property names.
extern const char kOperatorNameKey[];
extern const char kOperatorCodeKey[];
extern const char kOperatorCountryKey[];

// Flimflam device info property names.
extern const char kScanningProperty[];
extern const char kPoweredProperty[];
extern const char kNetworksProperty[];
extern const char kCarrierProperty[];
extern const char kCellularAllowRoamingProperty[];
extern const char kHomeProviderProperty[];
extern const char kMeidProperty[];
extern const char kImeiProperty[];
extern const char kImsiProperty[];
extern const char kEsnProperty[];
extern const char kMdnProperty[];
extern const char kMinProperty[];
extern const char kModelIdProperty[];
extern const char kManufacturerProperty[];
extern const char kFirmwareRevisionProperty[];
extern const char kHardwareRevisionProperty[];
extern const char kPrlVersionProperty[]; // (INT16)
extern const char kSelectedNetworkProperty[];
extern const char kSupportNetworkScanProperty[];
extern const char kFoundNetworksProperty[];

// Flimflam ip config property names.
extern const char kAddressProperty[];
extern const char kPrefixlenProperty[];
extern const char kGatewayProperty[];
extern const char kNameServersProperty[];

// Flimflam type options.
extern const char kTypeEthernet[];
extern const char kTypeWifi[];
extern const char kTypeWimax[];
extern const char kTypeBluetooth[];
extern const char kTypeCellular[];
extern const char kTypeVpn[];

// Flimflam mode options.
extern const char kModeManaged[];
extern const char kModeAdhoc[];

// Flimflam security options.
extern const char kSecurityWpa[];
extern const char kSecurityWep[];
extern const char kSecurityRsn[];
extern const char kSecurity8021x[];
extern const char kSecurityPsk[];
extern const char kSecurityNone[];

// Flimflam L2TPIPsec property names.
extern const char kL2tpIpsecCaCertNssProperty[];
extern const char kL2tpIpsecClientCertIdProperty[];
extern const char kL2tpIpsecClientCertSlotProp[];
extern const char kL2tpIpsecPinProperty[];
extern const char kL2tpIpsecPskProperty[];
extern const char kL2tpIpsecUserProperty[];
extern const char kL2tpIpsecPasswordProperty[];

// Flimflam EAP property names.
// See src/third_party/flimflam/doc/service-api.txt.
extern const char kEapIdentityProperty[];
extern const char kEapMethodProperty[];
extern const char kEapPhase2AuthProperty[];
extern const char kEapAnonymousIdentityProperty[];
extern const char kEapClientCertProperty[];  // path
extern const char kEapCertIdProperty[];  // PKCS#11 ID
extern const char kEapClientCertNssProperty[];  // NSS nickname
extern const char kEapPrivateKeyProperty[];
extern const char kEapPrivateKeyPasswordProperty[];
extern const char kEapKeyIdProperty[];
extern const char kEapCaCertProperty[];  // server CA cert path
extern const char kEapCaCertIdProperty[];  // server CA PKCS#11 ID
extern const char kEapCaCertNssProperty[];  // server CA NSS nickname
extern const char kEapUseSystemCasProperty[];
extern const char kEapPinProperty[];
extern const char kEapPasswordProperty[];
extern const char kEapKeyMgmtProperty[];

// Flimflam EAP method options.
extern const char kEapMethodPeap[];
extern const char kEapMethodTls[];
extern const char kEapMethodTtls[];
extern const char kEapMethodLeap[];

// Flimflam EAP phase 2 auth options.
extern const char kEapPhase2AuthPeapMd5[];
extern const char kEapPhase2AuthPeapMschap2[];
extern const char kEapPhase2AuthTtlsMd5[];
extern const char kEapPhase2AuthTtlsMschapV2[];
extern const char kEapPhase2AuthTtlsMschap[];
extern const char kEapPhase2AuthTtlsPap[];
extern const char kEapPhase2AuthTtlsChap[];

// Flimflam VPN provider types.
extern const char kProviderL2tpIpsec[];
extern const char kProviderOpenVpn[];

// Flimflam state options.
extern const char kStateIdle[];
extern const char kStateCarrier[];
extern const char kStateAssociation[];
extern const char kStateConfiguration[];
extern const char kStateReady[];
extern const char kStatePortal[];
extern const char kStateOnline[];
extern const char kStateDisconnect[];
extern const char kStateFailure[];
extern const char kStateActivationFailure[];

// Flimflam network technology options.
extern const char kNetworkTechnology1Xrtt[];
extern const char kNetworkTechnologyEvdo[];
extern const char kNetworkTechnologyGprs[];
extern const char kNetworkTechnologyEdge[];
extern const char kNetworkTechnologyUmts[];
extern const char kNetworkTechnologyHspa[];
extern const char kNetworkTechnologyHspaPlus[];
extern const char kNetworkTechnologyLte[];
extern const char kNetworkTechnologyLteAdvanced[];
extern const char kNetworkTechnologyGsm[];

// Flimflam roaming state options
extern const char kRoamingStateHome[];
extern const char kRoamingStateRoaming[];
extern const char kRoamingStateUnknown[];

// Flimflam activation state options
extern const char kActivationStateActivated[];
extern const char kActivationStateActivating[];
extern const char kActivationStateNotActivated[];
extern const char kActivationStatePartiallyActivated[];
extern const char kActivationStateUnknown[];

// FlimFlam technology family options
extern const char kTechnologyFamilyCdma[];
extern const char kTechnologyFamilyGsm[];

// Flimflam error options.
extern const char kErrorOutOfRange[];
extern const char kErrorPinMissing[];
extern const char kErrorDhcpFailed[];
extern const char kErrorConnectFailed[];
extern const char kErrorBadPassphrase[];
extern const char kErrorBadWepKey[];
extern const char kErrorActivationFailed[];
extern const char kErrorNeedEvdo[];
extern const char kErrorNeedHomeNetwork[];
extern const char kErrorOtaspFailed[];
extern const char kErrorAaaFailed[];
extern const char kErrorInternal[];
extern const char kErrorDnsLookupFailed[];
extern const char kErrorHttpGetFailed[];

// Flimflam error messages.
extern const char kErrorPassphraseRequiredMsg[];
extern const char kErrorIncorrectPinMsg[];
extern const char kErrorPinBlockedMsg[];
extern const char kErrorPinRequiredMsg[];

extern const char kUnknownString[];

extern const char* ConnectionTypeToString(ConnectionType type);
extern const char* SecurityToString(ConnectionSecurity security);
extern const char* ProviderTypeToString(ProviderType type);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_NATIVE_NETWORK_CONSTANTS_H_
