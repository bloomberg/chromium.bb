// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/font_settings_handler.h"

#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "base/values.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

FontSettingsHandler::FontSettingsHandler() {
}

FontSettingsHandler::~FontSettingsHandler() {
}

void FontSettingsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  localized_strings->SetString(L"fontSettingsTitle",
      l10n_util::GetString(IDS_FONT_LANGUAGE_SETTING_FONT_TAB_TITLE));
}

