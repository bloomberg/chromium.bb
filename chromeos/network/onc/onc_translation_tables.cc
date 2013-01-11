// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/onc/onc_translation_tables.h"

#include <cstddef>

#include "chromeos/network/onc/onc_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {
namespace onc {

const StringTranslationEntry kNetworkTypeTable[] = {
  { kEthernet, flimflam::kTypeEthernet },
  { kWiFi, flimflam::kTypeWifi },
  { kCellular, flimflam::kTypeCellular },
  { kVPN, flimflam::kTypeVPN },
  { NULL }
};

const StringTranslationEntry kVPNTypeTable[] = {
  { vpn::kTypeL2TP_IPsec, flimflam::kProviderL2tpIpsec },
  { vpn::kOpenVPN, flimflam::kProviderOpenVpn },
  { NULL }
};

const StringTranslationEntry kWiFiSecurityTable[] = {
  { wifi::kNone, flimflam::kSecurityNone },
  { wifi::kWEP_PSK, flimflam::kSecurityWep },
  { wifi::kWPA_PSK, flimflam::kSecurityPsk },
  { wifi::kWPA_EAP, flimflam::kSecurity8021x },
  { NULL }
};

const StringTranslationEntry kEAPOuterTable[] = {
  { eap::kPEAP, flimflam::kEapMethodPEAP },
  { eap::kEAP_TLS, flimflam::kEapMethodTLS },
  { eap::kEAP_TTLS, flimflam::kEapMethodTTLS },
  { eap::kLEAP, flimflam::kEapMethodLEAP },
  { NULL }
};

// Translation of the EAP.Inner field in case of EAP.Outer == PEAP
const StringTranslationEntry kEAP_PEAP_InnerTable[] = {
  { eap::kMD5, flimflam::kEapPhase2AuthPEAPMD5 },
  { eap::kMSCHAPv2, flimflam::kEapPhase2AuthPEAPMSCHAPV2 },
  { NULL }
};

// Translation of the EAP.Inner field in case of EAP.Outer == TTLS
const StringTranslationEntry kEAP_TTLS_InnerTable[] = {
  { eap::kMD5, flimflam::kEapPhase2AuthTTLSMD5 },
  { eap::kMSCHAPv2, flimflam::kEapPhase2AuthTTLSMSCHAPV2 },
  { eap::kPAP, flimflam::kEapPhase2AuthTTLSPAP },
  { NULL }
};


}  // namespace onc
}  // namespace chromeos
