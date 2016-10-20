// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/options_stylus_handler.h"

#include "ash/common/system/chromeos/palette/palette_utils.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace options {

OptionsStylusHandler::OptionsStylusHandler() {}

OptionsStylusHandler::~OptionsStylusHandler() {}

void OptionsStylusHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  RegisterTitle(localized_strings, "stylusOverlay", IDS_SETTINGS_STYLUS_TITLE);

  localized_strings->SetString(
      "stylusSettingsButtonTitle",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_DEVICE_GROUP_STYLUS_SETTINGS_BUTTON));
  localized_strings->SetString(
      "stylusAutoOpenStylusTools",
      l10n_util::GetStringUTF16(IDS_SETTINGS_STYLUS_AUTO_OPEN_STYLUS_TOOLS));
  localized_strings->SetString(
      "stylusEnableStylusTools",
      l10n_util::GetStringUTF16(IDS_SETTINGS_STYLUS_ENABLE_STYLUS_TOOLS));
  localized_strings->SetString(
      "stylusFindMoreApps",
      l10n_util::GetStringUTF16(IDS_SETTINGS_STYLUS_FIND_MORE_APPS));

  localized_strings->SetBoolean("showStylusSettings",
                                ash::IsPaletteFeatureEnabled());
}

void OptionsStylusHandler::InitializePage() {}

void OptionsStylusHandler::RegisterMessages() {}

}  // namespace options
}  // namespace chromeos
