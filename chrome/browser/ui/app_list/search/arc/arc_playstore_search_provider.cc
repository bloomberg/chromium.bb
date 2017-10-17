// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/arc/arc_playstore_search_provider.h"

#include <string>
#include <utility>

#include "base/metrics/histogram_macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/extensions/gfx_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/search/arc/arc_playstore_search_result.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"

namespace {
constexpr int kHistogramBuckets = 13;
constexpr char kAppListPlayStoreQueryStateHistogram[] =
    "Apps.AppListPlayStoreQueryState";
// TODO(crbug.com/742517): Use the mojo generated constants.
constexpr int kAppListPlayStoreQueryStateNum = 17;

// Skips Play Store apps that have equivalent extensions installed.
// Do not skip recent instant apps since they should be treated like
// on-device apps.
bool CanSkipSearchResult(content::BrowserContext* context,
                         const arc::mojom::AppDiscoveryResult& result) {
  if (result.is_instant_app && result.is_recent)
    return false;

  if (!result.package_name.has_value())
    return false;

  if (!extensions::util::GetEquivalentInstalledExtensions(
           context, result.package_name.value())
           .empty()) {
    return true;
  }

  // TODO(crbug/763562): Remove this once we have a fix in Phonesky.
  // Don't show installed Android apps.
  const ArcAppListPrefs* arc_prefs = ArcAppListPrefs::Get(context);
  return arc_prefs && arc_prefs->GetPackage(result.package_name.value());
}
}  // namespace

namespace app_list {

ArcPlayStoreSearchProvider::ArcPlayStoreSearchProvider(
    int max_results,
    Profile* profile,
    AppListControllerDelegate* list_controller)
    : max_results_(max_results),
      profile_(profile),
      list_controller_(list_controller),
      weak_ptr_factory_(this) {
  DCHECK_EQ(kHistogramBuckets, max_results + 1);
}

ArcPlayStoreSearchProvider::~ArcPlayStoreSearchProvider() = default;

void ArcPlayStoreSearchProvider::Start(bool is_voice_query,
                                       const base::string16& query) {
  DCHECK(!is_voice_query);
  arc::mojom::AppInstance* app_instance =
      arc::ArcServiceManager::Get()
          ? ARC_GET_INSTANCE_FOR_METHOD(
                arc::ArcServiceManager::Get()->arc_bridge_service()->app(),
                GetRecentAndSuggestedAppsFromPlayStore)
          : nullptr;

  if (app_instance == nullptr || query.empty()) {
    ClearResults();
    return;
  }

  app_instance->GetRecentAndSuggestedAppsFromPlayStore(
      base::UTF16ToUTF8(query), max_results_,
      base::Bind(&ArcPlayStoreSearchProvider::OnResults,
                 weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now()));
}

void ArcPlayStoreSearchProvider::Stop() {}

void ArcPlayStoreSearchProvider::OnResults(
    base::TimeTicks query_start_time,
    arc::mojom::AppDiscoveryRequestState state,
    std::vector<arc::mojom::AppDiscoveryResultPtr> results) {
  UMA_HISTOGRAM_ENUMERATION(kAppListPlayStoreQueryStateHistogram, state,
                            kAppListPlayStoreQueryStateNum);
  if (state != arc::mojom::AppDiscoveryRequestState::SUCCESS) {
    DCHECK(results.empty());
    ClearResults();
    return;
  }

  SearchProvider::Results new_results;
  size_t instant_app_count = 0;
  for (auto& result : results) {
    if (result->is_instant_app)
      ++instant_app_count;

    if (CanSkipSearchResult(profile_, *result))
      continue;

    new_results.emplace_back(base::MakeUnique<ArcPlayStoreSearchResult>(
        std::move(result), profile_, list_controller_));
  }
  SwapResults(&new_results);

  // Record user metrics.
  UMA_HISTOGRAM_TIMES("Arc.PlayStoreSearch.QueryTime",
                      base::TimeTicks::Now() - query_start_time);
  UMA_HISTOGRAM_EXACT_LINEAR("Arc.PlayStoreSearch.ReturnedAppsTotal",
                             results.size(), kHistogramBuckets);
  UMA_HISTOGRAM_EXACT_LINEAR("Arc.PlayStoreSearch.ReturnedUninstalledApps",
                             results.size() - instant_app_count,
                             kHistogramBuckets);
  UMA_HISTOGRAM_EXACT_LINEAR("Arc.PlayStoreSearch.ReturnedInstantApps",
                             instant_app_count, kHistogramBuckets);
}

}  // namespace app_list
