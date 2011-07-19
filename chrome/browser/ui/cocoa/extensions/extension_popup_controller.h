// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_POPUP_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_POPUP_CONTROLLER_H_
#pragma once

#import <Cocoa/Cocoa.h>

#import "base/mac/cocoa_protocols.h"
#import "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/cocoa/info_bubble_view.h"
#include "googleurl/src/gurl.h"


class Browser;
class DevtoolsNotificationBridge;
class ExtensionHost;
@class InfoBubbleWindow;
class NotificationRegistrar;

// This controller manages a single browser action popup that can appear once a
// user has clicked on a browser action button. It instantiates the extension
// popup view showing the content and resizes the window to accomodate any size
// changes as they occur.
//
// There can only be one browser action popup open at a time, so a static
// variable holds a reference to the current popup.
@interface ExtensionPopupController : NSWindowController<NSWindowDelegate> {
 @private
  // The native extension view retrieved from the extension host. Weak.
  NSView* extensionView_;

  // The popup's parent window. Weak.
  NSWindow* parentWindow_;

  // Where the window is anchored. Right now it's the bottom center of the
  // browser action button.
  NSPoint anchor_;

  // The current frame of the extension view. Cached to prevent setting the
  // frame if the size hasn't changed.
  NSRect extensionFrame_;

  // The extension host object.
  scoped_ptr<ExtensionHost> host_;

  scoped_ptr<NotificationRegistrar> registrar_;
  scoped_ptr<DevtoolsNotificationBridge> notificationBridge_;

  // Whether the popup has a devtools window attached to it.
  BOOL beingInspected_;
}

// Returns the ExtensionHost object associated with this popup.
- (ExtensionHost*)extensionHost;

// Starts the process of showing the given popup URL. Instantiates an
// ExtensionPopupController with the parent window retrieved from |browser|, a
// host for the popup created by the extension process manager specific to the
// browser profile and the remaining arguments |anchoredAt| and |arrowLocation|.
// |anchoredAt| is expected to be in the window's coordinates at the bottom
// center of the browser action button.
// The actual display of the popup is delayed until the page contents finish
// loading in order to minimize UI flashing and resizing.
// Passing YES to |devMode| will launch the webkit inspector for the popup,
// and prevent the popup from closing when focus is lost.  It will be closed
// after the inspector is closed, or another popup is opened.
+ (ExtensionPopupController*)showURL:(GURL)url
                           inBrowser:(Browser*)browser
                          anchoredAt:(NSPoint)anchoredAt
                       arrowLocation:(info_bubble::BubbleArrowLocation)
                                         arrowLocation
                             devMode:(BOOL)devMode;

// Returns the controller used to display the popup being shown. If no popup is
// currently open, then nil is returned. Static because only one extension popup
// window can be open at a time.
+ (ExtensionPopupController*)popup;

// Whether the popup is in the process of closing (via Core Animation).
- (BOOL)isClosing;

// Show the dev tools attached to the popup.
- (void)showDevTools;
@end

@interface ExtensionPopupController(TestingAPI)
// Returns a weak pointer to the current popup's view.
- (NSView*)view;
// Returns the minimum allowed size for an extension popup.
+ (NSSize)minPopupSize;
// Returns the maximum allowed size for an extension popup.
+ (NSSize)maxPopupSize;
@end

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_POPUP_CONTROLLER_H_
