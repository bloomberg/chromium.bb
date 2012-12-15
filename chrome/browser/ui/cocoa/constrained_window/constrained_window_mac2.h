// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_MAC_2_
#define CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_MAC_2_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "chrome/browser/ui/constrained_window.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace content {
class WebContents;
}
class ConstrainedWindowMac2;
@protocol ConstrainedWindowSheet;

// A delegate for a constrained window. The delegate is notified when the
// window closes.
class ConstrainedWindowMacDelegate2 {
 public:
  virtual void OnConstrainedWindowClosed(ConstrainedWindowMac2* window) = 0;
};

// Constrained window implementation for Mac.
// Normally an instance of this class is owned by the delegate. The delegate
// should delete the instance when the window is closed.
class ConstrainedWindowMac2 : public ConstrainedWindow,
                              public content::NotificationObserver {
 public:
  ConstrainedWindowMac2(
      ConstrainedWindowMacDelegate2* delegate,
      content::WebContents* web_contents,
      id<ConstrainedWindowSheet> sheet);
  virtual ~ConstrainedWindowMac2();

  // ConstrainedWindow implementation.
  virtual void ShowConstrainedWindow() OVERRIDE;
  // Closes the constrained window and deletes this instance.
  virtual void CloseConstrainedWindow() OVERRIDE;
  virtual void PulseConstrainedWindow() OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() OVERRIDE;
  virtual bool CanShowConstrainedWindow() OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // Gets the parent window of the dialog.
  NSWindow* GetParentWindow() const;

  ConstrainedWindowMacDelegate2* delegate_;  // weak, owns us.

  // The WebContents that owns and constrains this ConstrainedWindow. Weak.
  content::WebContents* web_contents_;

  scoped_nsprotocol<id<ConstrainedWindowSheet> > sheet_;

  // A scoped container for notification registries.
  content::NotificationRegistrar registrar_;

  // This is true if the constrained window is waiting to be shown.
  bool pending_show_;
};

#endif  // CHROME_BROWSER_UI_COCOA_CONSTRAINED_WINDOW_CONSTRAINED_WINDOW_MAC_2_
