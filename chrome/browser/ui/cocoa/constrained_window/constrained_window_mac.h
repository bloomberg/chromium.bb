// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_MAC_
#define CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_MAC_

#import <Cocoa/Cocoa.h>

namespace content {
class WebContents;
}
class ConstrainedWindowMac;
class SingleWebContentsDialogManagerCocoa;
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
  ConstrainedWindowMac(ConstrainedWindowMacDelegate* delegate,
                       content::WebContents* web_contents,
                       id<ConstrainedWindowSheet> sheet);
  ~ConstrainedWindowMac();

  // Closes the constrained window.
  void CloseWebContentsModalDialog();

  SingleWebContentsDialogManagerCocoa* manager() const { return manager_; }
  void set_manager(SingleWebContentsDialogManagerCocoa* manager) {
    manager_ = manager;
  }

  // Called by |manager_| when the dialog is closing.
  void OnDialogClosing();

 private:
  ConstrainedWindowMacDelegate* delegate_;  // weak, owns us.
  SingleWebContentsDialogManagerCocoa* manager_;  // weak, owned by WCMDM.
};

#endif  // CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_MAC_
