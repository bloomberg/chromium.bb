// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FRAME_BUBBLE_FRAME_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_FRAME_BUBBLE_FRAME_VIEW_H_
#pragma once

#include "chrome/browser/chromeos/frame/bubble_window.h"
#include "views/controls/button/button.h"
#include "views/window/non_client_view.h"

namespace gfx {
class Insets;
class Path;
class Point;
class Rect;
class Size;
}

namespace views {
class ImageButton;
class Label;
class Window;
}

namespace chromeos {

// BubbleFrameView implements a BubbleBorder based window frame.
class BubbleFrameView : public views::NonClientFrameView,
                        public views::ButtonListener {
 public:
  BubbleFrameView(views::Window* frame, BubbleWindow::Style style);
  virtual ~BubbleFrameView();

  // Overridden from views::NonClientFrameView:
  virtual gfx::Rect GetBoundsForClientView() const;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const;
  virtual int NonClientHitTest(const gfx::Point& point);
  virtual void GetWindowMask(const gfx::Size& size, gfx::Path* window_mask);
  virtual void EnableClose(bool enable);
  virtual void ResetWindowControls();

  // View overrides:
  virtual gfx::Insets GetInsets() const;
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void OnPaint(gfx::Canvas* canvas);

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event);

  void StartThrobber();
  void StopThrobber();

 private:
  // The window that owns this view.
  views::Window* frame_;

  // Allows to tweak appearance of the view.
  BubbleWindow::Style style_;

  // Title label
  views::Label* title_;

  // The bounds of the client view, in this view's coordinates.
  gfx::Rect client_view_bounds_;

  // Close button for STYLE_XBAR case.
  views::ImageButton* close_button_;

  // Throbber is optional. Employed by STYLE_THROBBER.
  views::Throbber* throbber_;

  DISALLOW_COPY_AND_ASSIGN(BubbleFrameView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FRAME_BUBBLE_FRAME_VIEW_H_

