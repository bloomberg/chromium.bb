// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_CHROME_SEARCH_RESULT_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_CHROME_SEARCH_RESULT_H_

#include <memory>

#include "ash/app_list/model/search/search_result.h"
#include "base/macros.h"

// ChromeSearchResult consists of an icon, title text and details text. Title
// and details text can have tagged ranges that are displayed differently from
// default style.
class ChromeSearchResult : public app_list::SearchResult {
 public:
  ChromeSearchResult();

  // TODO(mukai): Remove this method and really simplify the ownership of
  // SearchResult. Ideally, SearchResult will be copyable.
  virtual std::unique_ptr<ChromeSearchResult> Duplicate() const = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeSearchResult);
};

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_CHROME_SEARCH_RESULT_H_
