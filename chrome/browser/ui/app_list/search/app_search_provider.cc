// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/app_search_provider.h"

#include <string>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/clock.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/app_list_syncable_service_factory.h"
#include "chrome/browser/ui/app_list/search/app_result.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/search/tokenized_string.h"
#include "ui/app_list/search/tokenized_string_match.h"

using extensions::ExtensionRegistry;

namespace {

// The size of each step unlaunched apps should increase their relevance by.
const double kUnlaunchedAppRelevanceStepSize = 0.0001;
}

namespace app_list {

class AppSearchProvider::App {
 public:
  explicit App(const extensions::Extension* extension,
               const base::Time& last_launch_time)
      : app_id_(extension->id()),
        indexed_name_(base::UTF8ToUTF16(extension->short_name())),
        last_launch_time_(last_launch_time) {}
  ~App() {}

  const std::string& app_id() const { return app_id_; }
  const TokenizedString& indexed_name() const { return indexed_name_; }
  const base::Time& last_launch_time() const { return last_launch_time_; }

 private:
  const std::string app_id_;
  TokenizedString indexed_name_;
  base::Time last_launch_time_;

  DISALLOW_COPY_AND_ASSIGN(App);
};

AppSearchProvider::AppSearchProvider(Profile* profile,
                                     AppListControllerDelegate* list_controller,
                                     scoped_ptr<base::Clock> clock,
                                     AppListItemList* top_level_item_list)
    : profile_(profile),
      list_controller_(list_controller),
      extension_registry_observer_(this),
      top_level_item_list_(top_level_item_list),
      clock_(clock.Pass()),
      update_results_factory_(this) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(profile_));
  RefreshApps();
}

AppSearchProvider::~AppSearchProvider() {}

void AppSearchProvider::Start(bool /*is_voice_query*/,
                              const base::string16& query) {
  query_ = query;
  const TokenizedString query_terms(query);

  ClearResults();

  bool show_recommendations = query.empty();
  // Refresh list of apps to ensure we have the latest launch time information.
  // This will also cause the results to update.
  if (show_recommendations)
    RefreshApps();

  UpdateResults();
}

void AppSearchProvider::Stop() {
}

void AppSearchProvider::UpdateResults() {
  const TokenizedString query_terms(query_);
  bool show_recommendations = query_.empty();
  ClearResults();

  if (show_recommendations) {
    // Build a map of app ids to their position in the app list.
    std::map<std::string, size_t> id_to_app_list_index;
    for (size_t i = 0; i < top_level_item_list_->item_count(); ++i) {
      id_to_app_list_index[top_level_item_list_->item_at(i)->id()] = i;
    }

    for (const App* app : apps_) {
      scoped_ptr<AppResult> result(
          new AppResult(profile_, app->app_id(), list_controller_, true));
      result->set_title(app->indexed_name().text());

      // Use the app list order to tiebreak apps that have never been launched.
      if (app->last_launch_time().is_null()) {
        auto it = id_to_app_list_index.find(app->app_id());
        // If it's in a folder, it won't be in |id_to_app_list_index|. Rank
        // those as if they are at the end of the list.
        size_t app_list_index =
            it == id_to_app_list_index.end() ? apps_.size() : (*it).second;
        if (app_list_index > apps_.size())
          app_list_index = apps_.size();

        result->set_relevance(kUnlaunchedAppRelevanceStepSize *
                              (apps_.size() - app_list_index));
      } else {
        result->UpdateFromLastLaunched(clock_->Now(), app->last_launch_time());
      }
      Add(result.Pass());
    }
  } else {
    for (const App* app : apps_) {
      scoped_ptr<AppResult> result(
          new AppResult(profile_, app->app_id(), list_controller_, false));
      TokenizedStringMatch match;
      if (!match.Calculate(query_terms, app->indexed_name()))
        continue;

      result->UpdateFromMatch(app->indexed_name(), match);
      Add(result.Pass());
    }
  }

  update_results_factory_.InvalidateWeakPtrs();
}

void AppSearchProvider::AddApps(const extensions::ExtensionSet& extensions) {
  extensions::ExtensionPrefs* prefs = extensions::ExtensionPrefs::Get(profile_);
  for (extensions::ExtensionSet::const_iterator iter = extensions.begin();
       iter != extensions.end(); ++iter) {
    const extensions::Extension* app = iter->get();

    if (!extensions::ui_util::ShouldDisplayInAppLauncher(app, profile_))
      continue;

    if (profile_->IsOffTheRecord() &&
        !extensions::util::CanLoadInIncognito(app, profile_))
      continue;

    apps_.push_back(new App(app, prefs->GetLastLaunchTime(app->id())));
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
  if (!update_results_factory_.HasWeakPtrs()) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&AppSearchProvider::UpdateResults,
                   update_results_factory_.GetWeakPtr()));
  }
}

void AppSearchProvider::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const extensions::Extension* extension,
    extensions::UninstallReason reason) {
  RefreshApps();
  if (!update_results_factory_.HasWeakPtrs()) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&AppSearchProvider::UpdateResults,
                   update_results_factory_.GetWeakPtr()));
  }
}

}  // namespace app_list
