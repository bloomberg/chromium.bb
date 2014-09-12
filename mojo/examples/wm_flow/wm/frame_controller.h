// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_EXAMPLES_WM_FLOW_WM_FRAME_CONTROLLER_H_
#define MOJO_EXAMPLES_WM_FLOW_WM_FRAME_CONTROLLER_H_

#include "mojo/services/public/cpp/view_manager/view_observer.h"
#include "ui/gfx/geometry/rect.h"

namespace mojo {
class NativeWidgetViewManager;
class View;
}

namespace views {
class View;
class Widget;
}

// FrameController encapsulates the window manager's frame additions to a window
// created by an application. It renders the content of the frame and responds
// to any events targeted at it.
class FrameController : mojo::ViewObserver {
 public:
  FrameController(mojo::View* view, mojo::View** app_view);
  virtual ~FrameController();

  void CloseWindow();
  void ToggleMaximize();

 private:
  class LayoutManager;
  friend class LayoutManager;

  virtual void OnViewDestroyed(mojo::View* view) MOJO_OVERRIDE;

  mojo::View* view_;
  mojo::View* app_view_;
  views::View* frame_view_;
  LayoutManager* frame_view_layout_manager_;
  views::Widget* widget_;
  bool maximized_;
  gfx::Rect restored_bounds_;

  DISALLOW_COPY_AND_ASSIGN(FrameController);
};

#endif  // MOJO_EXAMPLES_WM_FLOW_WM_FRAME_CONTROLLER_H_
