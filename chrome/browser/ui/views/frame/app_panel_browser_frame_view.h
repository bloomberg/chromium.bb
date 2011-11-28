// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_APP_PANEL_BROWSER_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_APP_PANEL_BROWSER_FRAME_VIEW_H_
#pragma once

#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/tab_icon_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/window/non_client_view.h"

class BrowserView;
namespace gfx {
class Font;
}
namespace views {
class ImageButton;
}
// The frame view which is used for Application Panels.
// TODO(rafaelw): Refactor. This shares much duplicated code with
// OpaqueBrowserFrameView.
class AppPanelBrowserFrameView : public BrowserNonClientFrameView,
                                 public views::ButtonListener,
                                 public TabIconView::TabIconViewModel {
 public:
  // Constructs a non-client view for an BrowserFrame.
  AppPanelBrowserFrameView(BrowserFrame* frame, BrowserView* browser_view);
  virtual ~AppPanelBrowserFrameView();

  // Overridden from BrowserNonClientFrameView:
  virtual gfx::Rect GetBoundsForTabStrip(views::View* tabstrip) const OVERRIDE;
  virtual int GetHorizontalTabStripVerticalOffset(bool restored) const OVERRIDE;
  virtual void UpdateThrobber(bool running) OVERRIDE;
  virtual gfx::Size GetMinimumSize() OVERRIDE;

 protected:
  // Overridden from views::NonClientFrameView:
  virtual gfx::Rect GetBoundsForClientView() const OVERRIDE;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const OVERRIDE;
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE;
  virtual void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask)
      OVERRIDE;
  virtual void ResetWindowControls() OVERRIDE;
  virtual void UpdateWindowIcon() OVERRIDE;

  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void Layout() OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event)
      OVERRIDE;

  // Overridden from TabIconView::TabIconViewModel:
  virtual bool ShouldTabIconViewAnimate() const OVERRIDE;
  virtual SkBitmap GetFaviconForTabIconView() OVERRIDE;

 private:
  // Returns the thickness of the border that makes up the window frame edges.
  // This does not include any client edge.
  int FrameBorderThickness() const;

  // Returns the thickness of the entire nonclient left, right, and bottom
  // borders, including both the window frame and any client edge.
  int NonClientBorderThickness() const;

  // Returns the height of the entire nonclient top border, including the window
  // frame, any title area, and any connected client edge.
  int NonClientTopBorderHeight() const;

  // Returns the thickness of the nonclient portion of the 3D edge along the
  // bottom of the titlebar.
  int TitlebarBottomThickness() const;

  // Returns the size of the titlebar icon.
  int IconSize() const;

  // Returns the bounds of the titlebar icon.
  gfx::Rect IconBounds() const;

  // Paint various sub-components of this view.  The *FrameBorder() function
  // also paints the background of the titlebar area, since the top frame border
  // and titlebar background are a contiguous component.
  void PaintRestoredFrameBorder(gfx::Canvas* canvas);
  void PaintMaximizedFrameBorder(gfx::Canvas* canvas);
  void PaintTitleBar(gfx::Canvas* canvas);
  void PaintRestoredClientEdge(gfx::Canvas* canvas);

  // Layout various sub-components of this view.
  void LayoutWindowControls();
  void LayoutTitleBar();

  // Returns the bounds of the client area for the specified view size.
  gfx::Rect CalculateClientAreaBounds(int width, int height) const;

  // The layout rect of the title, if visible.
  gfx::Rect title_bounds_;

  // Window controls.
  views::ImageButton* close_button_;

  // The Window icon.
  TabIconView* window_icon_;

  // The bounds of the ClientView.
  gfx::Rect client_view_bounds_;

  // The accessible name of this view.
  std::wstring accessible_name_;

  static void InitAppWindowResources();
  static gfx::Font* title_font_;

  DISALLOW_COPY_AND_ASSIGN(AppPanelBrowserFrameView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_APP_PANEL_BROWSER_FRAME_VIEW_H_
