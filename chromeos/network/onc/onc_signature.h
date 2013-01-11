// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_ONC_ONC_SIGNATURE_H_
#define CHROMEOS_NETWORK_ONC_ONC_SIGNATURE_H_

#include <string>

#include "base/values.h"
#include "chromeos/chromeos_export.h"

namespace chromeos {
namespace onc {

struct OncValueSignature;

struct OncFieldSignature {
  const char* onc_field_name;
  const char* shill_property_name;
  const OncValueSignature* value_signature;
};

struct CHROMEOS_EXPORT OncValueSignature {
  base::Value::Type onc_type;
  const OncFieldSignature* fields;
  const OncValueSignature* onc_array_entry_signature;
};

CHROMEOS_EXPORT const OncFieldSignature* GetFieldSignature(
    const OncValueSignature& signature,
    const std::string& onc_field_name);

CHROMEOS_EXPORT extern const OncValueSignature kRecommendedSignature;
CHROMEOS_EXPORT extern const OncValueSignature kEAPSignature;
CHROMEOS_EXPORT extern const OncValueSignature kIssuerSubjectPatternSignature;
CHROMEOS_EXPORT extern const OncValueSignature kCertificatePatternSignature;
CHROMEOS_EXPORT extern const OncValueSignature kIPsecSignature;
CHROMEOS_EXPORT extern const OncValueSignature kL2TPSignature;
CHROMEOS_EXPORT extern const OncValueSignature kOpenVPNSignature;
CHROMEOS_EXPORT extern const OncValueSignature kVPNSignature;
CHROMEOS_EXPORT extern const OncValueSignature kEthernetSignature;
CHROMEOS_EXPORT extern const OncValueSignature kIPConfigSignature;
CHROMEOS_EXPORT extern const OncValueSignature kProxyLocationSignature;
CHROMEOS_EXPORT extern const OncValueSignature kProxyManualSignature;
CHROMEOS_EXPORT extern const OncValueSignature kProxySettingsSignature;
CHROMEOS_EXPORT extern const OncValueSignature kWiFiSignature;
CHROMEOS_EXPORT extern const OncValueSignature kCertificateSignature;
CHROMEOS_EXPORT extern const OncValueSignature kNetworkConfigurationSignature;
CHROMEOS_EXPORT extern const OncValueSignature kCertificateListSignature;
CHROMEOS_EXPORT extern const OncValueSignature
    kNetworkConfigurationListSignature;
CHROMEOS_EXPORT extern const OncValueSignature kToplevelConfigurationSignature;

}  // namespace onc
}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_ONC_ONC_SIGNATURE_H_
