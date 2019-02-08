// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_CROSTINI_CROSTINI_REPOSITORY_SEARCH_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_CROSTINI_CROSTINI_REPOSITORY_SEARCH_RESULT_H_

#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/app_list/app_context_menu_delegate.h"
#include "chrome/browser/ui/app_list/search/chrome_search_result.h"

namespace app_list {

class CrostiniRepositorySearchResult : public ChromeSearchResult {
 public:
  explicit CrostiniRepositorySearchResult(const std::string& app_name);
  ~CrostiniRepositorySearchResult() override;

  // ChromeSearchResult overrides:
  // TODO(https://crbug.com/921429): Implement open functionality (confirmation
  // and installation of app).
  void Open(int event_flags) override;

 private:
  std::string app_name_;
  base::WeakPtrFactory<CrostiniRepositorySearchResult> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniRepositorySearchResult);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_CROSTINI_CROSTINI_REPOSITORY_SEARCH_RESULT_H_
