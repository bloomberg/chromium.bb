// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_CHROMEOS_CROS_ONC_CONSTANTS_H_
#define CHROME_BROWSER_CHROMEOS_CROS_ONC_CONSTANTS_H_

namespace chromeos {

// Constants for ONC properties.
namespace onc {

// Top Level ONC.
extern const char kCertificates[];
extern const char kEncryptedConfiguration[];
extern const char kNetworkConfigurations[];
extern const char kUnencryptedConfiguration[];

// This is no ONC key or value but used for logging only.
// TODO(pneubeck): Remove.
extern const char kNetworkConfiguration[];

// Common keys/values.
extern const char kRecommended[];
extern const char kRemove[];

// NetworkConfiguration.
// TODO(pneubeck): Put into namespace.
extern const char kCellular[];
extern const char kEthernet[];
extern const char kGUID[];
extern const char kIPConfigs[];
extern const char kName[];
extern const char kNameServers[];
extern const char kProxySettings[];
extern const char kSearchDomains[];
extern const char kType[];
extern const char kVPN[];
extern const char kWiFi[];

namespace ipconfig {
extern const char kGateway[];
extern const char kIPAddress[];
extern const char kIPv4[];
extern const char kIPv6[];
extern const char kRoutingPrefix[];
extern const char kType[];
}  // namespace ipconfig

namespace ethernet {
extern const char kAuthentication[];
extern const char kEAP[];
extern const char kNone[];
extern const char k8021X[];
}  // namespace ethernet

namespace wifi {
extern const char kAutoConnect[];
extern const char kEAP[];
extern const char kHiddenSSID[];
extern const char kNone[];
extern const char kPassphrase[];
extern const char kProxyURL[];
extern const char kSecurity[];
extern const char kSSID[];
extern const char kWEP_PSK[];
extern const char kWEP_8021X[];
extern const char kWPA_PSK[];
extern const char kWPA_EAP[];
}  // namespace wifi

namespace certificate {
extern const char kAuthority[];
extern const char kClient[];
extern const char kCommonName[];
extern const char kEmailAddress[];
extern const char kEnrollmentURI[];
extern const char kIssuerCARef[];
extern const char kIssuer[];
extern const char kLocality[];
extern const char kNone[];
extern const char kOrganization[];
extern const char kOrganizationalUnit[];
extern const char kPKCS12[];
extern const char kPattern[];
extern const char kRef[];
extern const char kServer[];
extern const char kSubject[];
extern const char kTrust[];
extern const char kType[];
extern const char kWeb[];
extern const char kX509[];
}  // namespace certificate

namespace encrypted {
extern const char kAES256[];
extern const char kCipher[];
extern const char kCiphertext[];
extern const char kHMACMethod[];
extern const char kHMAC[];
extern const char kIV[];
extern const char kIterations[];
extern const char kPBKDF2[];
extern const char kSHA1[];
extern const char kSalt[];
extern const char kStretch[];
extern const char kType[];
}  // namespace encrypted

namespace eap {
extern const char kAnonymousIdentity[];
extern const char kAutomatic[];
extern const char kClientCertPattern[];
extern const char kClientCertRef[];
extern const char kClientCertType[];
extern const char kEAP_AKA[];
extern const char kEAP_FAST[];
extern const char kEAP_SIM[];
extern const char kEAP_TLS[];
extern const char kEAP_TTLS[];
extern const char kIdentity[];
extern const char kInner[];
extern const char kLEAP[];
extern const char kMD5[];
extern const char kMSCHAPv2[];
extern const char kOuter[];
extern const char kPAP[];
extern const char kPEAP[];
extern const char kPassword[];
extern const char kSaveCredentials[];
extern const char kServerCARef[];
extern const char kUseSystemCAs[];
}  // namespace eap

namespace vpn {
extern const char kAuthNoCache[];
extern const char kAuthRetry[];
extern const char kAuth[];
extern const char kAuthenticationType[];
extern const char kCert[];
extern const char kCipher[];
extern const char kClientCertPattern[];
extern const char kClientCertRef[];
extern const char kClientCertType[];
extern const char kCompLZO[];
extern const char kCompNoAdapt[];
extern const char kEAP[];
extern const char kGroup[];
extern const char kHost[];
extern const char kIKEVersion[];
extern const char kIPsec[];
extern const char kKeyDirection[];
extern const char kL2TP[];
extern const char kNsCertType[];
extern const char kOpenVPN[];
extern const char kPSK[];
extern const char kPassword[];
extern const char kPort[];
extern const char kProto[];
extern const char kPushPeerInfo[];
extern const char kRemoteCertEKU[];
extern const char kRemoteCertKU[];
extern const char kRemoteCertTLS[];
extern const char kRenegSec[];
extern const char kSaveCredentials[];
extern const char kServerCARef[];
extern const char kServerCertRef[];
extern const char kServerPollTimeout[];
extern const char kShaper[];
extern const char kStaticChallenge[];
extern const char kTLSAuthContents[];
extern const char kTLSRemote[];
extern const char kTypeL2TP_IPsec[];
extern const char kType[];
extern const char kUsername[];
extern const char kVerb[];
extern const char kXAUTH[];
}  // namespace vpn

namespace openvpn {
extern const char kNone[];
extern const char kInteract[];
extern const char kNoInteract[];
extern const char kServer[];
}  // namespace openvpn

namespace substitutes {
extern const char kEmailField[];
extern const char kLoginIDField[];
}  // namespace substitutes

namespace proxy {
extern const char kDirect[];
extern const char kExcludeDomains[];
extern const char kFtp[];
extern const char kHost[];
extern const char kHttp[];
extern const char kHttps[];
extern const char kManual[];
extern const char kPAC[];
extern const char kPort[];
extern const char kSocks[];
extern const char kType[];
extern const char kWPAD[];
}  // namespace proxy

}  // namespace onc

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_ONC_CONSTANTS_H_
