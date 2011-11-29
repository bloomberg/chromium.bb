// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_DRAG_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_DRAG_CONTROLLER_H_
#pragma once

#include <vector>

class BaseTabStrip;
class BaseTab;
class TabStripSelectionModel;

namespace gfx {
class Point;
}

///////////////////////////////////////////////////////////////////////////////
//
// TabDragController
//
//  An object that handles a drag session for a set of Tabs within a
//  TabStrip. This object is created whenever the mouse is pressed down on a
//  Tab and destroyed when the mouse is released or the drag operation is
//  aborted. The TabStrip that the drag originated from owns this object.
//
///////////////////////////////////////////////////////////////////////////////
class TabDragController {
 public:
  virtual ~TabDragController() {}

  // Creates and returns a new TabDragController to drag the tabs in |tabs|
  // originating from |source_tabstrip|. |source_tab| is the tab that initiated
  // the drag (the one the user pressed the moused down on) and is contained in
  // |tabs|.  |mouse_offset| is the distance of the mouse pointer from the
  // origin of the first tab in |tabs| and |source_tab_offset| the offset from
  // |source_tab|. |source_tab_offset| is the horizontal distant for a
  // horizontal tab strip, and the vertical distance for a vertical tab
  // strip. |initial_selection_model| is the selection model before the drag
  // started and is only non-empty if |source_tab| was not initially selected.
  static TabDragController* Create(
      BaseTabStrip* source_tabstrip,
      BaseTab* source_tab,
      const std::vector<BaseTab*>& tabs,
      const gfx::Point& mouse_offset,
      int source_tab_offset,
      const TabStripSelectionModel& initial_selection_model);

  // Returns true if there is a drag underway and the drag is attached to
  // |tab_strip|.
  // NOTE: this returns false if the TabDragController is in the process of
  // finishing the drag.
  static bool IsAttachedTo(BaseTabStrip* tab_strip);

  // Responds to drag events subsequent to StartDrag. If the mouse moves a
  // sufficient distance before the mouse is released, a drag session is
  // initiated.
  virtual void Drag() = 0;

  // Complete the current drag session. If the drag session was canceled
  // because the user pressed escape or something interrupted it, |canceled|
  // is true so the helper can revert the state to the world before the drag
  // begun.
  virtual void EndDrag(bool canceled) = 0;

  // Returns true if a drag started.
  virtual bool GetStartedDrag() const = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_DRAG_CONTROLLER_H_
