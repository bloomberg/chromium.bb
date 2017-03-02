// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/network_element_localized_strings_provider.h"

#include "build/build_config.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {
namespace network_element {

void AddLocalizedStrings(content::WebUIDataSource* html_source) {
  struct {
    const char* name;
    int id;
  } localized_strings[] = {
      {"OncTypeCellular", IDS_NETWORK_TYPE_CELLULAR},
      {"OncTypeEthernet", IDS_NETWORK_TYPE_ETHERNET},
      {"OncTypeVPN", IDS_NETWORK_TYPE_VPN},
      {"OncTypeWiFi", IDS_NETWORK_TYPE_WIFI},
      {"OncTypeWiMAX", IDS_NETWORK_TYPE_WIMAX},
      {"networkDisabled", IDS_SETTINGS_DEVICE_OFF},
      {"networkListItemConnected", IDS_STATUSBAR_NETWORK_DEVICE_CONNECTED},
      {"networkListItemConnecting", IDS_STATUSBAR_NETWORK_DEVICE_CONNECTING},
      {"networkListItemConnectingTo", IDS_NETWORK_LIST_CONNECTING_TO},
      {"networkListItemNotConnected", IDS_NETWORK_LIST_NOT_CONNECTED},
      {"vpnNameTemplate", IDS_NETWORK_LIST_THIRD_PARTY_VPN_NAME_TEMPLATE},
  };
  for (const auto& entry : localized_strings)
    html_source->AddLocalizedString(entry.name, entry.id);
}

}  // namespace network_element
}  // namespace chromeos
