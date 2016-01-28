// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BROWSER_ITERATOR_H_
#define CHROME_BROWSER_UI_BROWSER_ITERATOR_H_

#include "base/macros.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/host_desktop.h"

class Browser;

namespace chrome {

// TODO(scottmg): Remove this file entirely. http://crbug.com/558054.

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
    // |iterator_| is never at the end of a list unless it is done (it
    // immediately moves to the next browser list upon hitting the end of the
    // current list unless there are no remaining empty browser lists).
    return iterator_ == browser_list_->end();
  }

  // Returns the current Browser, valid as long as !done().
  Browser* operator->() const { return *iterator_; }
  Browser* operator*() const { return *iterator_; }

  // Advances |iterator_| to the next browser.
  void Next();

 private:
  // The BrowserList being iterated over.
  BrowserList* browser_list_;

  // The underlying iterator over browsers in |browser_list_|.
  BrowserList::const_iterator iterator_;

  DISALLOW_COPY_AND_ASSIGN(BrowserIterator);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_BROWSER_ITERATOR_H_
