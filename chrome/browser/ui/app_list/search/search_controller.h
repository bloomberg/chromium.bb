// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_CONTROLLER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_CONTROLLER_H_

#include <stddef.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/app_list/search/mixer.h"

class AppListModelUpdater;

namespace app_list {

class History;
class SearchProvider;
class SearchResult;

// Controller that collects query from given SearchBoxModel, dispatches it
// to all search providers, then invokes the mixer to mix and to publish the
// results to the given SearchResults UI model.
class SearchController {
 public:
  SearchController(AppListModelUpdater* model_updater, History* history);
  virtual ~SearchController();

  // TODO(hejq): can we accept a trimmed query here?
  void Start(const base::string16& raw_query);

  void OpenResult(SearchResult* result, int event_flags);
  void InvokeResultAction(SearchResult* result,
                          int action_index,
                          int event_flags);

  // Adds a new mixer group. See Mixer::AddGroup.
  size_t AddGroup(size_t max_results, double multiplier, double boost);

  // Takes ownership of |provider| and associates it with given mixer group.
  void AddProvider(size_t group_id, std::unique_ptr<SearchProvider> provider);

 private:
  // Invoked when the search results are changed.
  void OnResultsChanged();

  base::string16 last_raw_query_;

  bool dispatching_query_ = false;

  // If true, the search results are shown on the launcher start page.
  bool query_for_recommendation_ = false;

  using Providers = std::vector<std::unique_ptr<SearchProvider>>;
  Providers providers_;
  std::unique_ptr<Mixer> mixer_;
  History* history_;  // KeyedService, not owned.

  DISALLOW_COPY_AND_ASSIGN(SearchController);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_CONTROLLER_H_
