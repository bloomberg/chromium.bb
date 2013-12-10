// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_ITERATOR_H_
#define CHROME_BROWSER_UI_BROWSER_ITERATOR_H_

#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/host_desktop.h"

class Browser;

namespace chrome {

// Iterates over all existing browsers (potentially across multiple desktops).
// Note: to iterate only over the browsers of a specific desktop, use the
// const_iterator of a given BrowserList instead.
//
// Example:
//   for (BrowserIterator iterator; !iterator.done(); iterator.Next()) {
//     Browser* cur = *iterator;
//     -or-
//     iterator->OperationOnBrowser();
//     ...
//   }
class BrowserIterator {
 public:
  BrowserIterator();
  ~BrowserIterator();

  // Returns true if this iterator is past the last Browser.
  bool done() const {
    // |current_iterator_| is never at the end of a list unless it is done (it
    // immediately moves to the next browser list upon hitting the end of the
    // current list unless there are no remaining empty browser lists).
    return current_iterator_ == current_browser_list_->end();
  }

  // Returns the current Browser, valid as long as !done().
  Browser* operator->() const {
    return *current_iterator_;
  }
  Browser* operator*() const {
    return *current_iterator_;
  }

  // Advances |current_iterator_| to the next browser.
  void Next();

 private:
  // If |current_iterator_| is at |current_browser_list_->end()|, advance to the
  // next non-empty browser list. After a call to this method: either
  // |current_iterator_| is valid or done().
  void NextBrowserListIfAtEnd();

  // The BrowserList currently being iterated over. Instances of this class do
  // not own this pointer.
  BrowserList* current_browser_list_;

  // The underlying iterator over browsers in |current_browser_list_|.
  BrowserList::const_iterator current_iterator_;

  // The next HostDesktopType to iterate over when |current_iterator_| reaches
  // |current_browser_list_->end()|.
  HostDesktopType next_desktop_type_;

  DISALLOW_COPY_AND_ASSIGN(BrowserIterator);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_ITERATOR_H_
