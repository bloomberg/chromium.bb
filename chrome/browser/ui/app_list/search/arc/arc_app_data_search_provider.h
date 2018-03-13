// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ARC_APP_DATA_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ARC_APP_DATA_SEARCH_PROVIDER_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/time/time.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "components/arc/common/app.mojom.h"

class Profile;
class AppListControllerDelegate;

namespace app_list {

class ArcAppDataSearchProvider : public SearchProvider {
 public:
  ArcAppDataSearchProvider(int max_results,
                           Profile* profile,
                           AppListControllerDelegate* list_controller);
  ~ArcAppDataSearchProvider() override;

  // SearchProvider:
  void Start(const base::string16& query) override;

 private:
  void OnResults(base::TimeTicks query_start_time,
                 arc::mojom::AppDataRequestState state,
                 std::vector<arc::mojom::AppDataResultPtr> results);

  const int max_results_;
  // |profile_| is owned by ProfileInfo.
  Profile* const profile_;
  // |list_controller_| is owned by AppListServiceAsh and lives until the
  // service finishes.
  AppListControllerDelegate* const list_controller_;
  base::WeakPtrFactory<ArcAppDataSearchProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ArcAppDataSearchProvider);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_ARC_ARC_APP_DATA_SEARCH_PROVIDER_H_
