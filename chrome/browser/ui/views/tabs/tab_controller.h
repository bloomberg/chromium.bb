// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTROLLER_H_

#include "chrome/browser/ui/views/tabs/tab_strip_types.h"

class Tab;

namespace gfx {
class Point;
}
namespace ui {
class ListSelectionModel;
class LocatedEvent;
class MouseEvent;
}
namespace views {
class View;
}

// Controller for tabs.
class TabController {
 public:
  virtual const ui::ListSelectionModel& GetSelectionModel() = 0;

  // Returns true if multiple selection is supported.
  virtual bool SupportsMultipleSelection() = 0;

  // Selects the tab.
  virtual void SelectTab(Tab* tab) = 0;

  // Extends the selection from the anchor to |tab|.
  virtual void ExtendSelectionTo(Tab* tab) = 0;

  // Toggles whether |tab| is selected.
  virtual void ToggleSelected(Tab* tab) = 0;

  // Adds the selection from the anchor to |tab|.
  virtual void AddSelectionFromAnchorTo(Tab* tab) = 0;

  // Closes the tab.
  virtual void CloseTab(Tab* tab, CloseTabSource source) = 0;

  // Shows a context menu for the tab at the specified point in screen coords.
  virtual void ShowContextMenuForTab(Tab* tab,
                                     const gfx::Point& p,
                                     ui::MenuSourceType source_type) = 0;

  // Returns true if |tab| is the active tab. The active tab is the one whose
  // content is shown in the browser.
  virtual bool IsActiveTab(const Tab* tab) const = 0;

  // Returns true if the specified Tab is selected.
  virtual bool IsTabSelected(const Tab* tab) const = 0;

  // Returns true if the specified Tab is pinned.
  virtual bool IsTabPinned(const Tab* tab) const = 0;

  // Potentially starts a drag for the specified Tab.
  virtual void MaybeStartDrag(
      Tab* tab,
      const ui::LocatedEvent& event,
      const ui::ListSelectionModel& original_selection) = 0;

  // Continues dragging a Tab.
  virtual void ContinueDrag(views::View* view,
                            const ui::LocatedEvent& event) = 0;

  // Ends dragging a Tab. Returns whether the tab has been destroyed.
  virtual bool EndDrag(EndDragReason reason) = 0;

  // Returns the tab that contains the specified coordinates, in terms of |tab|,
  // or NULL if there is no tab that contains the specified point.
  virtual Tab* GetTabAt(Tab* tab,
                        const gfx::Point& tab_in_tab_coordinates) = 0;

  // Invoked when a mouse event occurs on |source|.
  virtual void OnMouseEventInTab(views::View* source,
                                 const ui::MouseEvent& event) = 0;

  // Returns true if |tab| needs to be painted. If false is returned the tab is
  // not painted. If true is returned the tab should be painted and |clip| is
  // set to the clip (if |clip| is empty means no clip).
  virtual bool ShouldPaintTab(const Tab* tab, gfx::Rect* clip) = 0;

  // Returns true if tabs painted in the rectangular light-bar style.
  virtual bool IsImmersiveStyle() const = 0;

 protected:
  virtual ~TabController() {}
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_CONTROLLER_H_
