// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_iterator.h"

namespace chrome {

BrowserIterator::BrowserIterator()
    : current_browser_list_(
          BrowserListImpl::GetInstance(HOST_DESKTOP_TYPE_FIRST)),
      current_iterator_(current_browser_list_->begin()),
      next_desktop_type_(
          static_cast<HostDesktopType>(HOST_DESKTOP_TYPE_FIRST + 1)) {
  NextBrowserListIfAtEnd();
}

void BrowserIterator::Next() {
  ++current_iterator_;
  NextBrowserListIfAtEnd();
}

void BrowserIterator::NextBrowserListIfAtEnd() {
  // Make sure either |current_iterator_| is valid or done().
  while (current_iterator_ == current_browser_list_->end() &&
         next_desktop_type_ < HOST_DESKTOP_TYPE_COUNT) {
    current_browser_list_ = BrowserListImpl::GetInstance(next_desktop_type_);
    current_iterator_ = current_browser_list_->begin();
    next_desktop_type_ = static_cast<HostDesktopType>(next_desktop_type_ + 1);
  }
}

}  // namespace chrome
