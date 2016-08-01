// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/options_note_handler.h"

#include "ash/common/system/chromeos/palette/palette_utils.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace options {

OptionsNoteHandler::OptionsNoteHandler() {}

OptionsNoteHandler::~OptionsNoteHandler() {}

void OptionsNoteHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  RegisterTitle(localized_strings, "noteOverlay", IDS_OPTIONS_NOTE_TITLE);

  localized_strings->SetString(
      "noteSettingsButtonTitle",
      l10n_util::GetStringUTF16(IDS_OPTIONS_NOTE_TITLE));
  localized_strings->SetString(
      "noteDefaultAppAutoLaunch",
      l10n_util::GetStringUTF16(IDS_SETTINGS_NOTE_DEFAULT_APP_AUTO_LAUNCH));
  localized_strings->SetString(
      "noteFindMoreApps",
      l10n_util::GetStringUTF16(IDS_SETTINGS_NOTE_FIND_MORE_APPS));

  localized_strings->SetBoolean("showNoteSettings", ash::IsPaletteEnabled());
}

void OptionsNoteHandler::InitializePage() {}

void OptionsNoteHandler::RegisterMessages() {}

}  // namespace options
}  // namespace chromeos
