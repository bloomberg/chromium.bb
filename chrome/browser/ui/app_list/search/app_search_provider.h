// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_SEARCH_PROVIDER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "ui/app_list/search_provider.h"

class AppListControllerDelegate;
class Profile;

namespace base {
class Clock;
}

namespace app_list {

class AppListItemList;

class AppSearchProvider : public SearchProvider {
 public:
  class App;
  class DataSource;
  typedef std::vector<std::unique_ptr<App>> Apps;

  AppSearchProvider(Profile* profile,
                    AppListControllerDelegate* list_controller,
                    std::unique_ptr<base::Clock> clock,
                    AppListItemList* top_level_item_list);
  ~AppSearchProvider() override;

  // SearchProvider overrides:
  void Start(bool is_voice_query, const base::string16& query) override;
  void Stop() override;

  // Refresh indexed app data and update search results. When |force_inline| is
  // set to true, search results is updated before returning from the function.
  // Otherwise, search results would be grouped, i.e. multiple calls would only
  // update search results once.
  void RefreshAppsAndUpdateResults(bool force_inline);

 private:
  void RefreshApps();
  void UpdateResults();

  AppListControllerDelegate* list_controller_;
  base::string16 query_;
  Apps apps_;
  AppListItemList* top_level_item_list_;
  std::unique_ptr<base::Clock> clock_;
  std::vector<std::unique_ptr<DataSource>> data_sources_;
  base::WeakPtrFactory<AppSearchProvider> update_results_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppSearchProvider);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_APP_SEARCH_PROVIDER_H_
