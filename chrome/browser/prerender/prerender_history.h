// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRERENDER_PRERENDER_HISTORY_H_
#define CHROME_BROWSER_PRERENDER_PRERENDER_HISTORY_H_
#pragma once

#include <list>

#include "base/threading/non_thread_safe.h"
#include "chrome/browser/prerender/prerender_final_status.h"
#include "googleurl/src/gurl.h"

class Value;

namespace prerender {

// PrerenderHistory maintains a per-session history of prerendered pages
// and their final dispositions. It has a fixed maximum capacity, and old
// items in history will be removed when the capacity is reached.
class PrerenderHistory : public base::NonThreadSafe {
 public:
  // Entry is an individual entry in the history list. It corresponds to a
  // specific prerendered page.
  struct Entry {
    Entry() : final_status(FINAL_STATUS_MAX) {}

    Entry(const GURL& url_arg, FinalStatus final_status_arg)
        : url(url_arg), final_status(final_status_arg) {
    }

    // The URL which was prerendered. This should be the URL included in the
    // <link rel="prerender"> tag, and not any URLs which it may have redirected
    // to.
    GURL url;

    // The FinalStatus describing whether the prerendered page was used or why
    // it was cancelled.
    FinalStatus final_status;
  };

  // Creates a history with capacity for |max_items| entries.
  explicit PrerenderHistory(size_t max_items);
  ~PrerenderHistory();

  // Adds |entry| to the history. If at capacity, the oldest entry is dropped.
  void AddEntry(const Entry& entry);

  // Retrieves the entries as a value which can be displayed.
  Value* GetEntriesAsValue() const;

 private:
  std::list<Entry> entries_;
  size_t max_items_;

  DISALLOW_COPY_AND_ASSIGN(PrerenderHistory);
};

}
#endif  // CHROME_BROWSER_PRERENDER_PRERENDER_HISTORY_H_
