// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/add_startup_page_handler.h"

#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "base/values.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

AddStartupPageHandler::AddStartupPageHandler() {
}

AddStartupPageHandler::~AddStartupPageHandler() {
}

void AddStartupPageHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  localized_strings->SetString(L"addStartupPageTitle",
      l10n_util::GetString(IDS_ASI_ADD_TITLE));
  localized_strings->SetString(L"addStartupPageAddButton",
      l10n_util::GetString(IDS_ASI_ADD));
  localized_strings->SetString(L"addStartupPageCancelButton",
      l10n_util::GetString(IDS_CANCEL));
}

