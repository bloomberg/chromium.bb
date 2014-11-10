// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_NATIVE_BROWSER_FRAME_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_NATIVE_BROWSER_FRAME_H_

#include "ui/base/ui_base_types.h"
#include "ui/gfx/rect.h"

class BrowserFrame;
class BrowserView;

namespace views {
class NativeWidget;
}

class NativeBrowserFrame {
 public:
  virtual ~NativeBrowserFrame() {}

  virtual views::NativeWidget* AsNativeWidget() = 0;
  virtual const views::NativeWidget* AsNativeWidget() const = 0;

  // Returns true if the OS takes care of showing the system menu. Returning
  // false means BrowserFrame handles showing the system menu.
  virtual bool UsesNativeSystemMenu() const = 0;

  // Returns true when the window placement should be stored.
  virtual bool ShouldSaveWindowPlacement() const = 0;

  // Retrieves the window placement (show state and bounds) for restoring.
  virtual void GetWindowPlacement(gfx::Rect* bounds,
                                  ui::WindowShowState* show_state) const = 0;

 protected:
  friend class BrowserFrame;

  // BrowserFrame pass-thrus ---------------------------------------------------
  // See browser_frame.h for documentation:
  virtual int GetMinimizeButtonOffset() const = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_NATIVE_BROWSER_FRAME_H_
