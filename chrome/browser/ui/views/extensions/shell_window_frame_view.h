// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_SHELL_WINDOW_FRAME_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_SHELL_WINDOW_FRAME_VIEW_H_

#include <string>

#include "ui/gfx/path.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/window/non_client_view.h"

namespace gfx {
class Canvas;
class Point;
}

namespace ui {
class Event;
}

namespace views {
class ImageButton;
class Widget;
}

class NativeAppWindowViews;

// A frameless or non-Ash, non-panel NonClientFrameView for app windows.
class ShellWindowFrameView : public views::NonClientFrameView,
                             public views::ButtonListener {
 public:
  static const char kViewClassName[];

  explicit ShellWindowFrameView(NativeAppWindowViews* window);
  virtual ~ShellWindowFrameView();

  void Init(views::Widget* frame);

 private:
  // views::NonClientFrameView implementation.
  virtual gfx::Rect GetBoundsForClientView() const OVERRIDE;
  virtual gfx::Rect GetWindowBoundsForClientBounds(
      const gfx::Rect& client_bounds) const OVERRIDE;
  virtual int NonClientHitTest(const gfx::Point& point) OVERRIDE;
  virtual void GetWindowMask(const gfx::Size& size,
                             gfx::Path* window_mask) OVERRIDE;
  virtual void ResetWindowControls() OVERRIDE {}
  virtual void UpdateWindowIcon() OVERRIDE {}
  virtual void UpdateWindowTitle() OVERRIDE {}

  // views::View implementation.
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual gfx::Size GetMinimumSize() OVERRIDE;
  virtual gfx::Size GetMaximumSize() OVERRIDE;

  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender, const ui::Event& event)
      OVERRIDE;

  NativeAppWindowViews* window_;
  views::Widget* frame_;
  views::ImageButton* close_button_;
  views::ImageButton* maximize_button_;
  views::ImageButton* restore_button_;
  views::ImageButton* minimize_button_;

  DISALLOW_COPY_AND_ASSIGN(ShellWindowFrameView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_SHELL_WINDOW_FRAME_VIEW_H_
