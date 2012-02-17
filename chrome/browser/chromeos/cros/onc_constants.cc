// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/onc_constants.h"

namespace chromeos {

// Constants for ONC properties.
namespace onc {

const char kEthernet[] = "Ethernet";
const char kGUID[] = "GUID";
const char kName[] = "Name";
const char kNetworkConfiguration[] = "NetworkConfiguration";
const char kProxySettings[] = "ProxySettings";
const char kRecommended[] = "Recommended";
const char kRemove[] = "Remove";
const char kType[] = "Type";
const char kVPN[] = "VPN";
const char kWiFi[] = "WiFi";

namespace ethernet {
const char kAuthentication[] = "Authentication";
const char kEAP[] = "EAP";
}  // namespace ethernet

namespace wifi {
const char kAutoConnect[] = "AutoConnect";
const char kEAP[] = "EAP";
const char kHiddenSSID[] = "HiddenSSID";
const char kPassphrase[] = "Passphrase";
const char kProxyURL[] = "ProxyURL";
const char kSSID[] = "SSID";
const char kSecurity[] = "Security";
}  // namespace wifi

namespace certificate {
const char kCommonName[] = "CommonName";
const char kEmailAddress[] = "EmailAddress";
const char kEnrollmentURI[] = "EnrollmentURI";
const char kIssuer[] = "Issuer";
const char kIssuerCARef[] = "IssuerCARef";
const char kLocality[] = "Locality";
const char kNone[] = "None";
const char kOrganization[] = "Organization";
const char kOrganizationalUnit[] = "OrganizationalUnit";
const char kPattern[] = "Pattern";
const char kRef[] = "Ref";
const char kSubject[] = "Subject";
}  // namespace certificate

namespace eap {
const char kAnonymousIdentity[] = "AnonymousIdentity";
const char kClientCertPattern[] = "ClientCertPattern";
const char kClientCertRef[] = "ClientCertRef";
const char kClientCertType[] = "ClientCertType";
const char kIdentity[] = "Identity";
const char kInner[] = "Inner";
const char kOuter[] = "Outer";
const char kPassword[] = "Password";
const char kSaveCredentials[] = "SaveCredentials";
const char kServerCARef[] = "ServerCARef";
const char kUseSystemCAs[] = "UseSystemCAs";
}  // namespace eap

namespace vpn {
const char kAuthNoCache[] = "AuthNoCache";
const char kAuthRetry[] = "AuthRetry";
const char kAuth[] = "Auth";
const char kAuthenticationType[] = "AuthenticationType";
const char kCipher[] = "Cipher";
const char kClientCertPattern[] = "ClientCertPattern";
const char kClientCertRef[] = "ClientCertRef";
const char kClientCertType[] = "ClientCertType";
const char kCompLZO[] = "CompLZO";
const char kCompNoAdapt[] = "CompNoAdapt";
const char kGroup[] = "Group";
const char kHost[] = "Host";
const char kIKEVersion[] = "IKEVersion";
const char kIPsec[] = "IPsec";
const char kKeyDirection[] = "KeyDirection";
const char kL2TP[] = "L2TP";
const char kNsCertType[] = "NsCertType";
const char kOpenVPN[] = "OpenVPN";
const char kPSK[] = "PSK";
const char kPassword[] = "Password";
const char kPort[] = "Port";
const char kProto[] = "Proto";
const char kPushPeerInfo[] = "PushPeerInfo";
const char kRemoteCertEKU[] = "RemoteCertEKU";
const char kRemoteCertKU[] = "RemoteCertKU";
const char kRemoteCertTLS[] = "RemoteCertTLS";
const char kRenegSec[] = "RenegSec";
const char kSaveCredentials[] = "SaveCredentials";
const char kServerCARef[] = "ServerCARef";
const char kServerCertRef[] = "ServerCertRef";
const char kServerPollTimeout[] = "ServerPollTimeout";
const char kShaper[] = "Shaper";
const char kStaticChallenge[] = "StaticChallenge";
const char kTLSAuthContents[] = "TLSAuthContents";
const char kTLSRemote[] = "TLSRemote";
const char kType[] = "Type";
const char kUsername[] = "Username";
}  // namespace vpn

namespace proxy {
const char kDirect[] = "Direct";
const char kExcludeDomains[] = "ExcludeDomains";
const char kFtp[] = "FTPProxy";
const char kHost[] = "Host";
const char kHttp[] = "HTTPProxy";
const char kHttps[] = "SecureHTTPProxy";
const char kManual[] = "Manual";
const char kPAC[] = "PAC";
const char kPort[] = "Port";
const char kSocks[] = "SOCKS";
const char kType[] = "Type";
const char kWPAD[] = "WPAD";
}  // namespace proxy

namespace substitutes {
const char kLoginIDField[] = "${LOGIN_ID}";
const char kEmailField[] = "${LOGIN_EMAIL}";
}  // namespace substitutes

}  // namespace onc

}  // namespace chromeos
