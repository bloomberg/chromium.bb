// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROMEOS_NETWORK_ONC_ONC_CONSTANTS_H_
#define CHROMEOS_NETWORK_ONC_ONC_CONSTANTS_H_

#include "chromeos/chromeos_export.h"

namespace chromeos {

// Constants for ONC properties.
namespace onc {

// Indicates from which source an ONC blob comes from.
enum ONCSource {
  ONC_SOURCE_NONE,
  ONC_SOURCE_USER_IMPORT,
  ONC_SOURCE_DEVICE_POLICY,
  ONC_SOURCE_USER_POLICY,
};

// This is no ONC key or value but used for logging only.
// TODO(pneubeck): Remove.
CHROMEOS_EXPORT extern const char kNetworkConfiguration[];

// Common keys/values.
CHROMEOS_EXPORT extern const char kRecommended[];
CHROMEOS_EXPORT extern const char kRemove[];

// Top Level Configuration
namespace toplevel_config {
CHROMEOS_EXPORT extern const char kCertificates[];
CHROMEOS_EXPORT extern const char kEncryptedConfiguration[];
CHROMEOS_EXPORT extern const char kNetworkConfigurations[];
CHROMEOS_EXPORT extern const char kType[];
CHROMEOS_EXPORT extern const char kUnencryptedConfiguration[];
}  // namespace toplevel_config

// NetworkConfiguration.
namespace network_config {
CHROMEOS_EXPORT extern const char kCellular[];
CHROMEOS_EXPORT extern const char kEthernet[];
CHROMEOS_EXPORT extern const char kGUID[];
CHROMEOS_EXPORT extern const char kIPConfigs[];
CHROMEOS_EXPORT extern const char kName[];
CHROMEOS_EXPORT extern const char kNameServers[];
CHROMEOS_EXPORT extern const char kProxySettings[];
CHROMEOS_EXPORT extern const char kSearchDomains[];
CHROMEOS_EXPORT extern const char kServicePath[];
CHROMEOS_EXPORT extern const char kConnectionState[];
CHROMEOS_EXPORT extern const char kType[];
CHROMEOS_EXPORT extern const char kVPN[];
CHROMEOS_EXPORT extern const char kWiFi[];
}  // namespace network_config

namespace network_type {
CHROMEOS_EXPORT extern const char kAllTypes[];
CHROMEOS_EXPORT extern const char kCellular[];
CHROMEOS_EXPORT extern const char kEthernet[];
CHROMEOS_EXPORT extern const char kVPN[];
CHROMEOS_EXPORT extern const char kWiFi[];
}  // namespace network_type

namespace cellular {
CHROMEOS_EXPORT extern const char kActivateOverNonCellularNetwork[];
CHROMEOS_EXPORT extern const char kActivationState[];
CHROMEOS_EXPORT extern const char kAllowRoaming[];
CHROMEOS_EXPORT extern const char kAPN[];
CHROMEOS_EXPORT extern const char kCarrier[];
CHROMEOS_EXPORT extern const char kESN[];
CHROMEOS_EXPORT extern const char kFamily[];
CHROMEOS_EXPORT extern const char kFirmwareRevision[];
CHROMEOS_EXPORT extern const char kFoundNetworks[];
CHROMEOS_EXPORT extern const char kHardwareRevision[];
CHROMEOS_EXPORT extern const char kHomeProvider[];
CHROMEOS_EXPORT extern const char kICCID[];
CHROMEOS_EXPORT extern const char kIMEI[];
CHROMEOS_EXPORT extern const char kIMSI[];
CHROMEOS_EXPORT extern const char kManufacturer[];
CHROMEOS_EXPORT extern const char kMDN[];
CHROMEOS_EXPORT extern const char kMEID[];
CHROMEOS_EXPORT extern const char kMIN[];
CHROMEOS_EXPORT extern const char kModelID[];
CHROMEOS_EXPORT extern const char kNetworkTechnology[];
CHROMEOS_EXPORT extern const char kOperatorCode[];
CHROMEOS_EXPORT extern const char kOperatorName[];
CHROMEOS_EXPORT extern const char kPRLVersion[];
CHROMEOS_EXPORT extern const char kProviderRequiresRoaming[];
CHROMEOS_EXPORT extern const char kRoamingState[];
CHROMEOS_EXPORT extern const char kSelectedNetwork[];
CHROMEOS_EXPORT extern const char kServingOperator[];
CHROMEOS_EXPORT extern const char kSIMLockStatus[];
CHROMEOS_EXPORT extern const char kSIMPresent[];
CHROMEOS_EXPORT extern const char kSupportedCarriers[];
CHROMEOS_EXPORT extern const char kSupportNetworkScan[];
}  // namespace cellular

namespace connection_state {
CHROMEOS_EXPORT extern const char kConnected[];
CHROMEOS_EXPORT extern const char kConnecting[];
CHROMEOS_EXPORT extern const char kNotConnected[];
}  // namespace connection_state

namespace ipconfig {
CHROMEOS_EXPORT extern const char kGateway[];
CHROMEOS_EXPORT extern const char kIPAddress[];
CHROMEOS_EXPORT extern const char kIPv4[];
CHROMEOS_EXPORT extern const char kIPv6[];
CHROMEOS_EXPORT extern const char kRoutingPrefix[];
CHROMEOS_EXPORT extern const char kType[];
}  // namespace ipconfig

namespace ethernet {
CHROMEOS_EXPORT extern const char kAuthentication[];
CHROMEOS_EXPORT extern const char kEAP[];
CHROMEOS_EXPORT extern const char kNone[];
CHROMEOS_EXPORT extern const char k8021X[];
}  // namespace ethernet

namespace wifi {
CHROMEOS_EXPORT extern const char kAutoConnect[];
CHROMEOS_EXPORT extern const char kBSSID[];
CHROMEOS_EXPORT extern const char kEAP[];
CHROMEOS_EXPORT extern const char kHiddenSSID[];
CHROMEOS_EXPORT extern const char kNone[];
CHROMEOS_EXPORT extern const char kPassphrase[];
CHROMEOS_EXPORT extern const char kProxyURL[];
CHROMEOS_EXPORT extern const char kSecurity[];
CHROMEOS_EXPORT extern const char kSSID[];
CHROMEOS_EXPORT extern const char kWEP_PSK[];
CHROMEOS_EXPORT extern const char kWEP_8021X[];
CHROMEOS_EXPORT extern const char kWPA_PSK[];
CHROMEOS_EXPORT extern const char kWPA_EAP[];
}  // namespace wifi

namespace certificate {
CHROMEOS_EXPORT extern const char kAuthority[];
CHROMEOS_EXPORT extern const char kClient[];
CHROMEOS_EXPORT extern const char kCommonName[];
CHROMEOS_EXPORT extern const char kEmailAddress[];
CHROMEOS_EXPORT extern const char kEnrollmentURI[];
CHROMEOS_EXPORT extern const char kGUID[];
CHROMEOS_EXPORT extern const char kIssuerCARef[];
CHROMEOS_EXPORT extern const char kIssuer[];
CHROMEOS_EXPORT extern const char kLocality[];
CHROMEOS_EXPORT extern const char kNone[];
CHROMEOS_EXPORT extern const char kOrganization[];
CHROMEOS_EXPORT extern const char kOrganizationalUnit[];
CHROMEOS_EXPORT extern const char kPKCS12[];
CHROMEOS_EXPORT extern const char kPattern[];
CHROMEOS_EXPORT extern const char kRef[];
CHROMEOS_EXPORT extern const char kServer[];
CHROMEOS_EXPORT extern const char kSubject[];
CHROMEOS_EXPORT extern const char kTrust[];
CHROMEOS_EXPORT extern const char kType[];
CHROMEOS_EXPORT extern const char kWeb[];
CHROMEOS_EXPORT extern const char kX509[];
}  // namespace certificate

namespace encrypted {
CHROMEOS_EXPORT extern const char kAES256[];
CHROMEOS_EXPORT extern const char kCipher[];
CHROMEOS_EXPORT extern const char kCiphertext[];
CHROMEOS_EXPORT extern const char kHMACMethod[];
CHROMEOS_EXPORT extern const char kHMAC[];
CHROMEOS_EXPORT extern const char kIV[];
CHROMEOS_EXPORT extern const char kIterations[];
CHROMEOS_EXPORT extern const char kPBKDF2[];
CHROMEOS_EXPORT extern const char kSHA1[];
CHROMEOS_EXPORT extern const char kSalt[];
CHROMEOS_EXPORT extern const char kStretch[];
}  // namespace encrypted

namespace eap {
CHROMEOS_EXPORT extern const char kAnonymousIdentity[];
CHROMEOS_EXPORT extern const char kAutomatic[];
CHROMEOS_EXPORT extern const char kClientCertPattern[];
CHROMEOS_EXPORT extern const char kClientCertRef[];
CHROMEOS_EXPORT extern const char kClientCertType[];
CHROMEOS_EXPORT extern const char kEAP_AKA[];
CHROMEOS_EXPORT extern const char kEAP_FAST[];
CHROMEOS_EXPORT extern const char kEAP_SIM[];
CHROMEOS_EXPORT extern const char kEAP_TLS[];
CHROMEOS_EXPORT extern const char kEAP_TTLS[];
CHROMEOS_EXPORT extern const char kIdentity[];
CHROMEOS_EXPORT extern const char kInner[];
CHROMEOS_EXPORT extern const char kLEAP[];
CHROMEOS_EXPORT extern const char kMD5[];
CHROMEOS_EXPORT extern const char kMSCHAPv2[];
CHROMEOS_EXPORT extern const char kOuter[];
CHROMEOS_EXPORT extern const char kPAP[];
CHROMEOS_EXPORT extern const char kPEAP[];
CHROMEOS_EXPORT extern const char kPassword[];
CHROMEOS_EXPORT extern const char kSaveCredentials[];
CHROMEOS_EXPORT extern const char kServerCARef[];
CHROMEOS_EXPORT extern const char kUseSystemCAs[];
}  // namespace eap

namespace vpn {
CHROMEOS_EXPORT extern const char kAuthNoCache[];
CHROMEOS_EXPORT extern const char kAuthRetry[];
CHROMEOS_EXPORT extern const char kAuth[];
CHROMEOS_EXPORT extern const char kAuthenticationType[];
CHROMEOS_EXPORT extern const char kAutoConnect[];
CHROMEOS_EXPORT extern const char kCert[];
CHROMEOS_EXPORT extern const char kCipher[];
CHROMEOS_EXPORT extern const char kClientCertPattern[];
CHROMEOS_EXPORT extern const char kClientCertRef[];
CHROMEOS_EXPORT extern const char kClientCertType[];
CHROMEOS_EXPORT extern const char kCompLZO[];
CHROMEOS_EXPORT extern const char kCompNoAdapt[];
CHROMEOS_EXPORT extern const char kEAP[];
CHROMEOS_EXPORT extern const char kGroup[];
CHROMEOS_EXPORT extern const char kHost[];
CHROMEOS_EXPORT extern const char kIKEVersion[];
CHROMEOS_EXPORT extern const char kIPsec[];
CHROMEOS_EXPORT extern const char kKeyDirection[];
CHROMEOS_EXPORT extern const char kL2TP[];
CHROMEOS_EXPORT extern const char kNsCertType[];
CHROMEOS_EXPORT extern const char kOpenVPN[];
CHROMEOS_EXPORT extern const char kPSK[];
CHROMEOS_EXPORT extern const char kPassword[];
CHROMEOS_EXPORT extern const char kPort[];
CHROMEOS_EXPORT extern const char kProto[];
CHROMEOS_EXPORT extern const char kPushPeerInfo[];
CHROMEOS_EXPORT extern const char kRemoteCertEKU[];
CHROMEOS_EXPORT extern const char kRemoteCertKU[];
CHROMEOS_EXPORT extern const char kRemoteCertTLS[];
CHROMEOS_EXPORT extern const char kRenegSec[];
CHROMEOS_EXPORT extern const char kSaveCredentials[];
CHROMEOS_EXPORT extern const char kServerCARef[];
CHROMEOS_EXPORT extern const char kServerCertRef[];
CHROMEOS_EXPORT extern const char kServerPollTimeout[];
CHROMEOS_EXPORT extern const char kShaper[];
CHROMEOS_EXPORT extern const char kStaticChallenge[];
CHROMEOS_EXPORT extern const char kTLSAuthContents[];
CHROMEOS_EXPORT extern const char kTLSRemote[];
CHROMEOS_EXPORT extern const char kTypeL2TP_IPsec[];
CHROMEOS_EXPORT extern const char kType[];
CHROMEOS_EXPORT extern const char kUsername[];
CHROMEOS_EXPORT extern const char kVerb[];
CHROMEOS_EXPORT extern const char kXAUTH[];
}  // namespace vpn

namespace openvpn {
CHROMEOS_EXPORT extern const char kNone[];
CHROMEOS_EXPORT extern const char kInteract[];
CHROMEOS_EXPORT extern const char kNoInteract[];
CHROMEOS_EXPORT extern const char kServer[];
}  // namespace openvpn

namespace substitutes {
CHROMEOS_EXPORT extern const char kEmailField[];
CHROMEOS_EXPORT extern const char kLoginIDField[];
}  // namespace substitutes

namespace proxy {
CHROMEOS_EXPORT extern const char kDirect[];
CHROMEOS_EXPORT extern const char kExcludeDomains[];
CHROMEOS_EXPORT extern const char kFtp[];
CHROMEOS_EXPORT extern const char kHost[];
CHROMEOS_EXPORT extern const char kHttp[];
CHROMEOS_EXPORT extern const char kHttps[];
CHROMEOS_EXPORT extern const char kManual[];
CHROMEOS_EXPORT extern const char kPAC[];
CHROMEOS_EXPORT extern const char kPort[];
CHROMEOS_EXPORT extern const char kSocks[];
CHROMEOS_EXPORT extern const char kType[];
CHROMEOS_EXPORT extern const char kWPAD[];
}  // namespace proxy

}  // namespace onc

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_ONC_ONC_CONSTANTS_H_
