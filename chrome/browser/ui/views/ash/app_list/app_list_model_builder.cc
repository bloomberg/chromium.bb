// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/app_list/app_list_model_builder.h"

#include "base/string_util.h"
#include "base/i18n/case_conversion.h"
#include "base/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/ash/app_list/browser_command_item.h"
#include "chrome/browser/ui/views/ash/app_list/extension_app_item.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Data struct used to define BrowserCommand.
struct BrowserCommandData {
  int command_id;
  int title_id;
  int icon_id;
};

// Binary predict to sort app list items alphabetically.
bool AppPrecedes(const ash::AppListItemModel* a,
                 const ash::AppListItemModel* b) {
  return a->title() < b->title();
}

// Returns true if given |str| matches |query|. Currently, it returns
// if |query| is a substr of |str|.
bool MatchesQuery(const string16& query, const string16& str) {
  return query.empty() || str.find(query) != string16::npos;
}

}  // namespace

AppListModelBuilder::AppListModelBuilder(Profile* profile,
                                         ash::AppListModel* model)
    : profile_(profile),
      model_(model) {
}

AppListModelBuilder::~AppListModelBuilder() {
}

void AppListModelBuilder::Build(const std::string& query) {
  string16 normalized_query(base::i18n::ToLower(UTF8ToUTF16(query)));

  Items items;
  GetExtensionApps(normalized_query, &items);
  GetBrowserCommands(normalized_query, &items);

  std::sort(items.begin(), items.end(), &AppPrecedes);
  for (Items::const_iterator it = items.begin(); it != items.end(); ++it) {
    model_->AddItem(*it);
  }
}

void AppListModelBuilder::GetExtensionApps(const string16& query,
                                           Items* items) {
  DCHECK(profile_);
  ExtensionService* service = profile_->GetExtensionService();
  if (!service)
    return;

  // Get extension apps.
  const ExtensionSet* extensions = service->extensions();
  for (ExtensionSet::const_iterator app = extensions->begin();
       app != extensions->end(); ++app) {
    if ((*app)->ShouldDisplayInLauncher() &&
        MatchesQuery(query,
                     base::i18n::ToLower(UTF8ToUTF16((*app)->name())))) {
      items->push_back(new ExtensionAppItem(profile_, *app));
    }
  }
}

void AppListModelBuilder::GetBrowserCommands(const string16& query,
                                             Items* items) {
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
  if (!browser)
    return;

  const BrowserCommandData kBrowserCommands[] = {
    {
      IDC_NEW_INCOGNITO_WINDOW,
      IDS_APP_LIST_INCOGNITO,
      IDR_APP_LIST_INCOGNITO,
    },
    {
      IDC_OPTIONS,
      IDS_APP_LIST_SETTINGS,
      IDR_APP_LIST_SETTINGS,

    },
  };

  for (size_t i = 0; i < arraysize(kBrowserCommands); ++i) {
    string16 title = l10n_util::GetStringUTF16(kBrowserCommands[i].title_id);
    if (MatchesQuery(query, base::i18n::ToLower(title))) {
      items->push_back(new BrowserCommandItem(browser,
                                              kBrowserCommands[i].command_id,
                                              kBrowserCommands[i].title_id,
                                              kBrowserCommands[i].icon_id));
    }
  }
}
