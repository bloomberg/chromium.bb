// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/search_provider.h"

#include "chrome/browser/ui/app_list/search/chrome_search_result.h"
#include "ui/app_list/search_result.h"

namespace app_list {

SearchProvider::SearchProvider() {}
SearchProvider::~SearchProvider() {}

void SearchProvider::Add(scoped_ptr<ChromeSearchResult> result) {
  results_.push_back(result.release());
  FireResultChanged();
}

void SearchProvider::ClearResults() {
  results_.clear();
  FireResultChanged();
}

void SearchProvider::FireResultChanged() {
  if (result_changed_callback_.is_null())
    return;

  result_changed_callback_.Run();
}

}  // namespace app_list
