// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_NATIVE_PANEL_STACK_WINDOW_H_
#define CHROME_BROWSER_UI_PANELS_NATIVE_PANEL_STACK_WINDOW_H_

#include "base/memory/scoped_ptr.h"

class Panel;
namespace gfx {
class Rect;
}

// An interface that encapsulates the platform-specific behaviors that are
// needed to support multiple panels that are stacked together. A native
// window might be created to enclose all the panels in the stack. The lifetime
// of the class that implements this interface is managed by itself.
class NativePanelStackWindow {
 public:
  // Creates and returns a NativePanelStackWindow instance. Calling Close() will
  // destruct the instance.
  static NativePanelStackWindow* Create();

  virtual ~NativePanelStackWindow() {}

  virtual bool IsMinimized() const = 0;

 protected:
  friend class StackedPanelCollection;

  // Called when the stack is to be closed. This will cause this instance to be
  // self destructed after the native window closes.
  virtual void Close() = 0;

  // Makes |panel| be enclosed by this stack window.

  // Adds |panel| to the set of panels grouped and shown inside this stack
  // Window. It does not take ownership of |panel|.
  virtual void AddPanel(Panel* panel) = 0;

  // Removes |panel| from the set of panels grouped and shown inside this stack
  // window.
  virtual void RemovePanel(Panel* panel) = 0;

  // Returns true if no panel is being shown inside this stack window.
  virtual bool IsEmpty() const = 0;

  // Sets the bounds that is big enough to enclose all panels in the stack.
  virtual void SetBounds(const gfx::Rect& bounds) = 0;

  // Minimizes all the panels in the stack as a whole via system.
  virtual void Minimize() = 0;

  // Draws or clears the attention via system. The system might choose to
  // flash the taskbar icon for attention.
  virtual void DrawSystemAttention(bool draw_attention) = 0;
};

#endif  // CHROME_BROWSER_UI_PANELS_NATIVE_PANEL_STACK_WINDOW_H_
