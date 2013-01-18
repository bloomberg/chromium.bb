// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_NATIVE_PANEL_STACK_H_
#define CHROME_BROWSER_UI_PANELS_NATIVE_PANEL_STACK_H_

#include "base/memory/scoped_ptr.h"

class Panel;
class StackedPanelCollection;
namespace gfx {
class Rect;
}

// An interface that encapsulates the platform-specific behaviors that are
// needed to support multiple panels that are stacked together. A native
// window might be created to enclose all the panels in the stack. The lifetime
// of the class that implements this interface is managed by itself.
class NativePanelStack {
 public:
  // Creates and returns a NativePanelStack instance whose lifetime is managed
  // by itself and it will be valid until Close is called. NativePanelStack
  // also owns the StackedPanelCollection instance being passed here.
  static NativePanelStack* Create(scoped_ptr<StackedPanelCollection> stack);

  virtual ~NativePanelStack() {}

 protected:
  friend class StackedPanelCollection;

  // Called when the stack is to be closed. This will destruct the
  // NativePanelStack instance which causes the StackedPanelCollection
  // instance to be also destructed since the former owns the later.
  virtual void Close() = 0;

  // Called when |panel| is being added to or removed from the stack.
  virtual void OnPanelAddedOrRemoved(Panel* panel) = 0;

  // Sets the bounds that is big enough to enclose all panels in the stack.
  virtual void SetBounds(const gfx::Rect& bounds) = 0;

  // Minimizes all the panels in the stack as a whole via system.
  virtual void Minimize() = 0;
};

#endif  // CHROME_BROWSER_UI_PANELS_NATIVE_PANEL_STACK_H_
