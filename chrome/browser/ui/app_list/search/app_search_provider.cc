// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/app_search_provider.h"

#include <stddef.h>

#include <map>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/clock.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_ui_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/extension_app_result.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "ui/app_list/app_list_item.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/search/tokenized_string.h"
#include "ui/app_list/search/tokenized_string_match.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/browser/ui/app_list/search/arc_app_result.h"
#endif

using extensions::ExtensionRegistry;

namespace {

// The size of each step unlaunched apps should increase their relevance by.
constexpr double kUnlaunchedAppRelevanceStepSize = 0.0001;

// The minimum capacity we reserve in the Apps container which will be filled
// with extensions and ARC apps, to avoid successive reallocation.
constexpr size_t kMinimumReservedAppsContainerCapacity = 60U;
}

namespace app_list {

class AppSearchProvider::App {
 public:
  App(AppSearchProvider::DataSource* data_source,
      const std::string& id,
      const std::string& name,
      const base::Time& last_launch_time,
      const base::Time& install_time)
      : data_source_(data_source),
        id_(id),
        name_(base::UTF8ToUTF16(name)),
        last_launch_time_(last_launch_time),
        install_time_(install_time) {}
  ~App() {}

  TokenizedString* GetTokenizedIndexedName() {
    // Tokenizing a string is expensive. Don't pay the price for it at
    // construction of every App, but rather, only when needed (i.e. when the
    // query is not empty and cache the result.
    if (!tokenized_indexed_name_)
      tokenized_indexed_name_ = base::MakeUnique<TokenizedString>(name_);
    return tokenized_indexed_name_.get();
  }

  AppSearchProvider::DataSource* data_source() { return data_source_; }
  const std::string& id() const { return id_; }
  const base::string16& name() const { return name_; }
  const base::Time& last_launch_time() const { return last_launch_time_; }
  const base::Time& install_time() const { return install_time_; }

 private:
  AppSearchProvider::DataSource* data_source_;
  std::unique_ptr<TokenizedString> tokenized_indexed_name_;
  const std::string id_;
  const base::string16 name_;
  const base::Time last_launch_time_;
  const base::Time install_time_;

  DISALLOW_COPY_AND_ASSIGN(App);
};

class AppSearchProvider::DataSource {
 public:
  DataSource(Profile* profile, AppSearchProvider* owner)
      : profile_(profile),
        owner_(owner) {}
  virtual ~DataSource() {}

  virtual void AddApps(Apps* apps) = 0;

  virtual std::unique_ptr<AppResult> CreateResult(
      const std::string& app_id,
      AppListControllerDelegate* list_controller,
      AppListItemList* top_level_item_list,
      bool is_recommended) = 0;

 protected:
  Profile* profile() { return profile_; }
  AppSearchProvider* owner() { return owner_; }

 private:
  // Unowned pointers.
  Profile* profile_;
  AppSearchProvider* owner_;

  DISALLOW_COPY_AND_ASSIGN(DataSource);
};

namespace {

class ExtensionDataSource : public AppSearchProvider::DataSource,
                            public extensions::ExtensionRegistryObserver {
 public:
  ExtensionDataSource(Profile* profile, AppSearchProvider* owner)
      : AppSearchProvider::DataSource(profile, owner),
        extension_registry_observer_(this) {
    extension_registry_observer_.Add(ExtensionRegistry::Get(profile));
  }
  ~ExtensionDataSource() override {}

  // AppSearchProvider::DataSource overrides:
  void AddApps(AppSearchProvider::Apps* apps) override {
    ExtensionRegistry* registry = ExtensionRegistry::Get(profile());
    AddApps(apps, registry->enabled_extensions());
    AddApps(apps, registry->disabled_extensions());
    AddApps(apps, registry->terminated_extensions());
  }

  std::unique_ptr<AppResult> CreateResult(
      const std::string& app_id,
      AppListControllerDelegate* list_controller,
      AppListItemList* top_level_item_list,
      bool is_recommended) override {
    return base::MakeUnique<ExtensionAppResult>(
        profile(), app_id, list_controller, is_recommended);
  }

  // extensions::ExtensionRegistryObserver overrides:
  void OnExtensionLoaded(content::BrowserContext* browser_context,
                         const extensions::Extension* extension) override {
    owner()->RefreshAppsAndUpdateResults(false);
  }

  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override {
    owner()->RefreshAppsAndUpdateResults(true);
  }

 private:
  void AddApps(AppSearchProvider::Apps* apps,
               const extensions::ExtensionSet& extensions) {
    extensions::ExtensionPrefs* prefs = extensions::ExtensionPrefs::Get(
        profile());
    for (const auto& it : extensions) {
      const extensions::Extension* extension = it.get();

      if (!extensions::ui_util::ShouldDisplayInAppLauncher(extension,
                                                           profile())) {
        continue;
      }

      if (profile()->IsOffTheRecord() &&
          !extensions::util::CanLoadInIncognito(extension, profile())) {
        continue;
      }

      apps->emplace_back(base::MakeUnique<AppSearchProvider::App>(
          this, extension->id(), extension->short_name(),
          prefs->GetLastLaunchTime(extension->id()),
          prefs->GetInstallTime(extension->id())));
    }
  }

  ScopedObserver<extensions::ExtensionRegistry,
                 extensions::ExtensionRegistryObserver>
      extension_registry_observer_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionDataSource);
};

#if defined(OS_CHROMEOS)
class ArcDataSource : public AppSearchProvider::DataSource,
                      public ArcAppListPrefs::Observer {
 public:
  ArcDataSource(Profile* profile, AppSearchProvider* owner)
      : AppSearchProvider::DataSource(profile, owner) {
    ArcAppListPrefs::Get(profile)->AddObserver(this);
  }

  ~ArcDataSource() override {
    ArcAppListPrefs::Get(profile())->RemoveObserver(this);
  }

  // AppSearchProvider::DataSource overrides:
  void AddApps(AppSearchProvider::Apps* apps) override {
    ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(profile());
    CHECK(arc_prefs);

    const std::vector<std::string> app_ids = arc_prefs->GetAppIds();
    for (const auto& app_id : app_ids) {
      std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
          arc_prefs->GetApp(app_id);
      if (!app_info) {
        NOTREACHED();
        continue;
      }

      if (!app_info->launchable || !app_info->showInLauncher)
        continue;

      apps->emplace_back(base::MakeUnique<AppSearchProvider::App>(
          this, app_id, app_info->name, app_info->last_launch_time,
          app_info->install_time));
    }
  }

  std::unique_ptr<AppResult> CreateResult(
      const std::string& app_id,
      AppListControllerDelegate* list_controller,
      AppListItemList* top_level_item_list,
      bool is_recommended) override {
    return base::MakeUnique<ArcAppResult>(profile(), app_id, list_controller,
                                          is_recommended);
  }

  // ArcAppListPrefs::Observer overrides:
  void OnAppRegistered(const std::string& app_id,
                       const ArcAppListPrefs::AppInfo& app_info) override {
    owner()->RefreshAppsAndUpdateResults(false);
  }

  void OnAppRemoved(const std::string& id) override {
    owner()->RefreshAppsAndUpdateResults(true);
  }

  void OnAppNameUpdated(const std::string& id,
                        const std::string& name) override {
    owner()->RefreshAppsAndUpdateResults(false);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcDataSource);
};
#endif

}  // namespace

