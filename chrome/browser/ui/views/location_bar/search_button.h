// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_SEARCH_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_SEARCH_BUTTON_H_

#include "ui/views/controls/button/label_button.h"

// A blue button shown at the end of the omnibox that holds either a search or
// navigation icon based on the current omnibox text.  This is something like a
// cross between a historical browser "Go" button and the Google "search"
// button.
class SearchButton : public views::LabelButton {
 public:
  explicit SearchButton(views::ButtonListener* listener);
  virtual ~SearchButton();

  // Updates the search button icon to a search icon if |is_search| is true, or
  // a navigation icon otherwise.
  void UpdateIcon(bool is_search);

 private:
  DISALLOW_COPY_AND_ASSIGN(SearchButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_SEARCH_BUTTON_H_
