// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "extension_installed_bubble_controller.h"

#include "app/l10n_util.h"
#include "base/mac_util.h"
#include "base/sys_string_conversions.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/cocoa/autocomplete_text_field_cell.h"
#include "chrome/browser/cocoa/browser_window_cocoa.h"
#include "chrome/browser/cocoa/browser_window_controller.h"
#include "chrome/browser/cocoa/extensions/browser_actions_controller.h"
#include "chrome/browser/cocoa/hover_close_button.h"
#include "chrome/browser/cocoa/info_bubble_view.h"
#include "chrome/browser/cocoa/location_bar_view_mac.h"
#include "chrome/browser/cocoa/toolbar_controller.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"
#import "skia/ext/skia_utils_mac.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"



// C++ class that receives EXTENSION_LOADED notifications and proxies them back
// to |controller|.
class ExtensionLoadedNotificationObserver : public NotificationObserver {
 public:
  ExtensionLoadedNotificationObserver(
      ExtensionInstalledBubbleController* controller)
          : controller_(controller) {
    // Create a registrar and add ourselves to it.
    registrar_.Add(this, NotificationType::EXTENSION_LOADED,
        NotificationService::AllSources());
  }

 private:
  // NotificationObserver implementation. Tells the controller to start showing
  // its window on the main thread when the extension has finished loading.
  void Observe(NotificationType type,
               const NotificationSource& source,
               const NotificationDetails& details) {
    if (type == NotificationType::EXTENSION_LOADED) {
      Extension* extension = Details<Extension>(details).ptr();
      if (extension == [controller_ extension]) {
        [controller_ performSelectorOnMainThread:@selector(showWindow:)
                                      withObject:controller_
                                   waitUntilDone:NO];
      }
    } else {
      NOTREACHED() << "Received unexpected notification.";
    }
  }

  NotificationRegistrar registrar_;
  ExtensionInstalledBubbleController* controller_;  // weak, owns us
};

@implementation ExtensionInstalledBubbleController

@synthesize extension = extension_;

- (id)initWithParentWindow:(NSWindow*)parentWindow
                 extension:(Extension*)extension
                   browser:(Browser*)browser
                      icon:(SkBitmap)icon {
  NSString* nibPath =
      [mac_util::MainAppBundle() pathForResource:@"ExtensionInstalledBubble"
                                          ofType:@"nib"];
  if ((self = [super initWithWindowNibPath:nibPath owner:self])) {
    parentWindow_ = parentWindow;
    extension_ = extension;
    browser_ = browser;
    icon_.reset([gfx::SkBitmapToNSImage(icon) retain]);

    if (extension->browser_action()) {
      type_ = extension_installed_bubble::kBrowserAction;
    } else if (extension->page_action() &&
               !extension->page_action()->default_icon_path().empty()) {
      type_ = extension_installed_bubble::kPageAction;
    } else {
      NOTREACHED();  // kGeneric installs handled in the extension_install_ui.
    }

    // Start showing window only after extension has fully loaded.
    extensionObserver_.reset(new ExtensionLoadedNotificationObserver(self));

    // Watch to see if the parent window closes, and close this one if so.
    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    [center addObserver:self
               selector:@selector(parentWindowWillClose:)
                   name:NSWindowWillCloseNotification
                 object:parentWindow_];
  }
  return self;
}

- (void)dealloc {
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)close {
  [parentWindow_ removeChildWindow:[self window]];
  [super close];
}

- (void)parentWindowWillClose:(NSNotification*)notification {
  [self close];
}

- (void)windowWillClose:(NSNotification*)notification {
  // Turn off page action icon preview when the window closes.
  if (extension_->page_action()) {
    [self removePageActionPreview];
  }
  // We caught a close so we don't need to watch for the parent closing.
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [self autorelease];
}

// The controller is the delegate of the window, so it receives "did resign
// key" notifications.  When key is resigned, close the window.
- (void)windowDidResignKey:(NSNotification*)notification {
  NSWindow* window = [self window];
  DCHECK_EQ([notification object], window);
  DCHECK([window isVisible]);
  [self close];
}

- (IBAction)closeWindow:(id)sender {
  DCHECK([[self window] isVisible]);
  [self close];
}

