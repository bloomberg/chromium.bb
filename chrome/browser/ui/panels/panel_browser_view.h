// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_VIEW_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_VIEW_H_
#pragma once

#include "base/gtest_prod_util.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/base/animation/animation_delegate.h"

class Browser;
class Panel;
class PanelBrowserFrameView;
namespace ui {
class SlideAnimation;
}

// A browser view that implements Panel specific behavior.
class PanelBrowserView : public ::BrowserView,
                         public ui::AnimationDelegate {
 public:
  PanelBrowserView(Browser* browser, Panel* panel);
  virtual ~PanelBrowserView();

  Panel* panel() const { return panel_; }

  // Called from frame view when title bar receives a mouse event.
  // Return true if the event is handled.
  bool OnTitleBarMousePressed(const views::MouseEvent& event);
  bool OnTitleBarMouseDragged(const views::MouseEvent& event);
  bool OnTitleBarMouseReleased(const views::MouseEvent& event);
  bool OnTitleBarMouseCaptureLost();

 private:
  friend class PanelBrowserViewTest;
  FRIEND_TEST_ALL_PREFIXES(PanelBrowserViewTest, CreatePanel);
  FRIEND_TEST_ALL_PREFIXES(PanelBrowserViewTest, ShowOrHideInfoButton);
  FRIEND_TEST_ALL_PREFIXES(PanelBrowserViewTest, SetBoundsAnimation);

  // Overridden from BrowserView:
  virtual void Init() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual void UpdateTitleBar() OVERRIDE;
  virtual bool GetSavedWindowBounds(gfx::Rect* bounds) const OVERRIDE;
  virtual void OnWindowActivationChanged(bool active) OVERRIDE;
  virtual bool AcceleratorPressed(const views::Accelerator& accelerator)
      OVERRIDE;

  // Overridden from AnimationDelegate:
  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE;

  PanelBrowserFrameView* GetFrameView() const;
  bool EndDragging(bool cancelled);

  Panel* panel_;

  // Is the mouse button currently down?
  bool mouse_pressed_;

  // Location the mouse was pressed at. Used to detect drag and drop.
  gfx::Point mouse_pressed_point_;

  // Is the titlebar currently being dragged?  That is, has the cursor
  // moved more than kDragThreshold away from its starting position?
  bool mouse_dragging_;

  // Used to animate the bounds change.
  scoped_ptr<ui::SlideAnimation> bounds_animator_;
  gfx::Rect animation_start_bounds_;
  gfx::Rect animation_target_bounds_;

  DISALLOW_COPY_AND_ASSIGN(PanelBrowserView);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_VIEW_H_
