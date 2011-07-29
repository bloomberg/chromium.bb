// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AURA_WINDOW_H_
#define AURA_WINDOW_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"

class SkCanvas;

namespace ui {
class Layer;
}

namespace aura {

class Desktop;
class WindowDelegate;

// Aura window implementation. Interesting events are sent to the
// WindowDelegate.
class Window {
 public:
  enum Visibility {
    // Don't display the window onscreen and don't let it receive mouse
    // events. This is the default.
    VISIBILITY_HIDDEN = 1,

    // Display the window and let it receive mouse events.
    VISIBILITY_SHOWN = 2,

    // Display the window but prevent it from receiving mouse events.
    VISIBILITY_SHOWN_NO_INPUT = 3,
  };

  explicit Window(Desktop* desktop);
  ~Window();

  void set_delegate(WindowDelegate* d) { delegate_ = d; }

  // Changes the visbility of the window.
  void SetVisibility(Visibility visibility);
  Visibility visibility() const { return visibility_; }

  // Changes the bounds of the window.
  void SetBounds(const gfx::Rect& bounds, int anim_ms);
  const gfx::Rect& bounds() const { return bounds_; }

  // Marks the window as needing to be painted.
  void SchedulePaint(const gfx::Rect& bounds);

  // Sets the contents of the window.
  void SetCanvas(const SkCanvas& canvas, const gfx::Point& origin);

  // If the window is visible its layer is drawn.
  void Draw();

  // If SchedulePaint has been invoked on the Window the delegate is notified.
  void UpdateLayerCanvas();

 private:
  // The desktop we're in.
  Desktop* desktop_;

  WindowDelegate* delegate_;

  Visibility visibility_;

  scoped_ptr<ui::Layer> layer_;

  // Union of regions passed to SchedulePaint. Cleaned when UpdateLayerCanvas is
  // invoked.
  gfx::Rect dirty_rect_;

  // If true UpdateLayerCanvas paints all. This is set when the window is first
  // created to trigger painting the complete bounds.
  bool needs_paint_all_;

  // Bounds of the window in the desktop's coordinate system.
  gfx::Rect bounds_;

  DISALLOW_COPY_AND_ASSIGN(Window);
};

}  // namespace aura

#endif  // AURA_WINDOW_H_
