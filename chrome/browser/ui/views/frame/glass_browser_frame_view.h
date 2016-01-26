// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_BROWSER_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_BROWSER_FRAME_VIEW_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/win/scoped_gdi_object.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "ui/views/window/non_client_view.h"

class BrowserView;

class GlassBrowserFrameView : public BrowserNonClientFrameView {
 public:
  // Constructs a non-client view for an BrowserFrame.
  GlassBrowserFrameView(BrowserFrame* frame, BrowserView* browser_view);
  ~GlassBrowserFrameView() override;

  // BrowserNonClientFrameView:
  gfx::Rect GetBoundsForTabStrip(views::View* tabstrip) const override;
  int GetTopInset(bool restored) const override;
  int GetThemeBackgroundXInset() const override;
  void UpdateThrobber(bool running) override;
  gfx::Size GetMinimumSize() const override;

  // views::NonClientFrameView:
  gfx::Rect GetBoundsForClientView() const override;
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const override;
  int NonClientHitTest(const gfx::Point& point) override;
  void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask) override {}
  void ResetWindowControls() override {}
  void UpdateWindowIcon() override {}
  void UpdateWindowTitle() override {}
  void SizeConstraintsChanged() override {}

 protected:
  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void Layout() override;

  // BrowserNonClientFrameView:
  void UpdateNewAvatarButtonImpl() override;

 private:
  // views::NonClientFrameView:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  // Returns the thickness of the border that makes up the window left, right,
  // and bottom frame edges.  This does not include any client edge.
  int FrameBorderThickness() const;

  // Returns the height of the window top frame edge.  If |restored| is true,
  // this is calculated as if the window was restored, regardless of its current
  // state.
  int FrameTopBorderHeight(bool restored) const;

  // Returns the thickness of the entire nonclient left, right, and bottom
  // borders, including both the window frame and any client edge. If |restored|
  // is true, this is calculated as if the window was restored, regardless of
  // its current state.
  int NonClientBorderThickness(bool restored) const;

  // Returns the height of the entire nonclient top border, including the window
  // frame, any title area, and any connected client edge.  If |restored| is
  // true, this is calculated as if the window was restored, regardless of its
  // current state.
  int NonClientTopBorderHeight(bool restored) const;

  // Returns whether the toolbar is currently visible.
  bool IsToolbarVisible() const;

  // Paint various sub-components of this view.
  void PaintToolbarBackground(gfx::Canvas* canvas) const;
  void PaintClientEdge(gfx::Canvas* canvas) const;
  void FillClientEdgeRects(int x,
                           int y,
                           int right,
                           int bottom,
                           SkColor color,
                           gfx::Canvas* canvas) const;

  // Layout various sub-components of this view.
  void LayoutIncognitoIcon();
  void LayoutNewStyleAvatar();
  void LayoutClientView();

  // Returns the insets of the client area. If |restored| is true, this is
  // calculated as if the window was restored, regardless of its current state.
  gfx::Insets GetClientAreaInsets(bool restored) const;

  // Returns the bounds of the client area for the specified view size.
  gfx::Rect CalculateClientAreaBounds() const;

  // Starts/Stops the window throbber running.
  void StartThrobber();
  void StopThrobber();

  // Displays the next throbber frame.
  void DisplayNextThrobberFrame();

  // The layout rect of the incognito icon, if visible.
  gfx::Rect incognito_bounds_;

  // The bounds of the ClientView.
  gfx::Rect client_view_bounds_;

  // The small icon created from the bitmap image of the window icon.
  base::win::ScopedHICON small_window_icon_;

  // The big icon created from the bitmap image of the window icon.
  base::win::ScopedHICON big_window_icon_;

  // Whether or not the window throbber is currently animating.
  bool throbber_running_;

  // The index of the current frame of the throbber animation.
  int throbber_frame_;

  static const int kThrobberIconCount = 24;
  static HICON throbber_icons_[kThrobberIconCount];
  static void InitThrobberIcons();

  DISALLOW_COPY_AND_ASSIGN(GlassBrowserFrameView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_GLASS_BROWSER_FRAME_VIEW_H_
