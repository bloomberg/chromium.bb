// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ACTION_BOX_BUTTON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ACTION_BOX_BUTTON_VIEW_H_

#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"

class Browser;
class Profile;

// ActionBoxButtonView displays a plus button with associated menu.
class ActionBoxButtonView : public views::MenuButton,
                            public views::MenuButtonListener {
 public:
  explicit ActionBoxButtonView(Browser* browser, Profile* profile);
  virtual ~ActionBoxButtonView();

  SkColor GetBackgroundColor();
  SkColor GetBorderColor();

  void set_starred(bool starred) { starred_ = starred; }

 private:
  // CustomButton
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

  // MenuButtonListener
  virtual void OnMenuButtonClicked(View* source,
                                   const gfx::Point& point) OVERRIDE;

  Browser* browser_;
  Profile* profile_;

  // Set to true when the current page is bookmarked.
  // To be passed to action box menu when this button is pressed.
  bool starred_;

  DISALLOW_COPY_AND_ASSIGN(ActionBoxButtonView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ACTION_BOX_BUTTON_VIEW_H_