// Extracted to a function here so that it can be overwritten for unit
// testing.
- (void)removePageActionPreview {
  BrowserWindowCocoa* window = static_cast<BrowserWindowCocoa*>(
      browser_->window());
  LocationBarViewMac* locationBarView = static_cast<LocationBarViewMac*>(
      [[window->cocoa_controller() toolbarController] locationBarBridge]);

  locationBarView->SetPreviewEnabledPageAction(extension_->page_action(),
                                               false);  // disables preview.
  [locationBarView->GetAutocompleteTextField() setNeedsDisplay:YES];
}

// The extension installed bubble points at the browser action icon or the
// page action icon (shown as a preview), depending on the extension type.
// We need to calculate the location of these icons and the size of the
// message itself (which varies with the title of the extension) in order
// to figure out the origin point for the extension installed bubble.
// TODO(mirandac): add framework to easily test extension UI components!
- (NSPoint)calculateArrowPoint {
  BrowserWindowCocoa* window =
      static_cast<BrowserWindowCocoa*>(browser_->window());
  NSPoint arrowPoint;

  switch(type_) {
    case extension_installed_bubble::kBrowserAction: {
      // Find the center of the bottom of the browser action icon.
      NSView* button = [[[window->cocoa_controller() toolbarController]
          browserActionsController] browserActionViewForExtension:extension_];
      DCHECK(button);
      NSRect boundsRect = [[[button window] contentView]
          convertRect:[button frame]
             fromView:[button superview]];
      arrowPoint =
          NSMakePoint(NSMinX(boundsRect) + NSWidth([button frame]) / 2,
                      NSMinY(boundsRect));
      break;
    }
    case extension_installed_bubble::kPageAction: {
      LocationBarViewMac* locationBarView =
          static_cast<LocationBarViewMac*>(
              [[window->cocoa_controller() toolbarController]
               locationBarBridge]);
      // Tell the location bar to show a preview of the page action icon, which
      // would ordinarily only be displayed on a page of the appropriate type.
      // We remove this preview when the extension installed bubble closes.
      locationBarView->SetPreviewEnabledPageAction(extension_->page_action(),
                                                   true);

      // Find the center of the bottom of the page action icon.
      AutocompleteTextField* field =
          locationBarView->GetAutocompleteTextField();
      size_t index =
          locationBarView->GetPageActionIndex(extension_->page_action());
      NSView* browserContentWindow = [window->GetNativeHandle() contentView];
      NSRect iconRect = [[field autocompleteTextFieldCell]
          pageActionFrameForIndex:index inFrame:[field frame]];
      NSRect boundsrect = [browserContentWindow convertRect:iconRect
                                                   fromView:[field superview]];
      arrowPoint =
          NSMakePoint(NSMinX(boundsrect) - NSWidth(boundsrect) / 2 - 1,
                      NSMinY(boundsrect));
      break;
    }
    default: {
      NOTREACHED() << "Generic extension type not allowed in install bubble.";
    }
  }
  return arrowPoint;
}

// We want this to be a child of a browser window.  addChildWindow:
// (called from this function) will bring the window on-screen;
// unfortunately, [NSWindowController showWindow:] will also bring it
// on-screen (but will cause unexpected changes to the window's
// position).  We cannot have an addChildWindow: and a subsequent
// showWindow:. Thus, we have our own version.
- (void)showWindow:(id)sender {
  // Generic extensions get an infobar rather than a bubble.
  DCHECK(type_ != extension_installed_bubble::kGeneric);
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::UI));

  // Load nib and calculate height based on messages to be shown.
  NSWindow* window = [self initializeWindow];
  int newWindowHeight = [self calculateWindowHeight];
  NSSize windowDelta = NSMakeSize(
      0, newWindowHeight - NSHeight([[window contentView] bounds]));
  [infoBubbleView_ setFrameSize:NSMakeSize(
      NSWidth([[window contentView] bounds]), newWindowHeight)];
  NSRect newFrame = [window frame];
  newFrame.size.height += windowDelta.height;
  [window setFrame:newFrame display:NO];

  // Now that we have resized the window, adjust y pos of the messages.
  [self setMessageFrames:newWindowHeight];

  // Find window origin, taking into account bubble size and arrow location.
  NSPoint origin =
      [parentWindow_ convertBaseToScreen:[self calculateArrowPoint]];
  origin.x -= NSWidth([window frame]) - kBubbleArrowXOffset -
      kBubbleArrowWidth / 2;
  origin.y -= NSHeight([window frame]);
  [window setFrameOrigin:origin];

  [parentWindow_ addChildWindow:window
                        ordered:NSWindowAbove];
  [window makeKeyAndOrderFront:self];
}

