// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"
#include "third_party/skia/include/core/SkBitmap.h"

class Browser;
class ExtensionLoadedNotificationObserver;
@class HoverCloseButton;
@class InfoBubbleView;

namespace extensions {
class BundleInstaller;
class Extension;
}

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
  kGeneric,
  kOmniboxKeyword,
  kPageAction,
  kBundle,
} ExtensionType;

}  // namespace extension_installed_bubble

// Controller for the extension installed bubble.  This bubble pops up after
// an extension has been installed to inform the user that the install happened
// properly, and to let the user know how to manage this extension in the
// future.
@interface ExtensionInstalledBubbleController : BaseBubbleController {
 @private
  const extensions::Extension* extension_;  // weak
  const extensions::BundleInstaller* bundle_;  // weak
  Browser* browser_;  // weak
  scoped_nsobject<NSImage> icon_;

  extension_installed_bubble::ExtensionType type_;

  // We need to remove the page action immediately when the browser window
  // closes while this bubble is still open, so the bubble's closing animation
  // doesn't overlap browser destruction.
  BOOL pageActionRemoved_;

  // Lets us register for EXTENSION_LOADED notifications.  The actual
  // notifications are sent to the observer object, which proxies them
  // back to the controller.
  scoped_ptr<ExtensionLoadedNotificationObserver> extensionObserver_;

  // References below are weak, being obtained from the nib.
  IBOutlet HoverCloseButton* closeButton_;
  IBOutlet NSImageView* iconImage_;
  IBOutlet NSTextField* extensionInstalledMsg_;
  // Only shown for page actions and omnibox keywords.
  IBOutlet NSTextField* extraInfoMsg_;
  IBOutlet NSTextField* extensionInstalledInfoMsg_;
  // Only shown for extensions with commands.
  IBOutlet NSButton* manageShortcutLink_;
  // Only shown for bundle installs.
  IBOutlet NSTextField* installedHeadingMsg_;
  IBOutlet NSTextField* installedItemsMsg_;
  IBOutlet NSTextField* failedHeadingMsg_;
  IBOutlet NSTextField* failedItemsMsg_;
}

@property(nonatomic, readonly) const extensions::Extension* extension;
@property(nonatomic, readonly) const extensions::BundleInstaller* bundle;
@property(nonatomic) BOOL pageActionRemoved;

// Initialize the window, and then create observers to wait for the extension
// to complete loading, or the browser window to close.
- (id)initWithParentWindow:(NSWindow*)parentWindow
                 extension:(const extensions::Extension*)extension
                    bundle:(const extensions::BundleInstaller*)bundle
                   browser:(Browser*)browser
                      icon:(SkBitmap)icon;

// Action for close button.
- (IBAction)closeWindow:(id)sender;

// Displays the extension installed bubble. This callback is triggered by
// the extensionObserver when the extension has completed loading.
- (void)showWindow:(id)sender;

// Clears our weak pointer to the Extension. This callback is triggered by
// the extensionObserver when the extension is unloaded.
- (void)extensionUnloaded:(id)sender;

// Opens the shortcut configuration UI.
- (IBAction)onManageShortcutClicked:(id)sender;

@end

@interface ExtensionInstalledBubbleController (ExposedForTesting)

- (void)removePageActionPreviewIfNecessary;
- (NSWindow*)initializeWindow;
- (int)calculateWindowHeight;
- (void)setMessageFrames:(int)newWindowHeight;
- (NSRect)getExtensionInstalledMsgFrame;
- (NSRect)getExtraInfoMsgFrame;
- (NSRect)getExtensionInstalledInfoMsgFrame;

@end  // ExtensionInstalledBubbleController(ExposedForTesting)

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_CONTROLLER_H_
