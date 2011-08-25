// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AURA_WINDOW_DELEGATE_H_
#define AURA_WINDOW_DELEGATE_H_
#pragma once

namespace aura {

// Delegate interface for aura::Window.
class WindowDelegate {
 public:
  // Asks the delegate to paint to the window. The delegate should call back
  // to the window with SetCanvas.
  virtual void OnPaint(const gfx::Rect& bounds) = 0;

 protected:
  virtual ~WindowDelegate() {}
};

}  // namespace aura

#endif  // AURA_WINDOW_DELEGATE_H_
