// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_EXTENSIONS_EXTENSION_POPUP_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_EXTENSIONS_EXTENSION_POPUP_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#import "base/cocoa_protocols_mac.h"
#import "base/scoped_nsobject.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/cocoa/info_bubble_view.h"
#include "googleurl/src/gurl.h"

class Browser;
class ExtensionHost;
@class InfoBubbleWindow;

// This controller manages a single browser action popup that can appear once a
// user has clicked on a browser action button. It instantiates the extension
// popup view showing the content and resizes the window to accomodate any size
// changes as they occur.
// There can only be one browser action popup open at a time.
@interface ExtensionPopupController : NSWindowController<NSWindowDelegate> {
 @private
  // The native extension view retrieved from the extension host. Weak.
  NSView* extensionView_;

  // The popup's parent window. Weak.
  NSWindow* parentWindow_;

  // Where the window is anchored. Right now it's the bottom center of the
  // browser action button.
  NSPoint anchor_;

  // The extension host object.
  scoped_ptr<ExtensionHost> host_;
}

// Returns a pointer to the extension host object associated with this
// browser action.
- (ExtensionHost*)host;

// Starts the process of showing the given popup URL. Instantiates an
// ExtensionPopupController with the parent window retrieved from |browser|, a
// host for the popup created by the extension process manager specific to the
// browser profile and the remaining arguments |anchoredAt| and |arrowLocation|.
// |anchoredAt| is expected to be in the screen's coordinates at the bottom
// center of the browser action button.
// The actual display of the popup is delayed until the page contents finish
// loading in order to minimize UI flashing and resizing.
+ (ExtensionPopupController*)showURL:(GURL)url
                           inBrowser:(Browser*)browser
                          anchoredAt:(NSPoint)anchoredAt
                       arrowLocation:(BubbleArrowLocation)arrowLocation;
@end

@interface ExtensionPopupController(TestingAPI)
// Returns a weak pointer to the current popup's view.
- (NSView*)view;
// Returns the minimum allowed size for an extension popup.
+ (NSSize)minPopupSize;
// Returns the maximum allowed size for an extension popup.
+ (NSSize)maxPopupSize;
@end

#endif  // CHROME_BROWSER_COCOA_EXTENSIONS_EXTENSION_POPUP_CONTROLLER_H_
