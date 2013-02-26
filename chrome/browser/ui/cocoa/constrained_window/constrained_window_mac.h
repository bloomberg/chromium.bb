// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_MAC_
#define CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_MAC_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/ui/native_web_contents_modal_dialog.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

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
class ConstrainedWindowMac : public content::NotificationObserver {
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
  NativeWebContentsModalDialog GetNativeDialog();

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // Gets the parent window of the dialog.
  NSWindow* GetParentWindow() const;

  ConstrainedWindowMacDelegate* delegate_;  // weak, owns us.

  // The WebContents that owns and constrains this ConstrainedWindowMac. Weak.
  content::WebContents* web_contents_;

  scoped_nsprotocol<id<ConstrainedWindowSheet>> sheet_;

  // A scoped container for notification registries.
  content::NotificationRegistrar registrar_;

  // This is true if the constrained window is waiting to be shown.
  bool pending_show_;
};

#endif  // CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_MAC_
