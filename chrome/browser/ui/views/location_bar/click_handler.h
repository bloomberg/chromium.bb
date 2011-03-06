// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CLICK_HANDLER_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CLICK_HANDLER_H_
#pragma once

#include "base/basictypes.h"

class LocationBarView;

namespace views {
class MouseEvent;
class View;
}

// This helper class is kept as a member by classes that need to show the Page
// Info dialog on click, to encapsulate that logic in one place.
class ClickHandler {
 public:
  ClickHandler(const views::View* owner, LocationBarView* location_bar);

  void OnMouseReleased(const views::MouseEvent& event, bool canceled);

 private:
  const views::View* owner_;
  LocationBarView* location_bar_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ClickHandler);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_CLICK_HANDLER_H_

