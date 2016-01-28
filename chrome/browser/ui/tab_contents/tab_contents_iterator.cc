// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"

TabContentsIterator::TabContentsIterator()
    : web_view_index_(-1),
      cur_(NULL),
      browser_iterator_(BrowserList::GetInstance()->begin()) {
  // Load the first WebContents into |cur_|.
  Next();
}

TabContentsIterator::~TabContentsIterator() {
}

void TabContentsIterator::Next() {
  // The current WebContents should be valid unless we are at the beginning.
  DCHECK(cur_ || web_view_index_ == -1) << "Trying to advance past the end";

  // Update |cur_| to the next WebContents in the list.
  while (browser_iterator_ != BrowserList::GetInstance()->end()) {
    if (++web_view_index_ >= (*browser_iterator_)->tab_strip_model()->count()) {
      // Advance to the next Browser in the list.
      ++browser_iterator_;
      web_view_index_ = -1;
      continue;
    }

    content::WebContents* next_tab = (*browser_iterator_)->tab_strip_model()
        ->GetWebContentsAt(web_view_index_);
    if (next_tab) {
      cur_ = next_tab;
      return;
    }
  }
  // Reached the end.
  cur_ = NULL;
}
