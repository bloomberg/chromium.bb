// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/labs_handler.h"

#include "app/l10n_util.h"
#include "base/values.h"
#include "grit/generated_resources.h"

LabsHandler::LabsHandler() {
}

LabsHandler::~LabsHandler() {
}

void LabsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  // Labs page - ChromeOS
  localized_strings->SetString("labsPage",
      l10n_util::GetStringUTF16(IDS_OPTIONS_LABS_TAB_LABEL));

  localized_strings->SetString("mediaplayer_title",
     l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_SECTION_TITLE_MEDIAPLAYER));
  localized_strings->SetString("mediaplayer",
     l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_MEDIAPLAYER_DESCRIPTION));

  localized_strings->SetString("advanced_file_title",
     l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_SECTION_TITLE_ADVANCEDFS));
  localized_strings->SetString("advanced_filesystem",
     l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_ADVANCEDFS_DESCRIPTION));

  localized_strings->SetString("talk_title",
     l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_SECTION_TITLE_TALK));
  localized_strings->SetString("talk",
     l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_TALK_DESCRIPTION));

  localized_strings->SetString("side_tabs_title",
     l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_SECTION_TITLE_SIDE_TABS));
  localized_strings->SetString("side_tabs",
     l10n_util::GetStringUTF16(IDS_OPTIONS_SETTINGS_SIDE_TABS_DESCRIPTION));
}
