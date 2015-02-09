// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_OMNIBOX_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_OMNIBOX_RESULT_H_

#include "base/memory/scoped_ptr.h"
#include "components/omnibox/autocomplete_match.h"
#include "ui/app_list/search_result.h"

class AppListControllerDelegate;
class AutocompleteController;
class Profile;

namespace app_list {

class OmniboxResult : public SearchResult {
 public:
  OmniboxResult(Profile* profile,
                AppListControllerDelegate* list_controller,
                AutocompleteController* autocomplete_controller,
                bool is_voice_query,
                const AutocompleteMatch& match);
  ~OmniboxResult() override;

  // SearchResult overrides:
  void Open(int event_flags) override;

  scoped_ptr<SearchResult> Duplicate() const override;

 private:
  void UpdateIcon();

  void UpdateTitleAndDetails();

  Profile* profile_;
  AppListControllerDelegate* list_controller_;
  AutocompleteController* autocomplete_controller_;
  bool is_voice_query_;
  AutocompleteMatch match_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxResult);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_OMNIBOX_RESULT_H_
