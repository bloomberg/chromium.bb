// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AURA_WINDOW_H_
#define AURA_WINDOW_H_
#pragma once

#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/compositor/layer_delegate.h"
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
// TODO(beng): resolve ownership.
class Window : public ui::LayerDelegate {
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

  explicit Window(WindowDelegate* delegate);
  ~Window();

  void Init();

  int id() const { return id_; }
  void set_id(int id) { id_ = id; }

  ui::Layer* layer() { return layer_.get(); }
  const ui::Layer* layer() const { return layer_.get(); }

  // Changes the visibility of the window.
  void SetVisibility(Visibility visibility);
  Visibility visibility() const { return visibility_; }

  // Changes the bounds of the window.
  void SetBounds(const gfx::Rect& bounds, int anim_ms);
  const gfx::Rect& bounds() const { return bounds_; }

  // Marks the a portion of window as needing to be painted.
  void SchedulePaintInRect(const gfx::Rect& rect);

  // Sets the contents of the window.
  void SetCanvas(const SkCanvas& canvas, const gfx::Point& origin);

  // Sets the parent window of the window. If NULL, the window is parented to
  // the desktop's window.
  void SetParent(Window* parent);
  Window* parent() { return parent_; }

  // Draw the window and its children.
  void DrawTree();

  // Tree operations.
  // TODO(beng): Child windows are currently not owned by the hierarchy. We
  //             should change this.
  void AddChild(Window* child);
  void RemoveChild(Window* child);

  static void ConvertPointToWindow(Window* source,
                                   Window* target,
                                   gfx::Point* point);

  // Handles a mouse event. Returns true if handled.
  bool OnMouseEvent(const MouseEvent& event);

  WindowDelegate* delegate() { return delegate_; }

  // Returns true if the mouse pointer at the specified |point| can trigger an
  // event for this Window.
  // TODO(beng):
  // A Window can supply a hit-test mask to cause some portions of itself to not
  // trigger events, causing the events to fall through to the Window behind.
  bool HitTest(const gfx::Point& point);

  // Returns the Window that most closely encloses |point| for the purposes of
  // event targeting.
  Window* GetEventHandlerForPoint(const gfx::Point& point);

 private:
  typedef std::vector<Window*> Windows;

  // If SchedulePaint has been invoked on the Window the delegate is notified.
  void UpdateLayerCanvas();

  // Draws the Window's contents.
  void Draw();

  // Schedules a paint for the Window's entire bounds.
  void SchedulePaint();

  // Overridden from ui::LayerDelegate:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  WindowDelegate* delegate_;

  Visibility visibility_;

  scoped_ptr<ui::Layer> layer_;

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
