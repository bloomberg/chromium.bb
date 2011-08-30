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

// Delegate interface for aura::Window.
class WindowDelegate {
 public:
  // Asks the delegate to paint window contents into the supplied canvas.
   virtual void OnPaint(gfx::Canvas* canvas) = 0;

 protected:
  virtual ~WindowDelegate() {}
};

}  // namespace aura

#endif  // AURA_WINDOW_DELEGATE_H_
