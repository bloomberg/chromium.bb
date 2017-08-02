// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/network_element_localized_strings_provider.h"

#include "build/build_config.h"
#include "chrome/grit/generated_resources.h"
#include "components/login/localized_values_builder.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {
namespace network_element {

namespace {

struct {
  const char* name;
  int id;
} const localized_strings[] = {
    {"OncTypeCellular", IDS_NETWORK_TYPE_MOBILE_DATA},
    {"OncTypeEthernet", IDS_NETWORK_TYPE_ETHERNET},
    {"OncTypeTether", IDS_NETWORK_TYPE_MOBILE_DATA},
    {"OncTypeVPN", IDS_NETWORK_TYPE_VPN},
    {"OncTypeWiFi", IDS_NETWORK_TYPE_WIFI},
    {"OncTypeWiMAX", IDS_NETWORK_TYPE_WIMAX},
    {"networkListItemConnected", IDS_STATUSBAR_NETWORK_DEVICE_CONNECTED},
    {"networkListItemConnecting", IDS_STATUSBAR_NETWORK_DEVICE_CONNECTING},
    {"networkListItemConnectingTo", IDS_NETWORK_LIST_CONNECTING_TO},
    {"networkListItemNotConnected", IDS_NETWORK_LIST_NOT_CONNECTED},
    {"vpnNameTemplate", IDS_NETWORK_LIST_THIRD_PARTY_VPN_NAME_TEMPLATE},
};
}  //  namespace

void AddLocalizedStrings(content::WebUIDataSource* html_source) {
  for (const auto& entry : localized_strings)
    html_source->AddLocalizedString(entry.name, entry.id);
}

void AddLocalizedValuesToBuilder(::login::LocalizedValuesBuilder* builder) {
  for (const auto& entry : localized_strings)
    builder->Add(entry.name, entry.id);
}

}  // namespace network_element

}  // namespace chromeos
