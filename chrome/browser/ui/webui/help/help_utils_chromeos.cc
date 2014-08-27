// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/help/help_utils_chromeos.h"

#include <algorithm>

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/network/network_type_pattern.h"
#include "chromeos/settings/cros_settings_names.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const bool kDefaultUpdateOverCellularAllowed = false;

}  // namespace

namespace help_utils_chromeos {

bool IsUpdateOverCellularAllowed() {
  chromeos::CrosSettings* settings = chromeos::CrosSettings::Get();
  if (!settings)
    return kDefaultUpdateOverCellularAllowed;

  const base::Value* raw_types_value =
      settings->GetPref(chromeos::kAllowedConnectionTypesForUpdate);
  if (!raw_types_value)
    return kDefaultUpdateOverCellularAllowed;
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
  return kDefaultUpdateOverCellularAllowed;
}

base::string16 GetConnectionTypeAsUTF16(const std::string& type) {
  if (chromeos::NetworkTypePattern::Ethernet().MatchesType(type))
    return l10n_util::GetStringUTF16(IDS_NETWORK_TYPE_ETHERNET);
  if (type == shill::kTypeWifi)
    return l10n_util::GetStringUTF16(IDS_NETWORK_TYPE_WIFI);
  if (type == shill::kTypeWimax)
    return l10n_util::GetStringUTF16(IDS_NETWORK_TYPE_WIMAX);
  if (type == shill::kTypeBluetooth)
    return l10n_util::GetStringUTF16(IDS_NETWORK_TYPE_BLUETOOTH);
  if (type == shill::kTypeCellular)
    return l10n_util::GetStringUTF16(IDS_NETWORK_TYPE_CELLULAR);
  if (type == shill::kTypeVPN)
    return l10n_util::GetStringUTF16(IDS_NETWORK_TYPE_VPN);
  NOTREACHED();
  return base::string16();
}

}  // namespace help_utils_chromeos
