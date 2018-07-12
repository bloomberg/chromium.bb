// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_controller_factory.h"

#include <stddef.h>

#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_switches.h"
#include "base/strings/string_util.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/search/answer_card/answer_card_search_provider.h"
#include "chrome/browser/ui/app_list/search/answer_card/answer_card_web_contents.h"
#include "chrome/browser/ui/app_list/search/app_search_provider.h"
#include "chrome/browser/ui/app_list/search/arc/arc_app_data_search_provider.h"
#include "chrome/browser/ui/app_list/search/arc/arc_app_shortcuts_search_provider.h"
#include "chrome/browser/ui/app_list/search/arc/arc_playstore_search_provider.h"
#include "chrome/browser/ui/app_list/search/launcher_search/launcher_search_provider.h"
#include "chrome/browser/ui/app_list/search/mixer.h"
#include "chrome/browser/ui/app_list/search/omnibox_provider.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chrome/browser/ui/app_list/search/settings_shortcut/settings_shortcut_provider.h"
#include "chrome/browser/ui/app_list/search/webstore/webstore_provider.h"
#include "chrome/common/chrome_switches.h"
#include "components/arc/arc_util.h"

namespace app_list {

namespace {

// Maximum number of results to show in each mixer group.
constexpr size_t kMaxAppsGroupResults = 6;
constexpr size_t kMaxOmniboxResults = 4;
constexpr size_t kMaxWebstoreResults = 2;
constexpr size_t kMaxLauncherSearchResults = 2;
// We show up to 6 Play Store results. However, part of Play Store results may
// be filtered out because they may correspond to already installed Web apps. So
// we request twice as many Play Store apps as we can show. Note that this still
// doesn't guarantee that all 6 positions will be filled, as we might in theory
// filter out more than half of results.
// TODO(753947): Consider progressive algorithm of getting Play Store results.
constexpr size_t kMaxPlayStoreResults = 12;

// TODO(warx): Need UX spec.
constexpr size_t kMaxAppDataResults = 6;
constexpr size_t kMaxAppShortcutResults = 4;

// TODO(wutao): Need UX spec.
constexpr size_t kMaxSettingsShortcutResults = 6;

constexpr float kBoostOfSettingsShortcut = 10.0f;
constexpr float kBoostOfApps = 8.0f;

}  // namespace

std::unique_ptr<SearchController> CreateSearchController(
    Profile* profile,
    AppListModelUpdater* model_updater,
    AppListControllerDelegate* list_controller) {
  std::unique_ptr<SearchController> controller =
      std::make_unique<SearchController>(model_updater, list_controller);

  // Add mixer groups. There are four main groups: answer card, apps, webstore
  // and omnibox. Each group has a "soft" maximum number of results. However, if
  // a query turns up very few results, the mixer may take more than this
  // maximum from a particular group.

  // For fullscreen app list, Settings shortcuts will show on the very top and
  // apps and answer card in the middle and other search results in the bottom.
  // So set boost 10.0, 8.0, 5.0, 0.0 respectively.
  size_t answer_card_group_id = controller->AddGroup(1, 1.0, 5.0);
  size_t apps_group_id =
      controller->AddGroup(kMaxAppsGroupResults, 1.0, kBoostOfApps);
  size_t omnibox_group_id = controller->AddGroup(kMaxOmniboxResults, 1.0, 0.0);
  size_t webstore_group_id =
      controller->AddGroup(kMaxWebstoreResults, 0.4, 0.0);

  // Add search providers.
  controller->AddProvider(
      apps_group_id, std::make_unique<AppSearchProvider>(
                         profile, list_controller,
                         base::DefaultClock::GetInstance(), model_updater));
  controller->AddProvider(omnibox_group_id, std::make_unique<OmniboxProvider>(
                                                profile, list_controller));
  if (arc::IsWebstoreSearchEnabled()) {
    controller->AddProvider(
        webstore_group_id,
        std::make_unique<WebstoreProvider>(profile, list_controller));
  }
  if (features::IsAnswerCardEnabled()) {
    controller->AddProvider(
        answer_card_group_id,
        std::make_unique<AnswerCardSearchProvider>(
            profile, model_updater, list_controller,
            std::make_unique<AnswerCardWebContents>(profile),
            std::make_unique<AnswerCardWebContents>(profile)));
  }

  // LauncherSearchProvider is added only when flag is enabled, not in guest
  // session and running on Chrome OS.
  if (app_list::switches::IsDriveSearchInChromeLauncherEnabled() &&
      !profile->IsGuestSession()) {
    size_t search_api_group_id =
        controller->AddGroup(kMaxLauncherSearchResults, 1.0, 0.0);
    controller->AddProvider(search_api_group_id,
                            std::make_unique<LauncherSearchProvider>(profile));
  }

  if (features::IsPlayStoreAppSearchEnabled()) {
    // Set same boost as apps group since Play store results are placed
    // with apps.
    size_t playstore_api_group_id =
        controller->AddGroup(kMaxPlayStoreResults, 1.0, kBoostOfApps);
    controller->AddProvider(
        playstore_api_group_id,
        std::make_unique<ArcPlayStoreSearchProvider>(kMaxPlayStoreResults,
                                                     profile, list_controller));
  }

  size_t app_data_api_group_id =
      controller->AddGroup(kMaxAppDataResults, 1.0, kBoostOfApps);
  controller->AddProvider(app_data_api_group_id,
                          std::make_unique<ArcAppDataSearchProvider>(
                              kMaxAppDataResults, list_controller));

  if (features::IsSettingsShortcutSearchEnabled()) {
    size_t settings_shortcut_group_id = controller->AddGroup(
        kMaxSettingsShortcutResults, 1.0, kBoostOfSettingsShortcut);
    controller->AddProvider(
        settings_shortcut_group_id,
        std::make_unique<SettingsShortcutProvider>(profile));
  }

  if (features::IsAppShortcutSearchEnabled()) {
    size_t app_shortcut_group_id =
        controller->AddGroup(kMaxAppShortcutResults, 1.0, kBoostOfApps);
    controller->AddProvider(
        app_shortcut_group_id,
        std::make_unique<ArcAppShortcutsSearchProvider>(
            kMaxAppShortcutResults, profile, list_controller));
  }

  return controller;
}

}  // namespace app_list
