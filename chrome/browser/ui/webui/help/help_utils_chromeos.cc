// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/help/help_utils_chromeos.h"

#include <stddef.h>

#include <algorithm>

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_type_pattern.h"
#include "chromeos/settings/cros_settings_names.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace help_utils_chromeos {

bool IsUpdateOverCellularAllowed(bool interactive) {
  // If this is a Cellular First device or the user actively checks for update,
  // the default is to allow updates over cellular.
  bool default_update_over_cellular_allowed =
      interactive ? true : chromeos::switches::IsCellularFirstDevice();

  // Device Policy overrides the defaults.
  chromeos::CrosSettings* settings = chromeos::CrosSettings::Get();
  if (!settings)
    return default_update_over_cellular_allowed;

  const base::Value* raw_types_value =
      settings->GetPref(chromeos::kAllowedConnectionTypesForUpdate);
  if (!raw_types_value)
    return default_update_over_cellular_allowed;
  const base::ListValue* types_value;
  CHECK(raw_types_value->GetAsList(&types_value));
  for (size_t i = 0; i < types_value->GetSize(); ++i) {
    int connection_type;
    if (!types_value->GetInteger(i, &connection_type)) {
      LOG(WARNING) << "Can't parse connection type #" << i;
      continue;
    }
    if (connection_type == 4)
      return true;
  }
  // Device policy does not allow updates over cellular, as cellular is not
  // included in allowed connection types for updates.
  return false;
}

base::string16 GetConnectionTypeAsUTF16(const chromeos::NetworkState* network) {
  const std::string type =
      network->IsUsingMobileData() ? shill::kTypeCellular : network->type();
  if (chromeos::NetworkTypePattern::Ethernet().MatchesType(type))
    return l10n_util::GetStringUTF16(IDS_NETWORK_TYPE_ETHERNET);
  if (type == shill::kTypeWifi)
    return l10n_util::GetStringUTF16(IDS_NETWORK_TYPE_WIFI);
  if (type == shill::kTypeWimax)
    return l10n_util::GetStringUTF16(IDS_NETWORK_TYPE_WIMAX);
  if (type == shill::kTypeBluetooth)
    return l10n_util::GetStringUTF16(IDS_NETWORK_TYPE_BLUETOOTH);
  if (type == shill::kTypeCellular ||
      chromeos::NetworkTypePattern::Tether().MatchesType(type)) {
    return l10n_util::GetStringUTF16(IDS_NETWORK_TYPE_MOBILE_DATA);
  }
  if (type == shill::kTypeVPN)
    return l10n_util::GetStringUTF16(IDS_NETWORK_TYPE_VPN);
  NOTREACHED();
  return base::string16();
}

}  // namespace help_utils_chromeos
