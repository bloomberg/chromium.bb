// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_NETWORK_SETTINGS_ONC_SIGNATURE_H_
#define CHROME_BROWSER_CHROMEOS_NETWORK_SETTINGS_ONC_SIGNATURE_H_

#include <string>

#include "base/values.h"

namespace chromeos {
namespace onc {

struct OncValueSignature;

struct OncFieldSignature {
  const char* onc_field_name;
  const char* shill_property_name;
  const OncValueSignature* value_signature;
};

struct OncValueSignature {
  base::Value::Type onc_type;
  const OncFieldSignature* fields;
  const OncValueSignature* onc_array_entry_signature;
};

const OncFieldSignature* GetFieldSignature(const OncValueSignature& signature,
                                           const std::string& onc_field_name);

extern const OncValueSignature kRecommendedSignature;
extern const OncValueSignature kEAPSignature;
extern const OncValueSignature kIssuerSubjectPatternSignature;
extern const OncValueSignature kCertificatePatternSignature;
extern const OncValueSignature kIPsecSignature;
extern const OncValueSignature kL2TPSignature;
extern const OncValueSignature kOpenVPNSignature;
extern const OncValueSignature kVPNSignature;
extern const OncValueSignature kEthernetSignature;
extern const OncValueSignature kIPConfigSignature;
extern const OncValueSignature kProxyLocationSignature;
extern const OncValueSignature kProxyManualSignature;
extern const OncValueSignature kProxySettingsSignature;
extern const OncValueSignature kWiFiSignature;
extern const OncValueSignature kCertificateSignature;
extern const OncValueSignature kNetworkConfigurationSignature;
extern const OncValueSignature kUnencryptedConfigurationSignature;

}  // namespace onc
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NETWORK_SETTINGS_ONC_SIGNATURE_H_
