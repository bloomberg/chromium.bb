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
@class HyperlinkTextView;
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

// An offset we apply to position the point of the bubble's arrow pointing at
// the NewTabButton.
const int kAppsBubbleArrowOffset = 4;

// We use a different kind of notification for each of these extension types.
typedef enum {
  kApp,
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
@interface ExtensionInstalledBubbleController :
    BaseBubbleController<NSTextViewDelegate> {
 @private
  const extensions::Extension* extension_;  // weak
  const extensions::BundleInstaller* bundle_;  // weak
  Browser* browser_;  // weak
  base::scoped_nsobject<NSImage> icon_;

  extension_installed_bubble::ExtensionType type_;

  // We need to remove the page action immediately when the browser window
  // closes while this bubble is still open, so the bubble's closing animation
  // doesn't overlap browser destruction.
  BOOL pageActionPreviewShowing_;

  // Lets us register for EXTENSION_LOADED notifications.  The actual
  // notifications are sent to the observer object, which proxies them
  // back to the controller.
  scoped_ptr<ExtensionLoadedNotificationObserver> extensionObserver_;

  // References below are weak, being obtained from the nib.
  IBOutlet HoverCloseButton* closeButton_;
  IBOutlet NSImageView* iconImage_;
  IBOutlet NSTextField* heading_;
  // Only shown for browser actions, page actions and omnibox keywords.
  IBOutlet NSTextField* howToUse_;
  IBOutlet NSTextField* howToManage_;
  // Only shown for app installs.
  IBOutlet NSButton* appShortcutLink_;
  // Only shown for extensions with commands.
  IBOutlet NSButton* manageShortcutLink_;
  // Only shown if the sign-in promo is active.
  IBOutlet NSTextField* promoPlaceholder_;
  // Text fields don't work as well with embedded links as text views, but
  // text views cannot conveniently be created in IB. The xib file contains
  // a text field |promoPlaceholder_| that's replaced by this text view |promo_|
  // in -awakeFromNib.
  base::scoped_nsobject<HyperlinkTextView> promo_;
  // Only shown for bundle installs.
  IBOutlet NSTextField* installedHeadingMsg_;
  IBOutlet NSTextField* installedItemsMsg_;
  IBOutlet NSTextField* failedHeadingMsg_;
  IBOutlet NSTextField* failedItemsMsg_;
}

@property(nonatomic, readonly) const extensions::Extension* extension;
@property(nonatomic, readonly) const extensions::BundleInstaller* bundle;
@property(nonatomic) BOOL pageActionPreviewShowing;

// Initialize the window, and then create observers to wait for the extension
// to complete loading, or the browser window to close.
- (id)initWithParentWindow:(NSWindow*)parentWindow
                 extension:(const extensions::Extension*)extension
                    bundle:(const extensions::BundleInstaller*)bundle
                   browser:(Browser*)browser
                      icon:(SkBitmap)icon;

// Action for close button.
- (IBAction)closeWindow:(id)sender;

// From NSTextViewDelegate:
- (BOOL)textView:(NSTextView*)aTextView
   clickedOnLink:(id)link
         atIndex:(NSUInteger)charIndex;

// Displays the extension installed bubble. This callback is triggered by
// the extensionObserver when the extension has completed loading.
- (void)showWindow:(id)sender;

// Clears our weak pointer to the Extension. This callback is triggered by
// the extensionObserver when the extension is unloaded.
- (void)extensionUnloaded:(id)sender;

// Opens the shortcut configuration UI.
- (IBAction)onManageShortcutClicked:(id)sender;

// Shows the new app installed animation.
- (IBAction)onAppShortcutClicked:(id)sender;

@end

@interface ExtensionInstalledBubbleController (ExposedForTesting)

- (void)removePageActionPreviewIfNecessary;
- (NSWindow*)initializeWindow;
- (int)calculateWindowHeight;
- (void)setMessageFrames:(int)newWindowHeight;
- (NSRect)headingFrame;
- (NSRect)frameOfHowToUse;
- (NSRect)frameOfHowToManage;
- (NSRect)frameOfSigninPromo;
- (BOOL)showSyncPromo;
- (NSButton*)appInstalledShortcutLink;

@end  // ExtensionInstalledBubbleController(ExposedForTesting)

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_CONTROLLER_H_
