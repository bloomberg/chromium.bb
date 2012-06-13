// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"

#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/printing/background_printing_manager.h"

namespace {

printing::BackgroundPrintingManager* GetBackgroundPrintingManager() {
  return g_browser_process->background_printing_manager();
}

}  // namespace

TabContentsIterator::TabContentsIterator()
    : browser_iterator_(BrowserList::begin()),
      web_view_index_(-1),
      bg_printing_iterator_(GetBackgroundPrintingManager()->begin()),
      cur_(NULL) {
  Advance();
}

void TabContentsIterator::Advance() {
  // The current TabContents should be valid unless we are at the beginning.
  DCHECK(cur_ || (web_view_index_ == -1 &&
                  browser_iterator_ == BrowserList::begin()))
      << "Trying to advance past the end";

  // Update cur_ to the next TabContents in the list.
  while (browser_iterator_ != BrowserList::end()) {
    if (++web_view_index_ >= (*browser_iterator_)->tab_count()) {
      // Advance to the next Browser in the list.
      ++browser_iterator_;
      web_view_index_ = -1;
      continue;
    }

    TabContents* next_tab =
        (*browser_iterator_)->GetTabContentsAt(web_view_index_);
    if (next_tab) {
      cur_ = next_tab;
      return;
    }
  }
  // If no more TabContents from Browsers, check the BackgroundPrintingManager.
  while (bg_printing_iterator_ != GetBackgroundPrintingManager()->end()) {
    cur_ = *bg_printing_iterator_;
    CHECK(cur_);
    ++bg_printing_iterator_;
    return;
  }
  // Reached the end - no more TabContents.
  cur_ = NULL;
}
