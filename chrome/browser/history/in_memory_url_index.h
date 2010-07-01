// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_IN_MEMORY_URL_INDEX_H_
#define CHROME_BROWSER_HISTORY_IN_MEMORY_URL_INDEX_H_

namespace history {

// The URL history source.
// Holds portions of the URL database in memory in an indexed form.  Used to
// quickly look up matching URLs for a given query string.  Used by
// the HistoryURLProvider for inline autocomplete and to provide URL
// matches to the omnibox.
class InMemoryURLIndex {
 public:
  InMemoryURLIndex();
  ~InMemoryURLIndex();
};

}  // namespace history

#endif  // CHROME_BROWSER_HISTORY_IN_MEMORY_URL_INDEX_H_
