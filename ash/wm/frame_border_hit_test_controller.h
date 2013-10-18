// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_FRAME_BORDER_HITTEST_CONTROLLER_H_
#define ASH_WM_FRAME_BORDER_HITTEST_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/wm/window_state_observer.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/window_observer.h"

namespace gfx {
class Point;
}

namespace views {
class NonClientFrameView;
class Widget;
}

namespace ash {
class HeaderPainter;

// Class which manages the hittest override bounds for |frame|. The override
// bounds are used to ensure that the resize cursors are shown when the user
// hovers over |frame|'s edges.
class ASH_EXPORT FrameBorderHitTestController : public wm::WindowStateObserver,
                                                public aura::WindowObserver {
 public:
  explicit FrameBorderHitTestController(views::Widget* frame);
  virtual ~FrameBorderHitTestController();

  // Does the non client hit test on behalf of |view|. |point| must be in the
  // coordinates of |view|'s widget.
  static int NonClientHitTest(views::NonClientFrameView* view,
                              HeaderPainter* header_painter,
                              const gfx::Point& point);

 private:
  // Updates the size of the region inside of |window_| in which the resize
  // handles are shown based on |window_|'s show type.
  void UpdateHitTestBoundsOverrideInner();

  // WindowStateObserver override:
  virtual void OnWindowShowTypeChanged(wm::WindowState* window_state,
                                       wm::WindowShowType old_type) OVERRIDE;
  // WindowObserver override:
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;

  // The window whose hittest override bounds are being managed.
  aura::Window* frame_window_;

  DISALLOW_COPY_AND_ASSIGN(FrameBorderHitTestController);
};

}  // namespace ash

#endif  // ASH_WM_FRAME_BORDER_HITTEST_CONTROLLER_H_
