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
      desktop_browser_list_(chrome::BrowserListImpl::GetInstance(
          chrome::HOST_DESKTOP_TYPE_NATIVE)),
      ash_browser_list_(chrome::BrowserListImpl::GetInstance(
          chrome::HOST_DESKTOP_TYPE_ASH)),
      desktop_browser_iterator_(desktop_browser_list_->begin()),
      ash_browser_iterator_(ash_browser_list_->begin()) {
  Advance();
}

Browser* TabContentsIterator::browser() const {
  if (desktop_browser_iterator_ != desktop_browser_list_->end())
    return *desktop_browser_iterator_;
  // In some cases like Chrome OS the two browser lists are the same.
  if (desktop_browser_list_ != ash_browser_list_) {
    if (ash_browser_iterator_ != ash_browser_list_->end())
      return *ash_browser_iterator_;
  }
  return NULL;
}

void TabContentsIterator::Advance() {
  // The current WebContents should be valid unless we are at the beginning.
  DCHECK(cur_ || (web_view_index_ == -1 &&
            desktop_browser_iterator_ == desktop_browser_list_->begin() &&
            ash_browser_iterator_ == ash_browser_list_->begin()))
      << "Trying to advance past the end";

  // First iterate over the Browser objects in the desktop environment and then
  // over those in the ASH environment.
  if (AdvanceBrowserIterator(&desktop_browser_iterator_,
                             desktop_browser_list_))
    return;

  // In some cases like Chrome OS the two browser lists are the same.
  if (desktop_browser_list_ != ash_browser_list_)
    AdvanceBrowserIterator(&ash_browser_iterator_, ash_browser_list_);
}

bool TabContentsIterator::AdvanceBrowserIterator(
    chrome::BrowserListImpl::const_iterator* list_iterator,
    chrome::BrowserListImpl* browser_list) {
  // Update cur_ to the next WebContents in the list.
  while (*list_iterator != browser_list->end()) {
    if (++web_view_index_ >=
          (**list_iterator)->tab_strip_model()->count()) {
      // Advance to the next Browser in the list.
      ++(*list_iterator);
      web_view_index_ = -1;
      continue;
    }

    content::WebContents* next_tab =
        (**list_iterator)->tab_strip_model()->GetWebContentsAt(
            web_view_index_);
    if (next_tab) {
      cur_ = next_tab;
      return true;
    }
  }
  // Reached the end.
  cur_ = NULL;
  return false;
}
