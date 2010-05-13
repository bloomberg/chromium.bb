// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_BASE_TAB_STRIP_H_
#define CHROME_BROWSER_VIEWS_TABS_BASE_TAB_STRIP_H_

#include "base/scoped_ptr.h"
#include "chrome/browser/views/tabs/base_tab_renderer.h"
#include "views/view.h"

class TabStrip;
class TabStripController;
class ThemeProvider;

// Base class for the view tab strip implementations.
class BaseTabStrip : public views::View,
                     public TabController {
 public:
  explicit BaseTabStrip(TabStripController* controller);
  virtual ~BaseTabStrip();

  // Returns the preferred height of this TabStrip. This is based on the
  // typical height of its constituent tabs.
  virtual int GetPreferredHeight() = 0;

  // Set the background offset used by inactive tabs to match the frame image.
  virtual void SetBackgroundOffset(const gfx::Point& offset) = 0;

  // Returns true if the specified point(TabStrip coordinates) is
  // in the window caption area of the browser window.
  virtual bool IsPositionInWindowCaption(const gfx::Point& point) = 0;

  // Sets the bounds of the tab at the specified |tab_index|. |tab_bounds| are
  // in TabStrip coordinates.
  virtual void SetDraggedTabBounds(int tab_index,
                                   const gfx::Rect& tab_bounds) = 0;

  // Returns true if a drag session is currently active.
  virtual bool IsDragSessionActive() const = 0;

  // Updates the loading animations displayed by tabs in the tabstrip to the
  // next frame.
  void UpdateLoadingAnimations();

  // Returns true if Tabs in this TabStrip are currently changing size or
  // position.
  virtual bool IsAnimating() const = 0;

  // Returns this object as a TabStrip if it is one.
  virtual TabStrip* AsTabStrip() = 0;

  // Starts highlighting the tab at the specified index.
  virtual void StartHighlight(int model_index) = 0;

  // Stops all tab higlighting.
  virtual void StopAllHighlighting() = 0;

  // Returns the tab at the specified model index.
  virtual BaseTabRenderer* GetBaseTabAtModelIndex(int model_index) const = 0;

  // Returns the tab at the specified tab index.
  virtual BaseTabRenderer* GetBaseTabAtTabIndex(int tab_index) const = 0;

  // Returns the index of the specified tab in the model coordiate system, or
  // -1 if tab is closing or not valid.
  virtual int GetModelIndexOfBaseTab(const BaseTabRenderer* tab) const = 0;

  // Returns the number of tabs.
  virtual int GetTabCount() const = 0;

  // Returns the selected tab.
  virtual BaseTabRenderer* GetSelectedBaseTab() const;

  // Creates and returns a tab that can be used for dragging. Ownership passes
  // to the caller.
  virtual BaseTabRenderer* CreateTabForDragging() = 0;

  // Adds a tab at the specified index.
  virtual void AddTabAt(int model_index,
                        bool foreground,
                        const TabRendererData& data) = 0;

  // Removes a tab at the specified index.
  virtual void RemoveTabAt(int model_index) = 0;

  // Selects a tab at the specified index. |old_model_index| is the selected
  // index prior to the selection change.
  virtual void SelectTabAt(int old_model_index, int new_model_index) = 0;

  // Moves a tab.
  virtual void MoveTab(int from_model_index, int to_model_index) = 0;

  // Invoked when the title of a tab changes and the tab isn't loading.
  virtual void TabTitleChangedNotLoading(int model_index) = 0;

  // Sets the tab data at the specified model index.
  virtual void SetTabData(int model_index, const TabRendererData& data) = 0;

  // Cover methods for TabStripController method.
  int GetModelCount() const;
  bool IsValidModelIndex(int model_index) const;

  // TabController overrides:
  virtual void SelectTab(BaseTabRenderer* tab);
  virtual void CloseTab(BaseTabRenderer* tab);
  virtual void ShowContextMenu(BaseTabRenderer* tab, const gfx::Point& p);
  virtual bool IsTabSelected(const BaseTabRenderer* tab) const;
  virtual bool IsTabPinned(const BaseTabRenderer* tab) const;
  virtual void MaybeStartDrag(BaseTabRenderer* tab,
                              const views::MouseEvent& event) = 0;
  virtual void ContinueDrag(const views::MouseEvent& event) = 0;
  virtual bool EndDrag(bool canceled) = 0;

  TabStripController* controller() const { return controller_.get(); }

 private:
  scoped_ptr<TabStripController> controller_;
};

#endif  // CHROME_BROWSER_VIEWS_TABS_BASE_TAB_STRIP_H_
