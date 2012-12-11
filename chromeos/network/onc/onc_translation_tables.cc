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

}  // namespace onc
}  // namespace chromeos
