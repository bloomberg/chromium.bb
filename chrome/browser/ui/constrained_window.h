// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CONSTRAINED_WINDOW_H_
#define CHROME_BROWSER_UI_CONSTRAINED_WINDOW_H_

#include "build/build_config.h"
#include "ui/gfx/native_widget_types.h"

///////////////////////////////////////////////////////////////////////////////
// ConstrainedWindow
//
//  This interface represents a window that is constrained to a
//  WebContentsView's bounds.
//
class ConstrainedWindow {
 public:
  // Makes the Constrained Window visible. Only one Constrained Window is shown
  // at a time per tab.
  virtual void ShowConstrainedWindow() = 0;

  // Closes the Constrained Window.
  virtual void CloseConstrainedWindow() = 0;

  // Sets focus on the Constrained Window.
  virtual void FocusConstrainedWindow();

  // Runs a pulse animation for the constrained window.
  virtual void PulseConstrainedWindow();

  // Checks if the constrained window can be shown.
  virtual bool CanShowConstrainedWindow();

  // Returns the native window of the constrained window.
  virtual gfx::NativeWindow GetNativeWindow();

 protected:
  virtual ~ConstrainedWindow() {}
};

#endif  // CHROME_BROWSER_UI_CONSTRAINED_WINDOW_H_
