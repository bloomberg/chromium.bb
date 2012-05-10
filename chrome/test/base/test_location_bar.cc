// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/test_location_bar.h"

TestLocationBar::TestLocationBar()
    : disposition_(CURRENT_TAB),
      transition_(content::PAGE_TRANSITION_LINK) {
}

TestLocationBar::~TestLocationBar() {}

string16 TestLocationBar::GetInputString() const {
  return input_string_;
}

WindowOpenDisposition TestLocationBar::GetWindowOpenDisposition() const {
  return disposition_;
}

content::PageTransition TestLocationBar::GetPageTransition() const {
  return transition_;
}

const OmniboxView* TestLocationBar::GetLocationEntry() const {
  return NULL;
}

OmniboxView* TestLocationBar::GetLocationEntry() {
  return NULL;
}

LocationBarTesting* TestLocationBar::GetLocationBarForTesting() {
  return NULL;
}
