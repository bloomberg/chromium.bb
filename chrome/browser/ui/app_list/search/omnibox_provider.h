// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_OMNIBOX_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_OMNIBOX_PROVIDER_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"
#include "components/omnibox/browser/autocomplete_controller_delegate.h"

class AppListControllerDelegate;
class AutocompleteController;
class AutocompleteResult;
class Profile;

namespace app_list {

// OmniboxProvider wraps AutocompleteController to provide omnibox results.
class OmniboxProvider : public SearchProvider,
                        public AutocompleteControllerDelegate {
 public:
  explicit OmniboxProvider(Profile* profile,
                           AppListControllerDelegate* list_controller);
  ~OmniboxProvider() override;

  // SearchProvider overrides:
  void Start(const base::string16& query) override;

 private:
  // Populates result list from AutocompleteResult.
  void PopulateFromACResult(const AutocompleteResult& result);

  // AutocompleteControllerDelegate overrides:
  void OnResultChanged(bool default_match_changed) override;

  void RecordQueryLatencyHistogram();

  Profile* profile_;
  bool is_zero_state_enabled_ = false;
  // True if the input is empty for zero state suggestion.
  bool is_zero_state_input_ = false;
  AppListControllerDelegate* list_controller_;
  base::TimeTicks query_start_time_;

  // The omnibox AutocompleteController that collects/sorts/dup-
  // eliminates the results as they come in.
  std::unique_ptr<AutocompleteController> controller_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxProvider);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_OMNIBOX_PROVIDER_H_
