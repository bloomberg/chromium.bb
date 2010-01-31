// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COCOA_EXTENSION_INSTALLED_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_COCOA_EXTENSION_INSTALLED_BUBBLE_CONTROLLER_H_

#import <Cocoa/Cocoa.h>
#import "base/cocoa_protocols_mac.h"
#include "base/scoped_ptr.h"
#import "chrome/browser/cocoa/browser_window_controller.h"
#import "chrome/browser/cocoa/info_bubble_view.h"
#include "third_party/skia/include/core/SkBitmap.h"

class Browser;
class Extension;
class ExtensionLoadedNotificationObserver;

@class HoverCloseButton;

namespace extension_installed_bubble {

// Maximum height or width of extension's icon (corresponds to Windows & GTK).
const int kIconSize = 43;

// Outer vertical margin for text, icon, and closing x.
const int kOuterVerticalMargin = 15;

// Inner vertical margin for text messages.
const int kInnerVerticalMargin = 10;

// We use a different kind of notification for each of these extension types.
typedef enum {
  kBrowserAction,
  kPageAction,
  kGeneric
} ExtensionType;

}

// Controller for the extension installed bubble.  This bubble pops up after
// an extension has been installed to inform the user that the install happened
// properly, and to let the user know how to manage this extension in the
// future.
@interface ExtensionInstalledBubbleController :
    NSWindowController<NSWindowDelegate> {
 @private
  NSWindow* parentWindow_;  // weak
  Extension* extension_;  // weak
  Browser* browser_;  // weak
  scoped_nsobject<NSImage> icon_;

  extension_installed_bubble::ExtensionType type_;

  // Lets us register for EXTENSION_LOADED notifications.  The actual
  // notifications are sent to the observer object, which proxies them
  // back to the controller.
  scoped_ptr<ExtensionLoadedNotificationObserver> extensionObserver_;

  // References below are weak, being obtained from the nib.
  IBOutlet InfoBubbleView* infoBubbleView_;
  IBOutlet HoverCloseButton* closeButton_;
  IBOutlet NSImageView* iconImage_;
  IBOutlet NSTextField* extensionInstalledMsg_;
  IBOutlet NSTextField* pageActionInfoMsg_;  // Only shown for page actions.
  IBOutlet NSTextField* extensionInstalledInfoMsg_;
}

@property (readonly) Extension* extension;

// Initialize the window, and then create observers to wait for the extension
// to complete loading, or the browser window to close.
- (id)initWithParentWindow:(NSWindow*)parentWindow
                 extension:(Extension*)extension
                   browser:(Browser*)browser
                      icon:(SkBitmap)icon;

// Action for close button.
- (IBAction)closeWindow:(id)sender;

// Displays the extension installed bubble. This callback is triggered by
// the extensionObserver when the extension has completed loading.
- (void)showWindow:(id)sender;

@end

@interface ExtensionInstalledBubbleController(ExposedForTesting)

- (void)removePageActionPreview;
- (NSWindow*)initializeWindow;
- (int)calculateWindowHeight;
- (void)setMessageFrames:(int)newWindowHeight;
- (NSRect)getExtensionInstalledMsgFrame;
- (NSRect)getPageActionInfoMsgFrame;
- (NSRect)getExtensionInstalledInfoMsgFrame;

@end  // ExtensionInstalledBubbleController(ExposedForTesting)

#endif  // CHROME_BROWSER_COCOA_EXTENSION_INSTALLED_BUBBLE_CONTROLLER_H_
