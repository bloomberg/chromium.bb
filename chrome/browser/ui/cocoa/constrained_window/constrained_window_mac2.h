// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_MAC_2_
#define CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_MAC_2_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/ui/constrained_window.h"

class TabContents;

// Constrained window implementation for Mac.
class ConstrainedWindowMac2 : public ConstrainedWindow {
 public:
  ConstrainedWindowMac2(TabContents* tab_contents, NSWindow* window);

  // ConstrainedWindow implementation.
  virtual void ShowConstrainedWindow() OVERRIDE;
  // Closes the constrained window and deletes this instance.
  virtual void CloseConstrainedWindow() OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() OVERRIDE;
  virtual bool CanShowConstrainedWindow() OVERRIDE;

 private:
  virtual ~ConstrainedWindowMac2();

  // Gets the parent window of the dialog.
  NSWindow* GetParentWindow() const;

  TabContents* tab_contents_;  // weak
  scoped_nsobject<NSWindow> window_;
};

#endif  // CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_MAC_2_
