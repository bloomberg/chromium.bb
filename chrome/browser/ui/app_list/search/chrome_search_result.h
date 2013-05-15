// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_CHROME_SEARCH_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_CHROME_SEARCH_RESULT_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "ui/app_list/search_result.h"

namespace app_list {

// Base class of all search results. It provides an additional interface
// for SearchController to mix the results, duplicate a result from a
// SearchProvider and pass it to UI and invoke actions on the results when
// underlying  UI is activated.
class ChromeSearchResult : public SearchResult {
 public:
  ChromeSearchResult() : relevance_(0.0) {}
  virtual ~ChromeSearchResult() {}

  // Opens the result.
  virtual void Open(int event_flags) = 0;

  // Invokes a custom action on the result.
  virtual void InvokeAction(int action_index, int event_flags) = 0;

  // Creates a copy of the result.
  virtual scoped_ptr<ChromeSearchResult> Duplicate() = 0;

  const std::string& id() const { return id_; }
  double relevance() { return relevance_; }

 protected:
  void set_id(const std::string& id) { id_ = id; }
  void set_relevance(double relevance) { relevance_ = relevance; }

 private:
  std::string id_;
  double relevance_;

  DISALLOW_COPY_AND_ASSIGN(ChromeSearchResult);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_CHROME_SEARCH_RESULT_H_
