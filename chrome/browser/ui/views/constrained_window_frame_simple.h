// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_FRAME_SIMPLE_H_
#define CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_FRAME_SIMPLE_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/window/non_client_view.h"

namespace views {
class ImageButton;
class Label;
}

////////////////////////////////////////////////////////////////////////////////
// ConstrainedWindowFrameView
// Provides a custom Window frame for ConstrainedWindowViews.
// Implements a simple window with a minimal frame.
class ConstrainedWindowFrameSimple : public views::NonClientFrameView,
                                     public views::ButtonListener {
 public:
  explicit ConstrainedWindowFrameSimple(
      ConstrainedWindowViews* container,
      ConstrainedWindowViews::ChromeStyleClientInsets client_insets);
  virtual ~ConstrainedWindowFrameSimple();

  void set_bottom_margin(int margin) { bottom_margin_ = margin; }

 private:
  gfx::Insets GetClientInsets() const;

  // Overridden from views::NonClientFrameView:
  virtual gfx::Rect GetBoundsForClientView() const OVERRIDE;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const OVERRIDE;
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE;
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask) OVERRIDE;
  virtual void ResetWindowControls() OVERRIDE;
  virtual void UpdateWindowIcon() OVERRIDE;
  virtual void UpdateWindowTitle() OVERRIDE;

  // Overridden from View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // Overridden from ButtonListener
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  ConstrainedWindowViews* container_;
  ConstrainedWindowViews::ChromeStyleClientInsets client_insets_;
  views::Label* title_label_;
  views::ImageButton* close_button_;
  // Number of margin pixels between the client area and the bottom part of the
  // frame.  These are visually part of the frame, but are actually rendered by
  // the client area.
  int bottom_margin_;

  DISALLOW_COPY_AND_ASSIGN(ConstrainedWindowFrameSimple);
};
#endif  // CHROME_BROWSER_UI_VIEWS_CONSTRAINED_WINDOW_FRAME_SIMPLE_H_
