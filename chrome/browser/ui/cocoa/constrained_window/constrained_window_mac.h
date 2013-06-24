// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_MAC_
#define CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_MAC_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#include "components/web_modal/native_web_contents_modal_dialog.h"

namespace content {
class WebContents;
}
class ConstrainedWindowMac;
@protocol ConstrainedWindowSheet;

// A delegate for a constrained window. The delegate is notified when the
// window closes.
class ConstrainedWindowMacDelegate {
 public:
  virtual void OnConstrainedWindowClosed(ConstrainedWindowMac* window) = 0;
};

// Constrained window implementation for Mac.
// Normally an instance of this class is owned by the delegate. The delegate
// should delete the instance when the window is closed.
class ConstrainedWindowMac {
 public:
  ConstrainedWindowMac(
      ConstrainedWindowMacDelegate* delegate,
      content::WebContents* web_contents,
      id<ConstrainedWindowSheet> sheet);
  virtual ~ConstrainedWindowMac();

  void ShowWebContentsModalDialog();
  // Closes the constrained window and deletes this instance.
  void CloseWebContentsModalDialog();
  void FocusWebContentsModalDialog();
  void PulseWebContentsModalDialog();
  web_modal::NativeWebContentsModalDialog GetNativeDialog();

 private:
  // Gets the parent window of the dialog.
  NSWindow* GetParentWindow() const;

  ConstrainedWindowMacDelegate* delegate_;  // weak, owns us.

  // The WebContents that owns and constrains this ConstrainedWindowMac. Weak.
  content::WebContents* web_contents_;

  base::scoped_nsprotocol<id<ConstrainedWindowSheet>> sheet_;

  // This is true if the constrained window has been shown.
  bool shown_;
};

#endif  // CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_MAC_
