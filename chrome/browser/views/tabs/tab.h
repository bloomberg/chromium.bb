// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_TABS_TAB_H_
#define CHROME_BROWSER_VIEWS_TABS_TAB_H_

#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/views/tabs/tab_renderer.h"

namespace gfx {
class Path;
class Point;
}

///////////////////////////////////////////////////////////////////////////////
//
// Tab
//
//  A subclass of TabRenderer that represents an individual Tab in a TabStrip.
//
///////////////////////////////////////////////////////////////////////////////
class Tab : public TabRenderer,
            public views::ContextMenuController {
 public:
  static const std::string kTabClassName;

  explicit Tab(TabController* controller);
  virtual ~Tab();

  // Used to set/check whether this Tab is being animated closed.
  void set_closing(bool closing) { closing_ = closing; }
  bool closing() const { return closing_; }

  // See description above field.
  void set_dragging(bool dragging) { dragging_ = dragging; }
  bool dragging() const { return dragging_; }

  // TabRenderer overrides:
  virtual bool IsSelected() const;

 private:
  // views::View overrides:
  virtual bool HasHitTestMask() const;
  virtual void GetHitTestMask(gfx::Path* mask) const;
  virtual bool OnMousePressed(const views::MouseEvent& event);
  virtual bool OnMouseDragged(const views::MouseEvent& event);
  virtual void OnMouseReleased(const views::MouseEvent& event,
                               bool canceled);
  virtual bool GetTooltipText(const gfx::Point& p, std::wstring* tooltip);
  virtual bool GetTooltipTextOrigin(const gfx::Point& p, gfx::Point* origin);
  virtual std::string GetClassName() const { return kTabClassName; }
  virtual bool GetAccessibleRole(AccessibilityTypes::Role* role);

  // views::ContextMenuController overrides:
  virtual void ShowContextMenu(views::View* source,
                               const gfx::Point& p,
                               bool is_mouse_gesture);

  // views::ButtonListener overrides:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Creates a path that contains the clickable region of the tab's visual
  // representation. Used by GetViewForPoint for hit-testing.
  void MakePathForTab(gfx::Path* path) const;

  // True if the tab is being animated closed.
  bool closing_;

  // True if the tab is being dragged.
  bool dragging_;

  DISALLOW_COPY_AND_ASSIGN(Tab);
};

#endif  // CHROME_BROWSER_VIEWS_TABS_TAB_H_
