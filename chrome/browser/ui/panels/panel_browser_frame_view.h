// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_FRAME_VIEW_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/message_loop.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/tab_icon_view.h"
#include "ui/views/controls/button/button.h"

class PanelBrowserView;
class SkPaint;
namespace gfx {
class Font;
class ImageSkia;
}
namespace ui {
class LinearAnimation;
}
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
  void UpdateTitleBarMinimizeRestoreButtonVisibility();

  // Returns the height of the entire nonclient top border, including the window
  // frame, any title area, and any connected client edge.
  int NonClientTopBorderHeight() const;

  // Returns the size of the non-client area, that is, the window size minus
  // the size of the client area.
  gfx::Size NonClientAreaSize() const;

  // Returns true if the frame window is showing only the titlebar.
  bool IsShowingTitlebarOnly() const;

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
  virtual void ResetWindowControls() OVERRIDE;
  virtual void UpdateWindowIcon() OVERRIDE;

  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnThemeChanged() OVERRIDE;
  virtual gfx::Size GetMinimumSize() OVERRIDE;
  virtual gfx::Size GetMaximumSize() OVERRIDE;
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
  virtual gfx::ImageSkia GetFaviconForTabIconView() OVERRIDE;

 private:
  friend class PanelBrowserViewTest;
  friend class NativePanelTestingWin;

  enum PaintState {
    NOT_PAINTED,
    PAINT_AS_INACTIVE,
    PAINT_AS_ACTIVE,
    PAINT_AS_MINIMIZED,
    PAINT_FOR_ATTENTION
  };

  // Returns the thickness of the entire nonclient left, right, and bottom
  // borders, including both the window frame and any client edge.
  int NonClientBorderThickness() const;

  // Returns the width of the panel that is showing only an icon.
  int IconOnlyWidth() const;

  // Update control styles to indicate if the titlebar is active or not.
  void UpdateControlStyles(PaintState paint_state);

  // Custom draw the frame.
  void PaintFrameBackground(gfx::Canvas* canvas);
  void PaintFrameEdge(gfx::Canvas* canvas);
  void PaintDivider(gfx::Canvas* canvas);

  // Retrieves the drawing metrics based on the current painting state.
  SkColor GetTitleColor(PaintState paint_state) const;
  SkColor GetDefaultTitleColor(PaintState paint_state) const;
  SkColor GetThemedTitleColor(PaintState paint_state) const;

  const gfx::ImageSkia* GetFrameBackground(PaintState paint_state) const;
  const gfx::ImageSkia* GetDefaultFrameBackground(PaintState paint_state) const;
  const gfx::ImageSkia* GetThemedFrameBackground(PaintState paint_state) const;

  bool UsingDefaultTheme(PaintState paint_state) const;

  string16 GetTitleText() const;

  bool CanResize() const;

  // The client view hosted within this non-client frame view that is
  // guaranteed to be freed before the client view.
  // (see comments about view hierarchies in non_client_view.h)
  PanelBrowserView* panel_browser_view_;

  PaintState paint_state_;
  views::ImageButton* close_button_;
  views::ImageButton* minimize_button_;
  views::ImageButton* restore_button_;
  TabIconView* title_icon_;
  views::Label* title_label_;
  gfx::Rect client_view_bounds_;

  DISALLOW_COPY_AND_ASSIGN(PanelBrowserFrameView);
};

#endif  // CHROME_BROWSER_UI_PANELS_PANEL_BROWSER_FRAME_VIEW_H_
