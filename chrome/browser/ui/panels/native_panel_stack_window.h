// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_NATIVE_PANEL_STACK_WINDOW_H_
#define CHROME_BROWSER_UI_PANELS_NATIVE_PANEL_STACK_WINDOW_H_

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "ui/gfx/image/image.h"

class Panel;
namespace gfx {
class Rect;
class Vector2d;
}

class NativePanelStackWindowDelegate {
 public:
  // Returns the title representing the whole stack.
  virtual base::string16 GetTitle() const = 0;

  // Returns the icon denoting the whole stack.
  virtual gfx::Image GetIcon() const = 0;

  // Called when the batch bounds update is completed, i.e. animation ends.
  virtual void PanelBoundsBatchUpdateCompleted() = 0;
};

// An interface that encapsulates the platform-specific behaviors that are
// needed to support multiple panels that are stacked together. A native
// window might be created to enclose all the panels in the stack. The lifetime
// of the class that implements this interface is managed by itself.
class NativePanelStackWindow {
 public:
  // Creates and returns a NativePanelStackWindow instance. Calling Close() will
  // destruct the instance.
  static NativePanelStackWindow* Create(
      NativePanelStackWindowDelegate* delegate);

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

  // Merges those panels grouped and shown inside |another| stack window into
  // the set of panels grouped and shown inside this stack window.
  virtual void MergeWith(NativePanelStackWindow* another) = 0;

  // Returns true if no panel is being shown inside this stack window.
  virtual bool IsEmpty() const = 0;

  // Returns true if |panel| is being enclosed by this stack window.
  virtual bool HasPanel(Panel* panel) const = 0;

  // Moves all panels instantly by |delta|. All the moves should be done
  // simulatenously.
  virtual void MovePanelsBy(const gfx::Vector2d& delta) = 0;

  // Changes the bounds of a set of panels synchronously.
  virtual void BeginBatchUpdatePanelBounds(bool animate) = 0;
  virtual void AddPanelBoundsForBatchUpdate(Panel* panel,
                                            const gfx::Rect& new_bounds) = 0;
  virtual void EndBatchUpdatePanelBounds() = 0;

  // Returns true if some panels within this stack window are still in the
  // process of bounds animation.
  virtual bool IsAnimatingPanelBounds() const = 0;

  // Minimizes all the panels in the stack as a whole via system.
  virtual void Minimize() = 0;

  // Draws or clears the attention via system. The system might choose to
  // flash the taskbar icon for attention.
  virtual void DrawSystemAttention(bool draw_attention) = 0;

  // Called when the panel is activated.
  virtual void OnPanelActivated(Panel* panel) = 0;
};

#endif  // CHROME_BROWSER_UI_PANELS_NATIVE_PANEL_STACK_WINDOW_H_
