// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_BASE_TAB_RENDERER_H_
#define CHROME_BROWSER_VIEWS_TABS_BASE_TAB_RENDERER_H_

#include "chrome/browser/views/tabs/tab_renderer_data.h"
#include "views/view.h"

class BaseTabRenderer;

// Controller for tabs.
class TabController {
 public:
  // Selects the tab.
  virtual void SelectTab(BaseTabRenderer* tab) = 0;

  // Closes the tab.
  virtual void CloseTab(BaseTabRenderer* tab) = 0;

  // Shows a context menu for the tab at the specified point in screen coords.
  virtual void ShowContextMenu(BaseTabRenderer* tab, const gfx::Point& p) = 0;

  // Returns true if the specified Tab is selected.
  virtual bool IsTabSelected(const BaseTabRenderer* tab) const = 0;

  // Returns true if the specified Tab is pinned.
  virtual bool IsTabPinned(const BaseTabRenderer* tab) const = 0;

  // Potentially starts a drag for the specified Tab.
  virtual void MaybeStartDrag(BaseTabRenderer* tab,
                              const views::MouseEvent& event) = 0;

  // Continues dragging a Tab.
  virtual void ContinueDrag(const views::MouseEvent& event) = 0;

  // Ends dragging a Tab. |canceled| is true if the drag was aborted in a way
  // other than the user releasing the mouse. Returns whether the tab has been
  // destroyed.
  virtual bool EndDrag(bool canceled) = 0;

 protected:
  virtual ~TabController() {}
};

// Base class for tab renderers.
class BaseTabRenderer : public views::View {
 public:
  explicit BaseTabRenderer(TabController* controller);

  // Sets the data this tabs displays. Invokes DataChanged for subclasses to
  // update themselves appropriately.
  void SetData(const TabRendererData& data);
  const TabRendererData& data() const { return data_; }

  // Sets the network state. If the network state changes NetworkStateChanged is
  // invoked.
  virtual void UpdateLoadingAnimation(TabRendererData::NetworkState state);

  // Used to set/check whether this Tab is being animated closed.
  void set_closing(bool closing) { closing_ = closing; }
  bool closing() const { return closing_; }

  // See description above field.
  void set_dragging(bool dragging) { dragging_ = dragging; }
  bool dragging() const { return dragging_; }

 protected:
  // Invoked from SetData after |data_| has been updated to the new data.
  virtual void DataChanged(const TabRendererData& old) {}

  // Invoked if data_.network_state changes, or the network_state is not none.
  virtual void AdvanceLoadingAnimation(TabRendererData::NetworkState state) {}

  TabController* controller() const { return controller_; }

 private:
  // The controller.
  // WARNING: this is null during detached tab dragging.
  TabController* controller_;

  TabRendererData data_;

  // True if the tab is being animated closed.
  bool closing_;

  // True if the tab is being dragged.
  bool dragging_;

  DISALLOW_COPY_AND_ASSIGN(BaseTabRenderer);
};

#endif  // CHROME_BROWSER_VIEWS_TABS_BASE_TAB_RENDERER_H_
