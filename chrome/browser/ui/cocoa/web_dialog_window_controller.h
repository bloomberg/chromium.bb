// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_WEB_DIALOG_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_WEB_DIALOG_WINDOW_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/web_dialogs/web_dialog_ui.h"

class WebDialogWindowDelegateBridge;

namespace content {
class BrowserContext;
class WebContents;
}

// This controller manages a dialog box with properties and HTML content taken
// from a WebDialogDelegate object.
@interface WebDialogWindowController : NSWindowController<NSWindowDelegate> {
 @private
  // Order here is important, as webContents_ may send messages to
  // delegate_ when it gets destroyed.
  scoped_ptr<WebDialogWindowDelegateBridge> delegate_;
  scoped_ptr<content::WebContents> webContents_;
}

// Creates and shows an WebDialogWindowController with the given
// delegate and profile. The window is automatically destroyed when it, or its
// profile, is closed. Returns the created window.
//
// Make sure to use the returned window only when you know it is safe
// to do so, i.e. before OnDialogClosed() is called on the delegate.
+ (NSWindow*)showWebDialog:(ui::WebDialogDelegate*)delegate
                   context:(content::BrowserContext*)context;

@end

@interface WebDialogWindowController (TestingAPI)

// This is the designated initializer.  However, this is exposed only
// for testing; use -showWebDialog:context: instead.
- (id)initWithDelegate:(ui::WebDialogDelegate*)delegate
               context:(content::BrowserContext*)context;

// Loads the HTML content from the delegate; this is not a lightweight
// process which is why it is not part of the constructor.  Must be
// called before -showWebDialog:context:.
- (void)loadDialogContents;

@end

#endif  // CHROME_BROWSER_UI_COCOA_WEB_DIALOG_WINDOW_CONTROLLER_H_
