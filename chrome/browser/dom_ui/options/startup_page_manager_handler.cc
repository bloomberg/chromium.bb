// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/options/startup_page_manager_handler.h"

#include "base/values.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"

StartupPageManagerHandler::StartupPageManagerHandler() {
}

StartupPageManagerHandler::~StartupPageManagerHandler() {
}

void StartupPageManagerHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  localized_strings->SetString("startupAddButton",
      l10n_util::GetStringUTF16(IDS_OPTIONS_STARTUP_ADD_BUTTON));
  localized_strings->SetString("startupPageManagerPage",
      l10n_util::GetStringUTF16(IDS_STARTUP_PAGE_SUBPAGE_TITLE));
  localized_strings->SetString("startupManagePages",
      l10n_util::GetStringUTF16(IDS_OPTIONS_STARTUP_MANAGE_BUTTON));
}
