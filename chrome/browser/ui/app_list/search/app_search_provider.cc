// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/app_search_provider.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/app_result.h"
#include "chrome/browser/ui/app_list/search/tokenized_string.h"
#include "chrome/browser/ui/app_list/search/tokenized_string_match.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"

namespace app_list {

class AppSearchProvider::App {
 public:
  explicit App(const extensions::Extension* extension)
      : app_id_(extension->id()),
        indexed_name_(UTF8ToUTF16(extension->name())) {
  }
  ~App() {}

  const std::string& app_id() const { return app_id_; }
  const TokenizedString& indexed_name() const { return indexed_name_; }

 private:
  const std::string app_id_;
  TokenizedString indexed_name_;

  DISALLOW_COPY_AND_ASSIGN(App);
};

AppSearchProvider::AppSearchProvider(
    Profile* profile,
    AppListControllerDelegate* list_controller)
    : profile_(profile),
      list_controller_(list_controller) {
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
                 content::Source<Profile>(profile_->GetOriginalProfile()));
  registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNINSTALLED,
                 content::Source<Profile>(profile_->GetOriginalProfile()));
  RefreshApps();
}

AppSearchProvider::~AppSearchProvider() {}

void AppSearchProvider::Start(const string16& query) {
  const TokenizedString query_terms(query);

  ClearResults();

  TokenizedStringMatch match;
  for (Apps::const_iterator app_it = apps_.begin();
       app_it != apps_.end();
       ++app_it) {
    if (!match.Calculate(query_terms, (*app_it)->indexed_name()))
      continue;

    scoped_ptr<AppResult> result(
        new AppResult(profile_, (*app_it)->app_id(), list_controller_));
    result->UpdateFromMatch((*app_it)->indexed_name(), match);
    Add(result.PassAs<ChromeSearchResult>());
  }
}

void AppSearchProvider::Stop() {}

void AppSearchProvider::AddApps(const ExtensionSet* extensions,
                                ExtensionService* service) {
  for (ExtensionSet::const_iterator iter = extensions->begin();
       iter != extensions->end(); ++iter) {
    const extensions::Extension* app = iter->get();

    if (!app->ShouldDisplayInAppLauncher())
      continue;

    if (profile_->IsOffTheRecord() &&
        !service->CanLoadInIncognito(app))
      continue;
    apps_.push_back(new App(app));
  }
}

void AppSearchProvider::RefreshApps() {
  ExtensionService* extension_service =
      extensions::ExtensionSystemFactory::GetForProfile(profile_)->
      extension_service();
  if (!extension_service)
    return;  // During testing, there is no extension service.

  apps_.clear();

  AddApps(extension_service->extensions(), extension_service);
  AddApps(extension_service->disabled_extensions(), extension_service);
  AddApps(extension_service->terminated_extensions(), extension_service);
}

void AppSearchProvider::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& detaila) {
  RefreshApps();
}

}  // namespace app_list
