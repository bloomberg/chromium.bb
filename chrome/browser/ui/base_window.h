// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_BASE_WINDOW_H_
#define CHROME_BROWSER_UI_BASE_WINDOW_H_

#include "base/compiler_specific.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Rect;
}

class SkRegion;

// This API needs to be implemented by any window that might be accessed
// through chrome.windows or chrome.tabs (e.g. browser windows and panels).

class BaseWindow {
 public:
  // Returns true if the window is currently the active/focused window.
  virtual bool IsActive() const = 0;

  // Returns true if the window is maximized (aka zoomed).
  virtual bool IsMaximized() const = 0;

  // Returns true if the window is minimized.
  virtual bool IsMinimized() const = 0;

  // Returns true if the window is full screen.
  virtual bool IsFullscreen() const = 0;

  // Return a platform dependent identifier for this window.
  virtual gfx::NativeWindow GetNativeWindow() = 0;

  // Returns the nonmaximized bounds of the window (even if the window is
  // currently maximized or minimized) in terms of the screen coordinates.
  virtual gfx::Rect GetRestoredBounds() const = 0;

  // Retrieves the window's current bounds, including its window.
  // This will only differ from GetRestoredBounds() for maximized
  // and minimized windows.
  virtual gfx::Rect GetBounds() const = 0;

  // Shows the window, or activates it if it's already visible.
  virtual void Show() = 0;

  // Hides the window.
  virtual void Hide() = 0;

  // Show the window, but do not activate it. Does nothing if window
  // is already visible.
  virtual void ShowInactive() = 0;

  // Closes the window as soon as possible. The close action may be delayed
  // if an operation is in progress (e.g. a drag operation).
  virtual void Close() = 0;

  // Activates (brings to front) the window. Restores the window from minimized
  // state if necessary.
  virtual void Activate() = 0;

  // Deactivates the window, making the next window in the Z order the active
  // window.
  virtual void Deactivate() = 0;

  // Maximizes/minimizes/restores the window.
  virtual void Maximize() = 0;
  virtual void Minimize() = 0;
  virtual void Restore() = 0;

  // Sets the window's size and position to the specified values.
  virtual void SetBounds(const gfx::Rect& bounds) = 0;

  // Flashes the taskbar item associated with this window.
  // Set |flash| to true to initiate flashing, false to stop flashing.
  virtual void FlashFrame(bool flash) = 0;

  // Returns true if a window is set to be always on top.
  virtual bool IsAlwaysOnTop() const = 0;
};

#endif  // CHROME_BROWSER_UI_BASE_WINDOW_H_
