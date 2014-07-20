// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/app_search_provider.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/app_result.h"
#include "chrome/browser/ui/app_list/search/tokenized_string.h"
#include "chrome/browser/ui/app_list/search/tokenized_string_match.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"

using extensions::ExtensionRegistry;

namespace app_list {

class AppSearchProvider::App {
 public:
  explicit App(const extensions::Extension* extension)
      : app_id_(extension->id()),
        indexed_name_(base::UTF8ToUTF16(extension->name())) {
  }
  ~App() {}

  const std::string& app_id() const { return app_id_; }
  const TokenizedString& indexed_name() const { return indexed_name_; }

 private:
  const std::string app_id_;
  TokenizedString indexed_name_;

  DISALLOW_COPY_AND_ASSIGN(App);
};

AppSearchProvider::AppSearchProvider(Profile* profile,
                                     AppListControllerDelegate* list_controller)
    : profile_(profile),
      list_controller_(list_controller),
      extension_registry_observer_(this) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(profile_));
  RefreshApps();
}

AppSearchProvider::~AppSearchProvider() {}

void AppSearchProvider::Start(const base::string16& query) {
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
    Add(result.PassAs<SearchResult>());
  }
}

void AppSearchProvider::Stop() {}

void AppSearchProvider::AddApps(const extensions::ExtensionSet& extensions) {
  for (extensions::ExtensionSet::const_iterator iter = extensions.begin();
       iter != extensions.end(); ++iter) {
    const extensions::Extension* app = iter->get();

    if (!extensions::ui_util::ShouldDisplayInAppLauncher(app, profile_))
      continue;

    if (profile_->IsOffTheRecord() &&
        !extensions::util::CanLoadInIncognito(app, profile_))
      continue;
    apps_.push_back(new App(app));
  }
}

void AppSearchProvider::RefreshApps() {
  apps_.clear();
  ExtensionRegistry* registry = ExtensionRegistry::Get(profile_);
  AddApps(registry->enabled_extensions());
  AddApps(registry->disabled_extensions());
  AddApps(registry->terminated_extensions());
}

void AppSearchProvider::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension) {
  RefreshApps();
}

void AppSearchProvider::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  RefreshApps();
}

}  // namespace app_list
