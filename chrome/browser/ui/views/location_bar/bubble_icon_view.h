// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_BUBBLE_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_BUBBLE_ICON_VIEW_H_

#include "ui/views/controls/image_view.h"

class CommandUpdater;

// Represents an icon on the omnibox that shows a bubble when clicked.
class BubbleIconView : public views::ImageView {
 protected:
  enum ExecuteSource {
    EXECUTE_SOURCE_MOUSE,
    EXECUTE_SOURCE_KEYBOARD,
    EXECUTE_SOURCE_GESTURE,
  };

  explicit BubbleIconView(CommandUpdater* command_updater, int command_id);
  virtual ~BubbleIconView();

  // Returns true if a related bubble is showing.
  virtual bool IsBubbleShowing() const = 0;

  // Invoked prior to executing the command.
  virtual void OnExecuting(ExecuteSource execute_source) = 0;

  // views::ImageView:
  virtual void GetAccessibleState(ui::AXViewState* state) OVERRIDE;
  virtual bool GetTooltipText(const gfx::Point& p, base::string16* tooltip)
      const OVERRIDE;
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE;

  // ui::EventHandler:
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

 private:
  // The CommandUpdater for the Browser object that owns the location bar.
  CommandUpdater* command_updater_;

  // The command ID executed when the user clicks this icon.
  const int command_id_;

  // This is used to check if the bookmark bubble was showing during the mouse
  // pressed event. If this is true then the mouse released event is ignored to
  // prevent the bubble from reshowing.
  bool suppress_mouse_released_action_;

  DISALLOW_COPY_AND_ASSIGN(BubbleIconView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_BUBBLE_ICON_VIEW_H_
