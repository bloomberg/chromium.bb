// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/test_location_bar.h"

TestLocationBar::TestLocationBar()
    : disposition_(CURRENT_TAB),
      transition_(PageTransition::LINK) {
}

TestLocationBar::~TestLocationBar() {}

std::wstring TestLocationBar::GetInputString() const {
  return input_string_;
}

WindowOpenDisposition TestLocationBar::GetWindowOpenDisposition() const {
  return disposition_;
}

PageTransition::Type TestLocationBar::GetPageTransition() const {
  return transition_;
}

const AutocompleteEditView* TestLocationBar::location_entry() const {
  return NULL;
}

AutocompleteEditView* TestLocationBar::location_entry() {
  return NULL;
}

LocationBarTesting* TestLocationBar::GetLocationBarForTesting() {
  return NULL;
}
