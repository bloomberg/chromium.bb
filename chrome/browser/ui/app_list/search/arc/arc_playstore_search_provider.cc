// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/arc/arc_playstore_search_provider.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "chrome/browser/ui/app_list/search/arc/arc_playstore_search_result.h"
#include "components/arc/arc_bridge_service.h"
#include "components/arc/arc_service_manager.h"

namespace app_list {

ArcPlayStoreSearchProvider::ArcPlayStoreSearchProvider(
    int max_results,
    Profile* profile,
    AppListControllerDelegate* list_controller)
    : max_results_(max_results),
      profile_(profile),
      list_controller_(list_controller),
      weak_ptr_factory_(this) {}

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

  if (app_instance == nullptr)
    return;

  ClearResults();
  app_instance->GetRecentAndSuggestedAppsFromPlayStore(
      UTF16ToUTF8(query), max_results_,
      base::Bind(&ArcPlayStoreSearchProvider::OnResults,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ArcPlayStoreSearchProvider::Stop() {}

void ArcPlayStoreSearchProvider::OnResults(
    std::vector<arc::mojom::AppDiscoveryResultPtr> results) {
  for (auto& result : results) {
    Add(base::MakeUnique<ArcPlayStoreSearchResult>(std::move(result), profile_,
                                                   list_controller_));
  }
}

}  // namespace app_list
