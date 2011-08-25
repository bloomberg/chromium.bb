// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AURA_WINDOW_H_
#define AURA_WINDOW_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"

class SkCanvas;

namespace ui {
class Compositor;
class Layer;
}

namespace aura {

class Desktop;
class MouseEvent;
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

  int id() const { return id_; }
  void set_id(int id) { id_ = id; }

  // Changes the visibility of the window.
  void SetVisibility(Visibility visibility);
  Visibility visibility() const { return visibility_; }

  // Changes the bounds of the window.
  void SetBounds(const gfx::Rect& bounds, int anim_ms);
  const gfx::Rect& bounds() const { return bounds_; }

  // Marks the window as needing to be painted.
  void SchedulePaint(const gfx::Rect& bounds);

  // Sets the contents of the window.
  void SetCanvas(const SkCanvas& canvas, const gfx::Point& origin);

  // Draw the window and its children.
  void DrawTree();

  // Tree operations.
  // TODO(beng): Child windows are currently not owned by the hierarchy. We
  //             should change this.
  void AddChild(Window* child);
  void RemoveChild(Window* child);
  Window* parent() { return parent_; }

  // Handles a mouse event. Returns true if handled.
  bool OnMouseEvent(const MouseEvent& event);

 private:
  typedef std::vector<Window*> Windows;

  // If SchedulePaint has been invoked on the Window the delegate is notified.
  void UpdateLayerCanvas();

  // Draws the Window's contents.
  void Draw();

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

  // The Window's parent.
  // TODO(beng): Implement NULL-ness for toplevels.
  Window* parent_;

  // Child windows. Topmost is last.
  Windows children_;

  int id_;

  DISALLOW_COPY_AND_ASSIGN(Window);
};

}  // namespace aura

#endif  // AURA_WINDOW_H_
