// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_HTML_DIALOG_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_HTML_DIALOG_WINDOW_CONTROLLER_H_

#include <string>
#include <vector>

#import <Cocoa/Cocoa.h>

#include "app/gfx/native_widget_types.h"
#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/dom_ui/html_dialog_ui.h"

class Browser;
class HtmlDialogWindowDelegateBridge;
class Profile;
class TabContents;

// This controller manages a dialog box with properties and HTML content taken
// from a HTMLDialogUIDelegate object.
@interface HtmlDialogWindowController : NSWindowController {
 @private
  // An HTML dialog can exist separately from a window in OS X, so this
  // controller needs its own browser.
  scoped_ptr<Browser> browser_;
  // Order here is important, as tab_contents_ may send messages to
  // delegate_ when it gets destroyed.
  scoped_ptr<HtmlDialogWindowDelegateBridge> delegate_;
  scoped_ptr<TabContents> tabContents_;
}

// Creates and shows an HtmlDialogWindowController with the given
// delegate, parent window, and profile, none of which may be NULL.
// The window is automatically destroyed when it is closed.
//
// TODO(akalin): Handle a NULL parentWindow as HTML dialogs may be launched
// without any browser windows being present (on OS X).
+ (void)showHtmlDialog:(HtmlDialogUIDelegate*)delegate
               profile:(Profile*)profile
          parentWindow:(gfx::NativeWindow)parent_window;

@end

@interface HtmlDialogWindowController (TestingAPI)

// This is the designated initializer.  However, this is exposed only
// for testing; use showHtmlDialog instead.
- (id)initWithDelegate:(HtmlDialogUIDelegate*)delegate
               profile:(Profile*)profile
          parentWindow:(gfx::NativeWindow)parentWindow;

// Loads the HTML content from the delegate; this is not a lightweight
// process which is why it is not part of the constructor.  Must be
// called before showWindow.
- (void)loadDialogContents;

@end

#endif  // CHROME_BROWSER_COCOA_HTML_DIALOG_WINDOW_CONTROLLER_H_

