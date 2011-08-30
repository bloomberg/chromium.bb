// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AURA_WINDOW_DELEGATE_H_
#define AURA_WINDOW_DELEGATE_H_
#pragma once

namespace gfx {
class Canvas;
}

namespace aura {

class MouseEvent;

// Delegate interface for aura::Window.
class WindowDelegate {
 public:
  virtual bool OnMouseEvent(const MouseEvent& event) = 0;

  // Asks the delegate to paint window contents into the supplied canvas.
  virtual void OnPaint(gfx::Canvas* canvas) = 0;

  // Called when the Window has been destroyed (i.e. from its destructor).
  // The delegate can use this as an opportunity to delete itself if necessary.
  virtual void OnWindowDestroyed() = 0;

 protected:
  virtual ~WindowDelegate() {}
};

}  // namespace aura

#endif  // AURA_WINDOW_DELEGATE_H_
