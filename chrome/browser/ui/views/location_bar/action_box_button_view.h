// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ACTION_BOX_BUTTON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ACTION_BOX_BUTTON_VIEW_H_
#pragma once

#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"

// ActionBoxButtonView displays a plus button with assosiated menu.
class ActionBoxButtonView : public views::MenuButton,
                            public views::MenuButtonListener {
 public:
  ActionBoxButtonView();
  virtual ~ActionBoxButtonView();

 private:
  void SetImages();

  // CustomButton
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

  // MenuButtonListener
  virtual void OnMenuButtonClicked(View* source,
                                   const gfx::Point& point) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ActionBoxButtonView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ACTION_BOX_BUTTON_VIEW_H_
