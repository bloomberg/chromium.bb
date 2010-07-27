// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/search_engine_manager_handler.h"

#include "app/l10n_util.h"
#include "base/callback.h"
#include "base/values.h"
#include "chrome/browser/profile.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"

SearchEngineManagerHandler::SearchEngineManagerHandler() {
}

SearchEngineManagerHandler::~SearchEngineManagerHandler() {
}

void SearchEngineManagerHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  localized_strings->SetString(L"searchEngineManagerPage",
      l10n_util::GetString(IDS_SEARCH_ENGINES_EDITOR_WINDOW_TITLE));
}

void SearchEngineManagerHandler::RegisterMessages() {
}
