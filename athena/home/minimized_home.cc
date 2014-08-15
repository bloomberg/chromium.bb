// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/home/minimized_home.h"

#include "athena/wm/public/window_manager.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/view.h"

namespace {

const int kDragHandleWidth = 112;
const int kDragHandleHeight = 2;

class MinimizedHomeBackground : public views::Background {
 public:
  MinimizedHomeBackground() {}
  virtual ~MinimizedHomeBackground() {}

 private:
  virtual void Paint(gfx::Canvas* canvas, views::View* view) const OVERRIDE {
    gfx::Rect bounds = view->GetLocalBounds();
    canvas->FillRect(bounds, SK_ColorBLACK);
    canvas->FillRect(gfx::Rect((bounds.width() - kDragHandleWidth) / 2,
                               bounds.bottom() - kDragHandleHeight,
                               kDragHandleWidth,
                               kDragHandleHeight),
                     SK_ColorWHITE);
  }

  DISALLOW_COPY_AND_ASSIGN(MinimizedHomeBackground);
};

// This View shows an instance of SmallBarView in the middle, and reacts to
// mouse and touch-gesture events.
class MinimizedHomeView : public views::View {
 public:
  MinimizedHomeView() {
    set_background(new MinimizedHomeBackground());
  }
  virtual ~MinimizedHomeView() {}

 private:
  // views::View:
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE {
    if (event.IsLeftMouseButton() && event.GetClickCount() == 1) {
      athena::WindowManager::GetInstance()->ToggleOverview();
      return true;
    }
    return false;
  }

  DISALLOW_COPY_AND_ASSIGN(MinimizedHomeView);
};

}  // namespace

namespace athena {

views::View* CreateMinimizedHome() {
  return new MinimizedHomeView();
}

}  // namespace athena
