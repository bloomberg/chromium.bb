// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/app_list/app_list_model_builder.h"

#include "base/i18n/case_conversion.h"
#include "base/i18n/string_search.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/ash/app_list/extension_app_item.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_collator.h"

namespace {

// ModelItemSortData provides a string key to sort with
// l10n_util::StringComparator.
struct ModelItemSortData {
  explicit ModelItemSortData(ash::AppListItemModel* item)
      : item(item),
        key(base::i18n::ToLower(UTF8ToUTF16(item->title()))) {
  }

  // Needed by StringComparator<Element> in SortVectorWithStringKey, which uses
  // this method to get a key to sort.
  const string16& GetStringKey() const {
    return key;
  }

  ash::AppListItemModel* item;
  string16 key;
};

// Returns true if given |str| matches |query|. Currently, it returns
// if |query| is a substr of |str|.
bool MatchesQuery(const string16& query, const string16& str) {
  return query.empty() ||
      base::i18n::StringSearchIgnoringCaseAndAccents(query, str);
}

}  // namespace

AppListModelBuilder::AppListModelBuilder(Profile* profile)
    : profile_(profile),
      model_(NULL) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
      content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
      content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_APP_INSTALLED_TO_APPLIST,
      content::Source<Profile>(profile_));
}

AppListModelBuilder::~AppListModelBuilder() {
}

void AppListModelBuilder::SetModel(ash::AppListModel* model) {
  model_ = model;
}

void AppListModelBuilder::Build(const std::string& query) {
  DCHECK(model_ && model_->item_count() == 0);
  query_ = base::i18n::ToLower(UTF8ToUTF16(query));

  Items items;
  GetExtensionApps(query_, &items);

  SortAndPopulateModel(items);
  HighlightApp();
}

void AppListModelBuilder::SortAndPopulateModel(const Items& items) {
  // Sort items case insensitive alphabetically.
  std::vector<ModelItemSortData> sorted;
  for (Items::const_iterator it = items.begin(); it != items.end(); ++it)
    sorted.push_back(ModelItemSortData(*it));

  l10n_util::SortVectorWithStringKey(g_browser_process->GetApplicationLocale(),
                                     &sorted,
                                     false /* needs_stable_sort */);
  for (std::vector<ModelItemSortData>::const_iterator it = sorted.begin();
       it != sorted.end();
       ++it) {
    model_->AddItem(it->item);
  }
}

void AppListModelBuilder::InsertItemByTitle(ash::AppListItemModel* item) {
  DCHECK(model_);

  icu::Locale locale(g_browser_process->GetApplicationLocale().c_str());
  UErrorCode error = U_ZERO_ERROR;
  scoped_ptr<icu::Collator> collator(
      icu::Collator::createInstance(locale, error));
  if (U_FAILURE(error))
    collator.reset();

  l10n_util::StringComparator<string16> c(collator.get());
  ModelItemSortData data(item);
  for (int i = 0; i < model_->item_count(); ++i) {
    ModelItemSortData current(model_->GetItem(i));
    if (!c(current.key, data.key)) {
      model_->AddItemAt(i, item);
      return;
    }
  }
  model_->AddItem(item);
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
        MatchesQuery(query, UTF8ToUTF16((*app)->name()))) {
      items->push_back(new ExtensionAppItem(profile_, *app));
    }
  }
}

int AppListModelBuilder::FindApp(const std::string& app_id) {
  DCHECK(model_);
  for (int i = 0; i < model_->item_count(); ++i) {
    ExtensionAppItem* extension_item =
        static_cast<ExtensionAppItem*>(model_->GetItem(i));
    if (extension_item->extension_id() == app_id)
      return i;
  }

  return -1;
}

void AppListModelBuilder::HighlightApp() {
  DCHECK(model_);
  if (highlight_app_id_.empty())
    return;

  int index = FindApp(highlight_app_id_);
  if (index == -1)
    return;

  model_->GetItem(index)->SetHighlighted(true);
  highlight_app_id_.clear();
}

void AppListModelBuilder::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      if (!extension->ShouldDisplayInLauncher() ||
          !MatchesQuery(query_, UTF8ToUTF16(extension->name())))
        return;

      if (FindApp(extension->id()) != -1)
        return;

      InsertItemByTitle(new ExtensionAppItem(profile_, extension));
      HighlightApp();
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      int index = FindApp(extension->id());
      if (index >= 0)
        model_->DeleteItemAt(index);
      break;
    }
    case chrome::NOTIFICATION_APP_INSTALLED_TO_APPLIST: {
      highlight_app_id_ = *content::Details<const std::string>(details).ptr();
      HighlightApp();
      break;
    }
    default:
      NOTREACHED();
  }
}
