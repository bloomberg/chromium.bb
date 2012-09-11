// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_BUILDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_BUILDER_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/autocomplete/autocomplete_controller_delegate.h"
#include "ui/app_list/app_list_model.h"

namespace app_list {
class SearchBoxModel;
class SearchResult;
}

class AppListController;
class AutocompleteController;
class AutocompleteResult;
class Profile;

// SearchBuilder creates app list search results via AutoCompleteController.
class SearchBuilder : public AutocompleteControllerDelegate {
 public:
  SearchBuilder(Profile* profile,
                app_list::SearchBoxModel* search_box,
                app_list::AppListModel::SearchResults* results,
                AppListController* list_controller);
  virtual ~SearchBuilder();

  void StartSearch();
  void StopSearch();

  // Opens |result|.
  void OpenResult(const app_list::SearchResult& result, int event_flags);

  // Invokes a custom action on |result|.  |action_index| corresponds to the
  // index of the selected icon in |result.action_icons()|.
  void InvokeResultAction(const app_list::SearchResult& result,
                          int action_index,
                          int event_flags);

 private:
  // Populates result list from AutocompleteResult.
  void PopulateFromACResult(const AutocompleteResult& result);

  // AutocompleteControllerDelegate overrides:
  virtual void OnResultChanged(bool default_match_changed) OVERRIDE;

  Profile* profile_;

  // Sub models of AppListModel that represent search box and result list.
  app_list::SearchBoxModel* search_box_;
  app_list::AppListModel::SearchResults* results_;

  // The omnibox AutocompleteController that collects/sorts/dup-
  // eliminates the results as they come in.
  scoped_ptr<AutocompleteController> controller_;

  // The controller of the app list. Owned by the app list delegate.
  AppListController* list_controller_;

  DISALLOW_COPY_AND_ASSIGN(SearchBuilder);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_BUILDER_H_
