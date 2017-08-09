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
#include "chrome/browser/ui/app_list/search/answer_card/answer_card_search_provider.h"
#include "chrome/browser/ui/app_list/search/answer_card/answer_card_web_contents.h"
#include "chrome/browser/ui/app_list/search/app_search_provider.h"
#include "chrome/browser/ui/app_list/search/history_factory.h"
#include "chrome/browser/ui/app_list/search/launcher_search/launcher_search_provider.h"
#include "chrome/browser/ui/app_list/search/omnibox_provider.h"
#include "chrome/browser/ui/app_list/search/suggestions/suggestions_search_provider.h"
#include "chrome/browser/ui/app_list/search/webstore/webstore_provider.h"
#include "chrome/common/chrome_switches.h"
#include "components/arc/arc_util.cc"
#include "ui/app_list/app_list_features.h"
#include "ui/app_list/app_list_model.h"
#include "ui/app_list/app_list_switches.h"
#include "ui/app_list/search/mixer.h"
#include "ui/app_list/search_controller.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/ui/app_list/search/arc/arc_playstore_search_provider.h"
#endif

namespace app_list {

namespace {

// Maximum number of results to show in each mixer group.
constexpr size_t kMaxAppsGroupResults = 8;
constexpr size_t kMaxOmniboxResults = 4;
constexpr size_t kMaxWebstoreResults = 2;
constexpr size_t kMaxSuggestionsResults = 6;
constexpr size_t kMaxLauncherSearchResults = 2;
#if defined(OS_CHROMEOS)
constexpr size_t kMaxPlayStoreResults = 6;
#endif

// Constants related to the SuggestionsService in AppList field trial.
constexpr char kSuggestionsProviderFieldTrialName[] =
    "SuggestionsAppListProvider";
constexpr char kSuggestionsProviderFieldTrialEnabledPrefix[] = "Enabled";

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
  std::unique_ptr<SearchController> controller =
      base::MakeUnique<SearchController>(
          model->search_box(), model->results(),
          HistoryFactory::GetForBrowserContext(profile));

  // Add mixer groups. There are three main groups: apps, webstore and
  // omnibox. Each group has a "soft" maximum number of results. However, if
  // a query turns up very few results, the mixer may take more than this
  // maximum from a particular group.

  // Multiplier 100 is used because the answer card is designed to be the most
  // relevant result and must be on the top of the result list.
  size_t answer_card_group_id = controller->AddGroup(1, 100.0);
  size_t apps_group_id = controller->AddGroup(kMaxAppsGroupResults, 1.0);
  size_t omnibox_group_id = controller->AddGroup(kMaxOmniboxResults, 1.0);
  size_t webstore_group_id = controller->AddGroup(kMaxWebstoreResults, 0.4);

  // Add search providers.
  controller->AddProvider(
      apps_group_id,
      base::MakeUnique<AppSearchProvider>(
          profile, list_controller, base::MakeUnique<base::DefaultClock>(),
          model->top_level_item_list()));
  controller->AddProvider(omnibox_group_id, base::MakeUnique<OmniboxProvider>(
                                                profile, list_controller));
  if (arc::IsWebstoreSearchEnabled()) {
    controller->AddProvider(
        webstore_group_id,
        base::MakeUnique<WebstoreProvider>(profile, list_controller));
  }
  if (features::IsAnswerCardEnabled()) {
    controller->AddProvider(
        answer_card_group_id,
        base::MakeUnique<AnswerCardSearchProvider>(
            profile, model, list_controller,
            base::MakeUnique<AnswerCardWebContents>(profile)));
  }
  if (IsSuggestionsSearchProviderEnabled()) {
    size_t suggestions_group_id =
        controller->AddGroup(kMaxSuggestionsResults, 1.0);
    controller->AddProvider(
        suggestions_group_id,
        base::MakeUnique<SuggestionsSearchProvider>(profile, list_controller));
  }

  // LauncherSearchProvider is added only when flag is enabled, not in guest
  // session and running on Chrome OS.
  if (app_list::switches::IsDriveSearchInChromeLauncherEnabled() &&
      !profile->IsGuestSession()) {
    size_t search_api_group_id =
        controller->AddGroup(kMaxLauncherSearchResults, 1.0);
    controller->AddProvider(search_api_group_id,
                            base::MakeUnique<LauncherSearchProvider>(profile));
  }

#if defined(OS_CHROMEOS)
  if (features::IsPlayStoreAppSearchEnabled()) {
    size_t playstore_api_group_id =
        controller->AddGroup(kMaxPlayStoreResults, 1.0);
    controller->AddProvider(
        playstore_api_group_id,
        base::MakeUnique<ArcPlayStoreSearchProvider>(kMaxPlayStoreResults,
                                                     profile, list_controller));
  }
#endif
  return controller;
}

}  // namespace app_list
