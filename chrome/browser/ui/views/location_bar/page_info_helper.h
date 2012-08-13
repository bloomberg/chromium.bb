// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PAGE_INFO_HELPER_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PAGE_INFO_HELPER_H_

#include "base/basictypes.h"

class LocationBarView;

namespace ui {
class LocatedEvent;
}

namespace views {
class View;
}

// This helper class is kept as a member by classes that need to show the Page
// Info dialog on click, to encapsulate that logic in one place.
class PageInfoHelper {
 public:
  PageInfoHelper(const views::View* owner, LocationBarView* location_bar);

  void ProcessEvent(const ui::LocatedEvent& event);

 private:
  const views::View* owner_;
  LocationBarView* location_bar_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(PageInfoHelper);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_PAGE_INFO_HELPER_H_

