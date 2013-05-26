// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APP_LIST_SEARCH_HISTORY_TYPES_H_
#define CHROME_BROWSER_UI_APP_LIST_SEARCH_HISTORY_TYPES_H_

#include <map>
#include <string>

namespace app_list {

// An enum that indicates how a search result id matches a query in history.
enum KnownResultType {
  UNKNOWN_RESULT = 0,
  PERFECT_PRIMARY,    // Exactly the same query and in primary association
  PERFECT_SECONDARY,  // Exactly the same query and in secondary association
  PREFIX_PRIMARY,     // Query is a prefix and in primary association
  PREFIX_SECONDARY,   // Query is a prefix and in secondary association
};

// KnownResults maps a result id to a KnownResultType.
typedef std::map<std::string, KnownResultType> KnownResults;

}  // namespace app_list

#endif  // CHROME_BROWSER_UI_APP_LIST_SEARCH_HISTORY_TYPES_H_
