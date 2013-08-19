// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/network/managed_network_configuration_handler.h"

#include "base/logging.h"
#include "base/values.h"
#include "chromeos/network/network_ui_data.h"
#include "chromeos/network/onc/onc_utils.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace chromeos {

ManagedNetworkConfigurationHandler::~ManagedNetworkConfigurationHandler() {}

// static
scoped_ptr<NetworkUIData> ManagedNetworkConfigurationHandler::GetUIData(
    const base::DictionaryValue& shill_dictionary) {
  std::string ui_data_blob;
  if (shill_dictionary.GetStringWithoutPathExpansion(flimflam::kUIDataProperty,
                                                     &ui_data_blob) &&
      !ui_data_blob.empty()) {
    scoped_ptr<base::DictionaryValue> ui_data_dict =
        onc::ReadDictionaryFromJson(ui_data_blob);
    if (ui_data_dict)
      return make_scoped_ptr(new NetworkUIData(*ui_data_dict));
    else
      LOG(ERROR) << "UIData is not a valid JSON dictionary.";
  }
  VLOG(2) << "JSON dictionary has no UIData blob: " << shill_dictionary;
  return scoped_ptr<NetworkUIData>();
}

}  // namespace chromeos
