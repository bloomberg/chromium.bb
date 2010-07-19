// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/clear_browser_data_handler.h"

#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "base/values.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

ClearBrowserDataHandler::ClearBrowserDataHandler() {
}

ClearBrowserDataHandler::~ClearBrowserDataHandler() {
}

void ClearBrowserDataHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  localized_strings->SetString(L"clearBrowsingDataTitle",
      l10n_util::GetString(IDS_CLEAR_BROWSING_DATA_TITLE));
}

