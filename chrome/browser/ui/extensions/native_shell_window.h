// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_NATIVE_SHELL_WINDOW_H_
#define CHROME_BROWSER_UI_EXTENSIONS_NATIVE_SHELL_WINDOW_H_

#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/browser/ui/base_window.h"

// This is an interface to a native implementation of a shell window, used for
// new-style packaged apps. Shell windows contain a web contents, but no tabs
// or URL bar.
class NativeShellWindow : public BaseWindow {
 public:
  // Used by ShellWindow to instantiate the platform-specific ShellWindow code.
  static NativeShellWindow* Create(ShellWindow* window,
                                   const ShellWindow::CreateParams& params);

  // Called when the draggable regions are changed.
  virtual void UpdateDraggableRegions(
      const std::vector<extensions::DraggableRegion>& regions) {}

  virtual void SetFullscreen(bool fullscreen) = 0;
  virtual bool IsFullscreenOrPending() const = 0;

  // Called when the icon of the window changes.
  virtual void UpdateWindowIcon() = 0;

  // Called when the title of the window changes.
  virtual void UpdateWindowTitle() = 0;

  // Allows the window to handle unhandled keyboard messages coming back from
  // the renderer.
  virtual void HandleKeyboardEvent(
      const content::NativeWebKeyboardEvent& event) = 0;

  virtual ~NativeShellWindow() {}
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_NATIVE_SHELL_WINDOW_H_
