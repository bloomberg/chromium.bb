// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTROLLER_H_
#pragma once

class BaseTab;

namespace gfx {
class Point;
}
namespace views {
class MouseEvent;
}

// Controller for tabs.
class TabController {
 public:
  // Selects the tab.
  virtual void SelectTab(BaseTab* tab) = 0;

  // Closes the tab.
  virtual void CloseTab(BaseTab* tab) = 0;

  // Shows a context menu for the tab at the specified point in screen coords.
  virtual void ShowContextMenuForTab(BaseTab* tab, const gfx::Point& p) = 0;

  // Returns true if the specified Tab is selected.
  virtual bool IsTabSelected(const BaseTab* tab) const = 0;

  // Returns true if the specified Tab is pinned.
  virtual bool IsTabPinned(const BaseTab* tab) const = 0;

  // Returns true if the specified Tab is closeable.
  virtual bool IsTabCloseable(const BaseTab* tab) const = 0;

  // Potentially starts a drag for the specified Tab.
  virtual void MaybeStartDrag(BaseTab* tab, const views::MouseEvent& event) = 0;

  // Continues dragging a Tab.
  virtual void ContinueDrag(const views::MouseEvent& event) = 0;

  // Ends dragging a Tab. |canceled| is true if the drag was aborted in a way
  // other than the user releasing the mouse. Returns whether the tab has been
  // destroyed.
  virtual bool EndDrag(bool canceled) = 0;

  // Returns the tab that contains the specified coordinates, in terms of |tab|,
  // or NULL if there is no tab that contains the specified point.
  virtual BaseTab* GetTabAt(BaseTab* tab,
                            const gfx::Point& tab_in_tab_coordinates) = 0;

 protected:
  virtual ~TabController() {}
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTROLLER_H_
