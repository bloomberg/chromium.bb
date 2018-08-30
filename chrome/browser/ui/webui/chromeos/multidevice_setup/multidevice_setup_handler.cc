// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/multidevice_setup/multidevice_setup_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/common/webui_url_constants.h"

namespace chromeos {

namespace multidevice_setup {

MultideviceSetupHandler::MultideviceSetupHandler() = default;

MultideviceSetupHandler::~MultideviceSetupHandler() = default;

void MultideviceSetupHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "openMultiDeviceSettings",
      base::BindRepeating(
          &MultideviceSetupHandler::HandleOpenMultiDeviceSettings,
          base::Unretained(this)));
}

void MultideviceSetupHandler::HandleOpenMultiDeviceSettings(
    const base::ListValue* args) {
  DCHECK(args->empty());
  chrome::ShowSettingsSubPageForProfile(Profile::FromWebUI(web_ui()),
                                        chrome::kConnectedDevicesSubPage);
}

}  // namespace multidevice_setup

}  // namespace chromeos
