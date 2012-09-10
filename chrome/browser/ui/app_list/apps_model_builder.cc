// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/apps_model_builder.h"

#include "base/i18n/case_conversion.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/extension_app_item.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/notification_service.h"
#include "ui/base/l10n/l10n_util_collator.h"

using extensions::Extension;

namespace {

// TODO(benwells): Get the list of special apps from the controller.
const char* kSpecialApps[] = {
  extension_misc::kChromeAppId,
  extension_misc::kWebStoreAppId,
};

// ModelItemSortData provides a string key to sort with
// l10n_util::StringComparator.
struct ModelItemSortData {
  explicit ModelItemSortData(app_list::AppListItemModel* app)
      : app(app),
        key(base::i18n::ToLower(UTF8ToUTF16(app->title()))) {
  }

  // Needed by StringComparator<Element> in SortVectorWithStringKey, which uses
  // this method to get a key to sort.
  const string16& GetStringKey() const {
    return key;
  }

  app_list::AppListItemModel* app;
  string16 key;
};

// Returns true if given |extension_id| is listed in kSpecialApps.
bool IsSpecialApp(const std::string& extension_id) {
  for (size_t i = 0; i < arraysize(kSpecialApps); ++i) {
    if (extension_id == kSpecialApps[i])
      return true;
  }

  return false;
}

}  // namespace

AppsModelBuilder::AppsModelBuilder(Profile* profile,
                                   app_list::AppListModel::Apps* model,
                                   AppListController* controller)
    : profile_(profile),
      controller_(controller),
      model_(model),
      special_apps_count_(0) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
      content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
      content::Source<Profile>(profile_));
  registrar_.Add(this, chrome::NOTIFICATION_APP_INSTALLED_TO_APPLIST,
      content::Source<Profile>(profile_));
}

AppsModelBuilder::~AppsModelBuilder() {
}

void AppsModelBuilder::Build() {
  DCHECK(model_ && model_->item_count() == 0);
  CreateSpecialApps();

  Apps apps;
  GetExtensionApps(&apps);

  SortAndPopulateModel(apps);
  HighlightApp();
}

void AppsModelBuilder::SortAndPopulateModel(const Apps& apps) {
  // Just return if there is nothing to populate.
  if (apps.empty())
    return;

  // Sort apps case insensitive alphabetically.
  std::vector<ModelItemSortData> sorted;
  for (Apps::const_iterator it = apps.begin(); it != apps.end(); ++it)
    sorted.push_back(ModelItemSortData(*it));

  l10n_util::SortVectorWithStringKey(g_browser_process->GetApplicationLocale(),
                                     &sorted,
                                     false /* needs_stable_sort */);
  for (std::vector<ModelItemSortData>::const_iterator it = sorted.begin();
       it != sorted.end();
       ++it) {
    model_->Add(it->app);
  }
}

void AppsModelBuilder::InsertItemByTitle(app_list::AppListItemModel* app) {
  DCHECK(model_);

  icu::Locale locale(g_browser_process->GetApplicationLocale().c_str());
  UErrorCode error = U_ZERO_ERROR;
  scoped_ptr<icu::Collator> collator(
      icu::Collator::createInstance(locale, error));
  if (U_FAILURE(error))
    collator.reset();

  l10n_util::StringComparator<string16> c(collator.get());
  ModelItemSortData data(app);
  for (size_t i = special_apps_count_; i < model_->item_count(); ++i) {
    ModelItemSortData current(model_->GetItemAt(i));
    if (!c(current.key, data.key)) {
      model_->AddAt(i, app);
      return;
    }
  }
  model_->Add(app);
}

void AppsModelBuilder::GetExtensionApps(Apps* apps) {
  DCHECK(profile_);
  ExtensionService* service = profile_->GetExtensionService();
  if (!service)
    return;

  // Get extension apps.
  const ExtensionSet* extensions = service->extensions();
  for (ExtensionSet::const_iterator app = extensions->begin();
       app != extensions->end(); ++app) {
    if ((*app)->ShouldDisplayInLauncher() &&
        !IsSpecialApp((*app)->id())) {
      apps->push_back(new ExtensionAppItem(profile_, *app, controller_));
    }
  }

#if defined(OS_CHROMEOS)
  // Explicitly add Talk extension if it's installed and enabled.
  // Add it here instead of in CreateSpecialApps() so it sorts naturally.
  // Prefer debug > alpha > beta > production version.
  const char* kTalkIds[] = {
      extension_misc::kTalkDebugExtensionId,
      extension_misc::kTalkAlphaExtensionId,
      extension_misc::kTalkBetaExtensionId,
      extension_misc::kTalkExtensionId,
  };
  for (size_t i = 0; i < arraysize(kTalkIds); ++i) {
    const Extension* talk = service->GetExtensionById(
        kTalkIds[i], false /*include_disabled*/);
    if (talk) {
      apps->push_back(new ExtensionAppItem(profile_, talk, controller_));
      break;
    }
  }
#endif  // OS_CHROMEOS
}

void AppsModelBuilder::CreateSpecialApps() {
  DCHECK(model_ && model_->item_count() == 0);

  bool is_guest_session = Profile::IsGuestSession();
  ExtensionService* service = profile_->GetExtensionService();
  DCHECK(service);
  for (size_t i = 0; i < arraysize(kSpecialApps); ++i) {
    const std::string extension_id(kSpecialApps[i]);
    if (is_guest_session && extension_id == extension_misc::kWebStoreAppId)
      continue;

    const Extension* extension = service->GetInstalledExtension(extension_id);
    if (!extension)
      continue;

    model_->Add(new ExtensionAppItem(profile_, extension, controller_));
  }

  special_apps_count_ = model_->item_count();
}

int AppsModelBuilder::FindApp(const std::string& app_id) {
  DCHECK(model_);
  for (size_t i = special_apps_count_; i < model_->item_count(); ++i) {
    ChromeAppListItem* app =
        static_cast<ChromeAppListItem*>(model_->GetItemAt(i));
    if (app->type() != ChromeAppListItem::TYPE_APP)
      continue;

    ExtensionAppItem* extension_item = static_cast<ExtensionAppItem*>(app);
    if (extension_item->extension_id() == app_id)
      return i;
  }

  return -1;
}

void AppsModelBuilder::HighlightApp() {
  DCHECK(model_);
  if (highlight_app_id_.empty())
    return;

  int index = FindApp(highlight_app_id_);
  if (index == -1)
    return;

  model_->GetItemAt(index)->SetHighlighted(true);
  highlight_app_id_.clear();
}

void AppsModelBuilder::Observe(int type,
                               const content::NotificationSource& source,
                               const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_EXTENSION_LOADED: {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      if (!extension->ShouldDisplayInLauncher())
        return;

      if (FindApp(extension->id()) != -1)
        return;

      InsertItemByTitle(new ExtensionAppItem(profile_, extension, controller_));
      HighlightApp();
      break;
    }
    case chrome::NOTIFICATION_EXTENSION_UNLOADED: {
      const Extension* extension =
          content::Details<extensions::UnloadedExtensionInfo>(
              details)->extension;
      int index = FindApp(extension->id());
      if (index >= 0)
        model_->DeleteAt(index);
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
