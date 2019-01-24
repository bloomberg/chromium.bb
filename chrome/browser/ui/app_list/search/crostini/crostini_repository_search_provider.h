// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_CROSTINI_CROSTINI_REPOSITORY_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_CROSTINI_CROSTINI_REPOSITORY_SEARCH_PROVIDER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/app_list/search/search_provider.h"

class Profile;

namespace app_list {

// Search provider for Crostini repository search.
class CrostiniRepositorySearchProvider : public SearchProvider {
 public:
  explicit CrostiniRepositorySearchProvider(Profile* profile);
  ~CrostiniRepositorySearchProvider() override;

  // SearchProvider overrides:
  void Start(const base::string16& query) override;

 private:
  // Callback for CrostiniRepositorySearchProvider::Start.
  void OnStart(const std::vector<std::string>& app_names);

  // Plaintext query to be searched.
  std::string query_;

  // Unowned pointer to associated profile.
  Profile* const profile_;

  base::WeakPtrFactory<CrostiniRepositorySearchProvider> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniRepositorySearchProvider);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_CROSTINI_CROSTINI_REPOSITORY_SEARCH_PROVIDER_H_
