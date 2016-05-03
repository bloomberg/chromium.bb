// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_controller_factory.h"

#include <stddef.h>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/app_search_provider.h"
#include "chrome/browser/ui/app_list/search/history_factory.h"
#include "chrome/browser/ui/app_list/search/omnibox_provider.h"
#include "chrome/browser/ui/app_list/search/suggestions/suggestions_search_provider.h"
#include "chrome/browser/ui/app_list/search/webstore/webstore_provider.h"
#include "chrome/common/chrome_switches.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/search/mixer.h"
#include "ui/app_list/search_controller.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/app_list/search/launcher_search/launcher_search_provider.h"
#endif

namespace app_list {

namespace {

// Maximum number of results to show in each mixer group.
const size_t kMaxAppsGroupResults = 4;
// Ignored unless AppListMixer field trial is "Blended".
const size_t kMaxOmniboxResults = 4;
const size_t kMaxWebstoreResults = 2;
const size_t kMaxSuggestionsResults = 6;

#if defined(OS_CHROMEOS)
const size_t kMaxLauncherSearchResults = 2;
#endif

// Constants related to the SuggestionsService in AppList field trial.
const char kSuggestionsProviderFieldTrialName[] = "SuggestionsAppListProvider";
const char kSuggestionsProviderFieldTrialEnabledPrefix[] = "Enabled";

// Returns whether the user is part of a group where the Suggestions provider is
// enabled.
bool IsSuggestionsSearchProviderEnabled() {
  return base::StartsWith(
      base::FieldTrialList::FindFullName(kSuggestionsProviderFieldTrialName),
      kSuggestionsProviderFieldTrialEnabledPrefix,
      base::CompareCase::SENSITIVE);
}

}  // namespace

std::unique_ptr<SearchController> CreateSearchController(
    Profile* profile,
    AppListModel* model,
    AppListControllerDelegate* list_controller) {
  std::unique_ptr<SearchController> controller(
      new SearchController(model->search_box(), model->results(),
                           HistoryFactory::GetForBrowserContext(profile)));

  // Add mixer groups. There are three main groups: apps, webstore and
  // omnibox. The behaviour depends on the AppListMixer field trial:
  // - If default: The apps and webstore groups each have a fixed
  //   maximum number of results. The omnibox group fills the remaining slots
  //   (with a minimum of one result).
  // - If "Blended": Each group has a "soft" maximum number of results. However,
  //   if a query turns up very few results, the mixer may take more than this
  //   maximum from a particular group.
  size_t apps_group_id = controller->AddGroup(kMaxAppsGroupResults, 3.0, 1.0);
  size_t omnibox_group_id =
      controller->AddOmniboxGroup(kMaxOmniboxResults, 2.0, 1.0);
  size_t webstore_group_id =
      controller->AddGroup(kMaxWebstoreResults, 1.0, 0.4);

  // Add search providers.
  controller->AddProvider(
      apps_group_id,
      std::unique_ptr<SearchProvider>(new AppSearchProvider(
          profile, list_controller, base::WrapUnique(new base::DefaultClock()),
          model->top_level_item_list())));
  controller->AddProvider(omnibox_group_id,
                          std::unique_ptr<SearchProvider>(
                              new OmniboxProvider(profile, list_controller)));
  controller->AddProvider(webstore_group_id,
                          std::unique_ptr<SearchProvider>(
                              new WebstoreProvider(profile, list_controller)));
  if (IsSuggestionsSearchProviderEnabled()) {
    size_t suggestions_group_id =
        controller->AddGroup(kMaxSuggestionsResults, 3.0, 1.0);
    controller->AddProvider(
        suggestions_group_id,
        std::unique_ptr<SearchProvider>(
            new SuggestionsSearchProvider(profile, list_controller)));
  }

  // LauncherSearchProvider is added only when flag is enabled, not in guest
  // session and running on Chrome OS.
#if defined(OS_CHROMEOS)
  if (app_list::switches::IsDriveSearchInChromeLauncherEnabled() &&
      !profile->IsGuestSession()) {
    size_t search_api_group_id =
        controller->AddGroup(kMaxLauncherSearchResults, 0.0, 1.0);
    controller->AddProvider(
        search_api_group_id,
        std::unique_ptr<SearchProvider>(new LauncherSearchProvider(profile)));
  }
#endif

  return controller;
}

}  // namespace app_list
