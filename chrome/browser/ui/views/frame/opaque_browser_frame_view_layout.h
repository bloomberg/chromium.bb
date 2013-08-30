// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_OPAQUE_BROWSER_FRAME_VIEW_LAYOUT_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_OPAQUE_BROWSER_FRAME_VIEW_LAYOUT_H_

#include "chrome/browser/ui/views/frame/opaque_browser_frame_view.h"
#include "ui/views/layout/layout_manager.h"

class OpaqueBrowserFrameViewLayoutDelegate;

namespace views {
class ImageButton;
class Label;
}

// Calculates the position of the widgets in the opaque browser frame view.
//
// This is separated out for testing reasons. OpaqueBrowserFrameView has tight
// dependencies with Browser and classes that depend on Browser.
class OpaqueBrowserFrameViewLayout : public views::LayoutManager {
 public:
  explicit OpaqueBrowserFrameViewLayout(
      OpaqueBrowserFrameViewLayoutDelegate* delegate);
  virtual ~OpaqueBrowserFrameViewLayout();

  // Whether we should add the (minimize,maximize,close) buttons. Can be false
  // on Windows 8 in metro mode.
  static bool ShouldAddDefaultCaptionButtons();

  gfx::Rect GetBoundsForTabStrip(
      const gfx::Size& tabstrip_preferred_size,
      int available_width) const;
  gfx::Rect GetBoundsForTabStripAndAvatarArea(
      const gfx::Size& tabstrip_preferred_size,
      int available_width) const;

  gfx::Size GetMinimumSize(int available_width) const;

  // Returns the bounds of the window required to display the content area at
  // the specified bounds.
  gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const;

  // Returns the thickness of the border that makes up the window frame edges.
  // This does not include any client edge.  If |restored| is true, acts as if
  // the window is restored regardless of the real mode.
  int FrameBorderThickness(bool restored) const;

  // Returns the thickness of the entire nonclient left, right, and bottom
  // borders, including both the window frame and any client edge.
  int NonClientBorderThickness() const;

  // Returns the height of the entire nonclient top border, including the window
  // frame, any title area, and any connected client edge.  If |restored| is
  // true, acts as if the window is restored regardless of the real mode.
  int NonClientTopBorderHeight(bool restored) const;

  int GetTabStripInsetsTop(bool restored) const;

  // Returns the y-coordinate of the caption buttons.  If |restored| is true,
  // acts as if the window is restored regardless of the real mode.
  int CaptionButtonY(bool restored) const;

  // Returns the thickness of the 3D edge along the bottom of the titlebar.  If
  // |restored| is true, acts as if the window is restored regardless of the
  // real mode.
  int TitlebarBottomThickness(bool restored) const;

  // Returns the bounds of the titlebar icon (or where the icon would be if
  // there was one).
  gfx::Rect IconBounds() const;

  // Returns the bounds of the client area for the specified view size.
  gfx::Rect CalculateClientAreaBounds(int width, int height) const;

  const gfx::Rect& client_view_bounds() const { return client_view_bounds_; }

 private:
  // Layout various sub-components of this view.
  void LayoutWindowControls(views::View* host);
  void LayoutTitleBar();
  void LayoutAvatar();

  // Internal implementation of ViewAdded() and ViewRemoved().
  void SetView(int id, views::View* view);

  // Overriden from views::LayoutManager:
  virtual void Layout(views::View* host) OVERRIDE;
  virtual gfx::Size GetPreferredSize(views::View* host) OVERRIDE;
  virtual void ViewAdded(views::View* host, views::View* view) OVERRIDE;
  virtual void ViewRemoved(views::View* host, views::View* view) OVERRIDE;

  OpaqueBrowserFrameViewLayoutDelegate* delegate_;

  // The layout rect of the avatar icon, if visible.
  gfx::Rect avatar_bounds_;

  // The bounds of the ClientView.
  gfx::Rect client_view_bounds_;

  // Window controls.
  views::ImageButton* minimize_button_;
  views::ImageButton* maximize_button_;
  views::ImageButton* restore_button_;
  views::ImageButton* close_button_;

  views::View* window_icon_;
  views::Label* window_title_;

  views::View* avatar_label_;
  views::View* avatar_button_;

  DISALLOW_COPY_AND_ASSIGN(OpaqueBrowserFrameViewLayout);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_OPAQUE_BROWSER_FRAME_VIEW_LAYOUT_H_
