// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_WEBSTORE_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_WEBSTORE_RESULT_H_

#include <string>

#include "base/basictypes.h"
#include "ui/app_list/search_result.h"
#include "url/gurl.h"

class AppListControllerDelegate;
class Profile;

namespace app_list {

// A "search in webstore" result.
class SearchWebstoreResult : public SearchResult {
 public:
  SearchWebstoreResult(Profile* profile,
                       AppListControllerDelegate* controller,
                       const std::string& query);
  ~SearchWebstoreResult() override;

  // SearchResult overrides:
  void Open(int event_flags) override;
  scoped_ptr<SearchResult> Duplicate() const override;

 private:
  Profile* profile_;
  AppListControllerDelegate* controller_;
  const std::string query_;
  GURL launch_url_;

  DISALLOW_COPY_AND_ASSIGN(SearchWebstoreResult);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_SEARCH_WEBSTORE_RESULT_H_