// Finish nib loading, set arrow location and load icon into window.  This
// function is exposed for unit testing.
- (NSWindow*)initializeWindow {
  NSWindow* window = [self window];  // completes nib load
  [infoBubbleView_ setArrowLocation:kTopRight];

  // Set appropriate icon, resizing if necessary.
  if ([icon_ size].width > extension_installed_bubble::kIconSize) {
    [icon_ setSize:NSMakeSize(extension_installed_bubble::kIconSize,
                              extension_installed_bubble::kIconSize)];
  }
  [iconImage_ setImage:icon_];
  [iconImage_ setNeedsDisplay:YES];
  return window;
 }

// Calculate the height of each install message, resizing messages in their
// frames to fit window width.  Return the new window height, based on the
// total of all message heights.
- (int)calculateWindowHeight {
  // Adjust the window height to reflect the sum height of all messages
  // and vertical padding.
  int newWindowHeight = 2 * extension_installed_bubble::kOuterVerticalMargin;

  // First part of extension installed message.
  [extensionInstalledMsg_ setStringValue:l10n_util::GetNSStringF(
      IDS_EXTENSION_INSTALLED_HEADING, UTF8ToUTF16(extension_->name()))];
  [GTMUILocalizerAndLayoutTweaker
      sizeToFitFixedWidthTextField:extensionInstalledMsg_];
  newWindowHeight += [extensionInstalledMsg_ frame].size.height +
      extension_installed_bubble::kInnerVerticalMargin;

  // If type is page action, include a special message about page actions.
  if (type_ == extension_installed_bubble::kPageAction) {
    [pageActionInfoMsg_ setHidden:NO];
    [[pageActionInfoMsg_ cell]
        setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
    [GTMUILocalizerAndLayoutTweaker
        sizeToFitFixedWidthTextField:pageActionInfoMsg_];
    newWindowHeight += [pageActionInfoMsg_ frame].size.height +
        extension_installed_bubble::kInnerVerticalMargin;
  }

  // Second part of extension installed message.
  [[extensionInstalledInfoMsg_ cell]
      setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
  [GTMUILocalizerAndLayoutTweaker
      sizeToFitFixedWidthTextField:extensionInstalledInfoMsg_];
  newWindowHeight += [extensionInstalledInfoMsg_ frame].size.height;

  return newWindowHeight;
}

// Adjust y-position of messages to sit properly in new window height.
- (void)setMessageFrames:(int)newWindowHeight {
  // The extension messages will always be shown.
  NSRect extensionMessageFrame1 = [extensionInstalledMsg_ frame];
  NSRect extensionMessageFrame2 = [extensionInstalledInfoMsg_ frame];

  extensionMessageFrame1.origin.y = newWindowHeight - (
      extensionMessageFrame1.size.height +
      extension_installed_bubble::kOuterVerticalMargin);
  [extensionInstalledMsg_ setFrame:extensionMessageFrame1];
  if (type_ == extension_installed_bubble::kPageAction) {
    // The page action message is only shown when appropriate.
    NSRect pageActionMessageFrame = [pageActionInfoMsg_ frame];
    pageActionMessageFrame.origin.y = extensionMessageFrame1.origin.y - (
        pageActionMessageFrame.size.height +
        extension_installed_bubble::kInnerVerticalMargin);
    [pageActionInfoMsg_ setFrame:pageActionMessageFrame];
    extensionMessageFrame2.origin.y = pageActionMessageFrame.origin.y - (
        extensionMessageFrame2.size.height +
        extension_installed_bubble::kInnerVerticalMargin);
  } else {
    extensionMessageFrame2.origin.y = extensionMessageFrame1.origin.y - (
        extensionMessageFrame2.size.height +
        extension_installed_bubble::kInnerVerticalMargin);
  }
  [extensionInstalledInfoMsg_ setFrame:extensionMessageFrame2];
}

// Exposed for unit testing.
- (NSRect)getExtensionInstalledMsgFrame {
  return [extensionInstalledMsg_ frame];
}

- (NSRect)getPageActionInfoMsgFrame {
  return [pageActionInfoMsg_ frame];
}

- (NSRect)getExtensionInstalledInfoMsgFrame {
  return [extensionInstalledInfoMsg_ frame];
}

@end
