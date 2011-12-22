// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/aura/app_list/app_list_model_builder.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/aura/app_list/browser_command_item.h"
#include "chrome/browser/ui/views/aura/app_list/extension_app_item.h"
#include "chrome/browser/ui/webui/ntp/app_launcher_handler.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/aura_shell/app_list/app_list_item_group_model.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Gets or creates group for given |extension| in |model|. The created group
// is added to |model| and owned by it.
aura_shell::AppListItemGroupModel* GetOrCreateGroup(
    int page_index,
    const ListValue* app_page_names,
    aura_shell::AppListModel* model) {
  if (page_index >= model->group_count()) {
    for (int i = model->group_count(); i <= page_index; ++i) {
      std::string title;
      if (!app_page_names->GetString(i, &title))
        title = l10n_util::GetStringUTF8(IDS_APP_DEFAULT_PAGE_NAME);

      aura_shell::AppListItemGroupModel* group =
          new aura_shell::AppListItemGroupModel(title);
      model->AddGroup(group);
    }
  }

  return model->GetGroup(page_index);
}

// Binary predict to sort extension apps. Smaller launch ordinal takes
// precedence.
bool ExtensionAppPrecedes(const ExtensionAppItem* a,
                          const ExtensionAppItem* b) {
  return a->launch_ordinal().LessThan(b->launch_ordinal());
}

}  // namespace

AppListModelBuilder::AppListModelBuilder(Profile* profile,
                                         aura_shell::AppListModel* model)
    : profile_(profile),
      model_(model) {
}

AppListModelBuilder::~AppListModelBuilder() {
}

void AppListModelBuilder::Build() {
  GetExtensionApps();
  GetBrowserCommands();
}

void AppListModelBuilder::GetExtensionApps() {
  DCHECK(profile_);
  ExtensionService* service = profile_->GetExtensionService();
  if (!service)
    return;

  // Get extension apps.
  std::vector<ExtensionAppItem*> items;
  const ExtensionSet* extensions = service->extensions();
  for (ExtensionSet::const_iterator app = extensions->begin();
       app != extensions->end(); ++app) {
    if (AppLauncherHandler::IsAppExcludedFromList(*app))
      continue;

    items.push_back(new ExtensionAppItem(profile_, *app));
  }

  // Sort by launch ordinal.
  std::sort(items.begin(), items.end(), &ExtensionAppPrecedes);

  // Put all items into model and group them by page ordinal.
  PrefService* prefs = profile_->GetPrefs();
  const ListValue* app_page_names = prefs->GetList(prefs::kNTPAppPageNames);
  for (size_t i = 0; i < items.size(); ++i) {
    ExtensionAppItem* item = items[i];

    aura_shell::AppListItemGroupModel* group = GetOrCreateGroup(
        item->page_index(),
        app_page_names,
        model_);

    group->AddItem(item);
  }
}

void AppListModelBuilder::GetBrowserCommands() {
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
  if (!browser)
    return;

  // Uses the first group to put browser commands
  if (model_->group_count() == 0)
    model_->AddGroup(new aura_shell::AppListItemGroupModel(""));
  aura_shell::AppListItemGroupModel* group = model_->GetGroup(0);

  group->AddItem(new BrowserCommandItem(browser,
                                        IDC_NEW_INCOGNITO_WINDOW,
                                        IDS_APP_LIST_INCOGNITO,
                                        IDR_APP_LIST_INCOGNITO));
  group->AddItem(new BrowserCommandItem(browser,
                                        IDC_OPTIONS,
                                        IDS_APP_LIST_SETTINGS,
                                        IDR_APP_LIST_SETTINGS));
}

