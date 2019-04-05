// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/cellular_setup/cellular_setup_dialog_launcher.h"

#include "chrome/browser/ui/webui/chromeos/cellular_setup/mobile_setup_dialog.h"

namespace chromeos {

namespace cellular_setup {

void OpenCellularSetupDialog(const std::string& cellular_network_guid) {
  // TODO(khorimoto): Open a different dialog depending on the state of
  // kUpdatedCellularActivationUi.
  MobileSetupDialog::ShowByNetworkId(cellular_network_guid);
}

}  // namespace cellular_setup

}  // namespace chromeos
