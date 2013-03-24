// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ACTION_BOX_BUTTON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ACTION_BOX_BUTTON_VIEW_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/toolbar/action_box_button_controller.h"
#include "chrome/browser/ui/views/location_bar/touchable_location_bar_view.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"

class ActionBoxMenu;
class Browser;

// ActionBoxButtonView displays a plus button with associated menu.
class ActionBoxButtonView : public views::MenuButton,
                            public views::MenuButtonListener,
                            public ActionBoxButtonController::Delegate,
                            public TouchableLocationBarView {
 public:
  ActionBoxButtonView(Browser* browser, const gfx::Point& menu_offset);
  virtual ~ActionBoxButtonView();

  ActionBoxButtonController* action_box_button_controller() {
   return &controller_;
  }

  // TouchableLocationBarView:
  virtual int GetBuiltInHorizontalPadding() const OVERRIDE;

 private:
  // Overridden from views::CustomButton:
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

  // Overridden from views::MenuButtonListener:
  virtual void OnMenuButtonClicked(View* source,
                                   const gfx::Point& point) OVERRIDE;

  // Overridden from ActionBoxButtonController::Delegate:
  virtual void ShowMenu(scoped_ptr<ActionBoxMenuModel> menu_model) OVERRIDE;

  Browser* browser_;

  gfx::Point menu_offset_;

  ActionBoxButtonController controller_;

  scoped_ptr<ActionBoxMenu> menu_;

  DISALLOW_COPY_AND_ASSIGN(ActionBoxButtonView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ACTION_BOX_BUTTON_VIEW_H_