AppSearchProvider::AppSearchProvider(Profile* profile,
                                     AppListControllerDelegate* list_controller,
                                     std::unique_ptr<base::Clock> clock,
                                     AppListItemList* top_level_item_list)
    : list_controller_(list_controller),
      top_level_item_list_(top_level_item_list),
      clock_(std::move(clock)),
      update_results_factory_(this) {
  data_sources_.emplace_back(
      base::MakeUnique<ExtensionDataSource>(profile, this));
#if defined(OS_CHROMEOS)
  if (arc::IsArcAllowedForProfile(profile))
    data_sources_.emplace_back(base::MakeUnique<ArcDataSource>(profile, this));
#endif
}

AppSearchProvider::~AppSearchProvider() {}

void AppSearchProvider::Start(bool /*is_voice_query*/,
                              const base::string16& query) {
  query_ = query;
  const bool show_recommendations = query.empty();
  // Refresh list of apps to ensure we have the latest launch time information.
  // This will also cause the results to update.
  if (show_recommendations || apps_.empty())
    RefreshApps();

  UpdateResults();
}

void AppSearchProvider::Stop() {
}

void AppSearchProvider::RefreshApps() {
  apps_.clear();
  apps_.reserve(kMinimumReservedAppsContainerCapacity);
  for (auto& data_source : data_sources_)
    data_source->AddApps(&apps_);
}

void AppSearchProvider::UpdateResults() {
  const bool show_recommendations = query_.empty();

  // No need to clear the current results as we will swap them with
  // |new_results| at the end.
  SearchProvider::Results new_results;
  const size_t apps_size = apps_.size();
  new_results.reserve(apps_size);
  if (show_recommendations) {
    // Build a map of app ids to their position in the app list.
    std::map<std::string, size_t> id_to_app_list_index;
    for (size_t i = 0; i < top_level_item_list_->item_count(); ++i)
      id_to_app_list_index[top_level_item_list_->item_at(i)->id()] = i;

    for (auto& app : apps_) {
      std::unique_ptr<AppResult> result = app->data_source()->CreateResult(
          app->id(), list_controller_, top_level_item_list_, true);
      result->set_title(app->name());

      // Use the app list order to tiebreak apps that have never been launched.
      // The apps that have been installed or launched recently should be
      // more relevant than other apps.
      const base::Time time = app->last_launch_time().is_null()
                                  ? app->install_time()
                                  : app->last_launch_time();
      if (time.is_null()) {
        const auto& it = id_to_app_list_index.find(app->id());
        // If it's in a folder, it won't be in |id_to_app_list_index|. Rank
        // those as if they are at the end of the list.
        const size_t app_list_index = (it == id_to_app_list_index.end())
                                          ? apps_size
                                          : std::min(apps_size, it->second);

        result->set_relevance(kUnlaunchedAppRelevanceStepSize *
                              (apps_size - app_list_index));
      } else {
        result->UpdateFromLastLaunchedOrInstalledTime(clock_->Now(), time);
      }
      new_results.emplace_back(std::move(result));
    }
  } else {
    const TokenizedString query_terms(query_);
    for (auto& app : apps_) {
      std::unique_ptr<AppResult> result = app->data_source()->CreateResult(
          app->id(), list_controller_, top_level_item_list_, false);
      TokenizedStringMatch match;
      TokenizedString* indexed_name = app->GetTokenizedIndexedName();
      if (!match.Calculate(query_terms, *indexed_name))
        continue;

      result->UpdateFromMatch(*indexed_name, match);
      new_results.emplace_back(std::move(result));
    }
  }

  SwapResults(&new_results);

  update_results_factory_.InvalidateWeakPtrs();
}

void AppSearchProvider::RefreshAppsAndUpdateResults(bool force_inline) {
  RefreshApps();

  if (force_inline) {
    UpdateResults();
  } else {
    if (!update_results_factory_.HasWeakPtrs()) {
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::Bind(&AppSearchProvider::UpdateResults,
                                update_results_factory_.GetWeakPtr()));
    }
  }
}

}  // namespace app_list
