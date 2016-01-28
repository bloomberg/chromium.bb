// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_iterator.h"

// TODO(scottmg): Remove this file entirely. http://crbug.com/558054.

namespace chrome {

BrowserIterator::BrowserIterator()
    : browser_list_(BrowserList::GetInstance()),
      iterator_(browser_list_->begin()) {
}

BrowserIterator::~BrowserIterator() {
}

void BrowserIterator::Next() {
  ++iterator_;
}

}  // namespace chrome
