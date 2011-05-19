// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_FRAME_VIEW_H_
#pragma once

#include "base/gtest_prod_util.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/tab_icon_view.h"
#include "views/controls/button/button.h"

class PanelBrowserView;
namespace views {
class ImageButton;
class Label;
}

class PanelBrowserFrameView : public BrowserNonClientFrameView,
                              public views::ButtonListener,
                              public TabIconView::TabIconViewModel {
 public:
  PanelBrowserFrameView(BrowserFrame* frame, PanelBrowserView* browser_view);
  virtual ~PanelBrowserFrameView();

  void UpdateTitleBar();
  void OnActivationChanged(bool active);

 protected:
  // Overridden from BrowserNonClientFrameView:
  virtual gfx::Rect GetBoundsForTabStrip(views::View* tabstrip) const OVERRIDE;
  virtual int GetHorizontalTabStripVerticalOffset(bool restored) const OVERRIDE;
  virtual void UpdateThrobber(bool running) OVERRIDE;

  // Overridden from views::NonClientFrameView:
  virtual gfx::Rect GetBoundsForClientView() const OVERRIDE;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const OVERRIDE;
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE;
  virtual void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask)
      OVERRIDE;
  virtual void EnableClose(bool enable) OVERRIDE;
  virtual void ResetWindowControls() OVERRIDE;
  virtual void UpdateWindowIcon() OVERRIDE;

  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnThemeChanged() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;
  virtual bool OnMousePressed(const views::MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const views::MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event)
      OVERRIDE;

  // Overridden from TabIconView::TabIconViewModel:
  virtual bool ShouldTabIconViewAnimate() const OVERRIDE;
  virtual SkBitmap GetFaviconForTabIconView() OVERRIDE;

 private:
  friend class PanelBrowserViewTest;
  FRIEND_TEST_ALL_PREFIXES(PanelBrowserViewTest, CreatePanel);

  enum PaintState {
    NOT_PAINTED,
    PAINT_AS_INACTIVE,
    PAINT_AS_ACTIVE
  };

  // Returns the thickness of the entire nonclient left, right, and bottom
  // borders, including both the window frame and any client edge.
  int NonClientBorderThickness() const;

  // Returns the height of the entire nonclient top border, including the window
  // frame, any title area, and any connected client edge.
  int NonClientTopBorderHeight() const;

  // Update control styles to indicate if the title bar is active or not.
  void UpdateControlStyles(PaintState paint_state);

  // Custom draw the frame.
  void PaintFrameBorder(gfx::Canvas* canvas);
  void PaintClientEdge(gfx::Canvas* canvas);

  // The frame that hosts this view. This is a weak reference such that frame_
  // will always be valid in the lifetime of this view.
  BrowserFrame* frame_;

  // The client view hosted within this non-client frame view that is
  // guaranteed to be freed before the client view.
  // (see comments about view hierarchies in non_client_view.h)
  PanelBrowserView* browser_view_;

  PaintState paint_state_;
  views::ImageButton* info_button_;
  views::ImageButton* close_button_;
  TabIconView* title_icon_;
  views::Label* title_label_;
  gfx::Rect client_view_bounds_;

  DISALLOW_COPY_AND_ASSIGN(PanelBrowserFrameView);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_FRAME_VIEW_H_
