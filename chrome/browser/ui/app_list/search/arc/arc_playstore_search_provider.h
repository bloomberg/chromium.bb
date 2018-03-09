// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ARC_PLAYSTORE_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ARC_PLAYSTORE_SEARCH_PROVIDER_H_

#include <memory>
#include <vector>

#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "components/arc/common/app.mojom.h"

class Profile;
class AppListControllerDelegate;

namespace arc {
enum class ArcPlayStoreSearchRequestState;
}  // namespace arc

namespace app_list {

class ArcPlayStoreSearchProvider : public SearchProvider {
 public:
  ArcPlayStoreSearchProvider(int max_results,
                             Profile* profile,
                             AppListControllerDelegate* list_controller);
  ~ArcPlayStoreSearchProvider() override;

  // SearchProvider:
  void Start(const base::string16& query) override;

 private:
  void OnResults(base::TimeTicks query_start_time,
                 arc::ArcPlayStoreSearchRequestState state,
                 std::vector<arc::mojom::AppDiscoveryResultPtr> results);

  const int max_results_;
  // |profile_| is owned by ProfileInfo.
  Profile* const profile_;
  // list_controller_ is owned by AppListServiceAsh and lives
  // until the service finishes.
  AppListControllerDelegate* const list_controller_;
  base::WeakPtrFactory<ArcPlayStoreSearchProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcPlayStoreSearchProvider);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ARC_PLAYSTORE_SEARCH_PROVIDER_H_
