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
  enum ButtonID {
    BUTTON_MINIMIZE,
    BUTTON_MAXIMIZE,
    BUTTON_CLOSE
  };

  explicit OpaqueBrowserFrameViewLayout(
      OpaqueBrowserFrameViewLayoutDelegate* delegate);
  virtual ~OpaqueBrowserFrameViewLayout();

  // Whether we should add the (minimize,maximize,close) buttons. Can be false
  // on Windows 8 in metro mode.
  static bool ShouldAddDefaultCaptionButtons();

  gfx::Rect GetBoundsForTabStrip(
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

  void set_extra_caption_y(int extra_caption_y) {
    extra_caption_y_ = extra_caption_y;
  }

  void set_window_caption_spacing(int window_caption_spacing) {
    window_caption_spacing_ = window_caption_spacing;
  }

  const gfx::Rect& client_view_bounds() const { return client_view_bounds_; }

 private:
  // Whether a specific button should be inserted on the leading or trailing
  // side.
  enum ButtonAlignment {
    ALIGN_LEADING,
    ALIGN_TRAILING
  };

  // Layout various sub-components of this view.
  void LayoutWindowControls(views::View* host);
  void LayoutTitleBar(views::View* host);
  void LayoutAvatar();

  void ConfigureButton(views::View* host,
                       ButtonID button_id,
                       ButtonAlignment align,
                       int caption_y);

  // Sets the visibility of all buttons associated with |button_id| to false.
  void HideButton(ButtonID button_id);

  // Adds a window caption button to either the leading or trailing side.
  void SetBoundsForButton(views::View* host,
                          views::ImageButton* button,
                          ButtonAlignment align,
                          int caption_y);

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

  // The layout of the window icon, if visible.
  gfx::Rect window_icon_bounds_;

  // How far from the leading/trailing edge of the view the next window control
  // should be placed.
  int leading_button_start_;
  int trailing_button_start_;

  // The size of the window buttons, and the avatar menu item (if any). This
  // does not count labels or other elements that should be counted in a
  // minimal frame.
  int minimum_size_for_buttons_;

  // Whether any of the window control buttons were packed on the leading.
  bool has_leading_buttons_;
  bool has_trailing_buttons_;

  // Extra offset from the top of the frame to the top of the window control
  // buttons. Configurable based on platform and whether we are under test.
  int extra_caption_y_;

  // Extra offset between the individual window caption buttons.
  int window_caption_spacing_;

  // Window controls.
  views::ImageButton* minimize_button_;
  views::ImageButton* maximize_button_;
  views::ImageButton* restore_button_;
  views::ImageButton* close_button_;

  views::View* window_icon_;
  views::Label* window_title_;

  views::View* avatar_label_;
  views::View* avatar_button_;

  std::vector<ButtonID> leading_buttons_;
  std::vector<ButtonID> trailing_buttons_;

  DISALLOW_COPY_AND_ASSIGN(OpaqueBrowserFrameViewLayout);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_OPAQUE_BROWSER_FRAME_VIEW_LAYOUT_H_
