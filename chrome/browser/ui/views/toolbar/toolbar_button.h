// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_BUTTON_H_

#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/label_button.h"

namespace ui {
class MenuModel;
}

namespace views {
class MenuRunner;
}

// This class provides basic drawing and mouse-over behavior for buttons
// appearing in the toolbar.
class ToolbarButton : public views::LabelButton,
                      public views::ContextMenuController {
 public:
  // Takes ownership of the |model|, which can be null if no menu
  // is to be shown.
  ToolbarButton(views::ButtonListener* listener, ui::MenuModel* model);
  virtual ~ToolbarButton();

  // Set up basic mouseover border behavior.
  // Should be called before first paint.
  void Init();

  // Methods for handling ButtonDropDown-style menus.
  void ClearPendingMenu();
  bool IsMenuShowing() const;

  // views::LabelButton:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  // Showing the drop down results in a MouseCaptureLost, we need to ignore it.
  virtual void OnMouseCaptureLost() OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;
  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE;
  virtual scoped_ptr<views::LabelButtonBorder> CreateDefaultBorder() const
      OVERRIDE;

  // views::ContextMenuController:
  virtual void ShowContextMenuForView(View* source,
                                      const gfx::Point& point,
                                      ui::MenuSourceType source_type) OVERRIDE;

 protected:
  // Overridden from CustomButton. Returns true if the button should become
  // pressed when a user holds the mouse down over the button. For this
  // implementation, both left and right mouse buttons can trigger a change
  // to the PUSHED state.
  virtual bool ShouldEnterPushedState(const ui::Event& event) OVERRIDE;

  // Returns if menu should be shown. Override this to change default behavior.
  virtual bool ShouldShowMenu();

  // Function to show the dropdown menu.
  virtual void ShowDropDownMenu(ui::MenuSourceType source_type);

 private:
  // The model that populates the attached menu.
  scoped_ptr<ui::MenuModel> model_;

  // Indicates if menu is currently showing.
  bool menu_showing_;

  // Y position of mouse when left mouse button is pressed
  int y_position_on_lbuttondown_;

  // Menu runner to display drop down menu.
  scoped_ptr<views::MenuRunner> menu_runner_;

  // A factory for tasks that show the dropdown context menu for the button.
  base::WeakPtrFactory<ToolbarButton> show_menu_factory_;

  DISALLOW_COPY_AND_ASSIGN(ToolbarButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_TOOLBAR_BUTTON_H_
