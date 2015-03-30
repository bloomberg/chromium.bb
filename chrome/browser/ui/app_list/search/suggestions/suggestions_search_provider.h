// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SUGGESTIONS_SUGGESTIONS_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SUGGESTIONS_SUGGESTIONS_SEARCH_PROVIDER_H_

#include "base/basictypes.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/app_list/search_provider.h"

class AppListControllerDelegate;
class Profile;

namespace favicon {
class FaviconService;
}  // namespace favicon

namespace suggestions {
class SuggestionsProfile;
class SuggestionsService;
}  // namespace suggestions

namespace app_list {

class SuggestionsSearchProvider : public SearchProvider {
 public:
  SuggestionsSearchProvider(Profile* profile,
                            AppListControllerDelegate* list_controller);
  ~SuggestionsSearchProvider() override;

  // SearchProvider overrides:
  void Start(bool is_voice_query, const base::string16& query) override;
  void Stop() override;

 private:
  // Called when suggestions are available from the SuggestionsService.
  void OnSuggestionsProfileAvailable(
      const suggestions::SuggestionsProfile& suggestions_profile);

  Profile* profile_;
  AppListControllerDelegate* list_controller_;
  favicon::FaviconService* favicon_service_;
  suggestions::SuggestionsService* suggestions_service_;

  // For callbacks may be run after destruction.
  base::WeakPtrFactory<SuggestionsSearchProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionsSearchProvider);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SUGGESTIONS_SUGGESTIONS_SEARCH_PROVIDER_H_
