// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/cros_options_page_ui_handler.h"

#include "base/values.h"
#include "chrome/browser/chromeos/cros_settings.h"

namespace chromeos {

CrosOptionsPageUIHandler::CrosOptionsPageUIHandler(
    CrosSettingsProvider* provider) : settings_provider_(provider) {
  if (settings_provider_.get())
    CrosSettings::Get()->AddSettingsProvider(settings_provider_.get());
}

CrosOptionsPageUIHandler::~CrosOptionsPageUIHandler() {
  if (settings_provider_.get())
    CrosSettings::Get()->RemoveSettingsProvider(settings_provider_.get());
}

}  // namespace chromeos
