// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/base_bubble_controller.h"

class Browser;
class ExtensionInstalledBubble;
@class HyperlinkTextView;
@class HoverCloseButton;

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
enum ExtensionType {
  kApp,
  kExtension,
  kBundle,
};

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

  // A weak reference to the bubble. It's owned by the BubbleManager.
  ExtensionInstalledBubble* installedBubble_;

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
  IBOutlet NSView* installedItemsView_;
  IBOutlet NSTextField* failedHeadingMsg_;
  IBOutlet NSView* failedItemsView_;
}

@property(nonatomic, readonly) const extensions::BundleInstaller* bundle;
@property(nonatomic, readonly) ExtensionInstalledBubble* installedBubble;
@property(nonatomic) BOOL pageActionPreviewShowing;

// Initialize the window. It will be shown by the BubbleManager.
- (id)initWithParentWindow:(NSWindow*)parentWindow
           extensionBubble:(ExtensionInstalledBubble*)extensionBubble;

// Initialize the window, and show it. BubbleManager is not currently used for
// displaying the "Bundle Installed" bubble.
- (id)initWithParentWindow:(NSWindow*)parentWindow
                    bundle:(const extensions::BundleInstaller*)bundle
                   browser:(Browser*)browser;

// Action for close button.
- (IBAction)closeWindow:(id)sender;

// From NSTextViewDelegate:
- (BOOL)textView:(NSTextView*)aTextView
   clickedOnLink:(id)link
         atIndex:(NSUInteger)charIndex;

// Displays the extension installed bubble. This callback is triggered by
// the extensionObserver when the extension has completed loading.
- (void)showWindow:(id)sender;

// Opens the shortcut configuration UI.
- (IBAction)onManageShortcutClicked:(id)sender;

// Shows the new app installed animation.
- (IBAction)onAppShortcutClicked:(id)sender;

// Should be called by the extension bridge to close this window.
- (void)doClose;

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
- (NSButton*)appInstalledShortcutLink;
- (void)updateAnchorPosition;

@end  // ExtensionInstalledBubbleController(ExposedForTesting)

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_EXTENSION_INSTALLED_BUBBLE_CONTROLLER_H_
