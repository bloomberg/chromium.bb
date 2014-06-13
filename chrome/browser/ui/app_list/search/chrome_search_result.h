// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_CHROME_SEARCH_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_CHROME_SEARCH_RESULT_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "ui/app_list/search_result.h"

namespace app_list {

// The type of the search result. This is used for logging so do not change the
// order of this enum.
enum ChromeSearchResultType {
  // A result that forwards an omnibox search result.
  OMNIBOX_SEARCH_RESULT,
  // An app result.
  APP_SEARCH_RESULT,
  // A search result from the webstore.
  WEBSTORE_SEARCH_RESULT,
  // A result that opens a webstore search.
  SEARCH_WEBSTORE_SEARCH_RESULT,
  // A result that opens a people search.
  SEARCH_PEOPLE_SEARCH_RESULT,
  SEARCH_RESULT_TYPE_BOUNDARY
};

// Base class of all search results. It provides an additional interface
// for SearchController to mix the results, duplicate a result from a
// SearchProvider and pass it to UI and invoke actions on the results when
// underlying  UI is activated.
class ChromeSearchResult : public SearchResult {
 public:
  ChromeSearchResult() {}
  virtual ~ChromeSearchResult() {}

  // Creates a copy of the result.
  virtual scoped_ptr<ChromeSearchResult> Duplicate() = 0;

  virtual ChromeSearchResultType GetType() = 0;

  // Overridden from SearchResult:
  virtual void Open(int event_flags) = 0;
  virtual void InvokeAction(int action_index, int event_flags) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeSearchResult);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_CHROME_SEARCH_RESULT_H_
