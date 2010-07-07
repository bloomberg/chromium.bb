// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/browser_options_handler.h"

#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "base/values.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

BrowserOptionsHandler::BrowserOptionsHandler() {
}

BrowserOptionsHandler::~BrowserOptionsHandler() {
}

void BrowserOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  localized_strings->SetString(L"startupGroupName",
      l10n_util::GetString(IDS_OPTIONS_STARTUP_GROUP_NAME));
  localized_strings->SetString(L"startupShowDefaultAndNewTab",
      l10n_util::GetString(IDS_OPTIONS_STARTUP_SHOW_DEFAULT_AND_NEWTAB));
  localized_strings->SetString(L"startupShowLastSession",
      l10n_util::GetString(IDS_OPTIONS_STARTUP_SHOW_LAST_SESSION));
  localized_strings->SetString(L"startupShowPages",
      l10n_util::GetString(IDS_OPTIONS_STARTUP_SHOW_PAGES));
  localized_strings->SetString(L"startupAddButton",
      l10n_util::GetString(IDS_OPTIONS_STARTUP_ADD_BUTTON));
  localized_strings->SetString(L"startupRemoveButton",
      l10n_util::GetString(IDS_OPTIONS_STARTUP_REMOVE_BUTTON));
  localized_strings->SetString(L"startupUseCurrent",
      l10n_util::GetString(IDS_OPTIONS_STARTUP_USE_CURRENT));
  localized_strings->SetString(L"homepageGroupName",
      l10n_util::GetString(IDS_OPTIONS_HOMEPAGE_GROUP_NAME));
  localized_strings->SetString(L"homepageUseNewTab",
      l10n_util::GetString(IDS_OPTIONS_HOMEPAGE_USE_NEWTAB));
  localized_strings->SetString(L"homepageUseURL",
      l10n_util::GetString(IDS_OPTIONS_HOMEPAGE_USE_URL));
  localized_strings->SetString(L"homepageShowButton",
      l10n_util::GetString(IDS_OPTIONS_HOMEPAGE_SHOW_BUTTON));
#if defined(OS_MACOSX)
  localized_strings->SetString(L"toolbarGroupName",
      l10n_util::GetString(IDS_OPTIONS_TOOLBAR_GROUP_NAME));
  localized_strings->SetString(L"pageOptionShowButton",
      l10n_util::GetString(IDS_OPTIONS_PAGE_OPTION_SHOW_BUTTON));
#endif
  localized_strings->SetString(L"defaultSearchGroupName",
      l10n_util::GetString(IDS_OPTIONS_DEFAULTSEARCH_GROUP_NAME));
  localized_strings->SetString(L"defaultSearchManageEnginesLink",
      l10n_util::GetString(IDS_OPTIONS_DEFAULTSEARCH_MANAGE_ENGINES_LINK));
  localized_strings->SetString(L"defaultBrowserGroupName",
      l10n_util::GetString(IDS_OPTIONS_DEFAULTBROWSER_GROUP_NAME));
  localized_strings->SetString(L"defaultBrowserNotDefault",
      l10n_util::GetStringF(IDS_OPTIONS_DEFAULTBROWSER_NOTDEFAULT,
          l10n_util::GetString(IDS_PRODUCT_NAME)));
  localized_strings->SetString(L"defaultBrowserUnknown",
      l10n_util::GetStringF(IDS_OPTIONS_DEFAULTBROWSER_UNKNOWN,
          l10n_util::GetString(IDS_PRODUCT_NAME)));
  localized_strings->SetString(L"defaultBrowserSXS",
      l10n_util::GetStringF(IDS_OPTIONS_DEFAULTBROWSER_SXS,
          l10n_util::GetString(IDS_PRODUCT_NAME)));
  localized_strings->SetString(L"defaultBrowserUseAsDefault",
      l10n_util::GetStringF(IDS_OPTIONS_DEFAULTBROWSER_USEASDEFAULT,
          l10n_util::GetString(IDS_PRODUCT_NAME)));
}
