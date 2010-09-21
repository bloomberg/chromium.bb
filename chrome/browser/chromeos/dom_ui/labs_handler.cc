// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/labs_handler.h"

#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "base/callback.h"
#include "base/stl_util-inl.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/notification_service.h"
#include "grit/browser_resources.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"

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
}
