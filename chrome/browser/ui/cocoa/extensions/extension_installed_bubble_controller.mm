// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/extension_installed_bubble_controller.h"

#include "base/i18n/rtl.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/api/commands/command_service_factory.h"
#include "chrome/browser/extensions/bundle_installer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/browser_window_cocoa.h"
#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/extensions/browser_actions_controller.h"
#include "chrome/browser/ui/cocoa/hover_close_button.h"
#include "chrome/browser/ui/cocoa/info_bubble_view.h"
#include "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_action.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#import "skia/ext/skia_utils_mac.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/l10n/l10n_util.h"

using content::BrowserThread;
using extensions::BundleInstaller;
using extensions::Extension;
using extensions::UnloadedExtensionInfo;

// C++ class that receives EXTENSION_LOADED notifications and proxies them back
// to |controller|.
class ExtensionLoadedNotificationObserver
    : public content::NotificationObserver {
 public:
  ExtensionLoadedNotificationObserver(
      ExtensionInstalledBubbleController* controller, Profile* profile)
          : controller_(controller) {
    registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_LOADED,
        content::Source<Profile>(profile));
    registrar_.Add(this, chrome::NOTIFICATION_EXTENSION_UNLOADED,
        content::Source<Profile>(profile));
  }

 private:
  // NotificationObserver implementation. Tells the controller to start showing
  // its window on the main thread when the extension has finished loading.
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) {
    if (type == chrome::NOTIFICATION_EXTENSION_LOADED) {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      if (extension == [controller_ extension]) {
        [controller_ performSelectorOnMainThread:@selector(showWindow:)
                                      withObject:controller_
                                   waitUntilDone:NO];
      }
    } else if (type == chrome::NOTIFICATION_EXTENSION_UNLOADED) {
      const Extension* extension =
          content::Details<const UnloadedExtensionInfo>(details)->extension;
      if (extension == [controller_ extension]) {
        [controller_ performSelectorOnMainThread:@selector(extensionUnloaded:)
                                      withObject:controller_
                                   waitUntilDone:NO];
      }
    } else {
      NOTREACHED() << "Received unexpected notification.";
    }
  }

  content::NotificationRegistrar registrar_;
  ExtensionInstalledBubbleController* controller_;  // weak, owns us
};

@implementation ExtensionInstalledBubbleController

@synthesize extension = extension_;
@synthesize bundle = bundle_;
@synthesize pageActionRemoved = pageActionRemoved_;  // Exposed for unit test.

- (id)initWithParentWindow:(NSWindow*)parentWindow
                 extension:(const Extension*)extension
                    bundle:(const BundleInstaller*)bundle
                   browser:(Browser*)browser
                      icon:(SkBitmap)icon {
  NSString* nibName = bundle ? @"ExtensionInstalledBubbleBundle" :
                               @"ExtensionInstalledBubble";
  if ((self = [super initWithWindowNibPath:nibName
                              parentWindow:parentWindow
                                anchoredAt:NSZeroPoint])) {
    extension_ = extension;
    bundle_ = bundle;
    DCHECK(browser);
    browser_ = browser;
    icon_.reset([gfx::SkBitmapToNSImage(icon) retain]);
    pageActionRemoved_ = NO;

    if (bundle_) {
      type_ = extension_installed_bubble::kBundle;
    } else if (!extension->omnibox_keyword().empty()) {
      type_ = extension_installed_bubble::kOmniboxKeyword;
    } else if (extension->browser_action()) {
      type_ = extension_installed_bubble::kBrowserAction;
    } else if (extension->page_action() &&
             extension->is_verbose_install_message()) {
      type_ = extension_installed_bubble::kPageAction;
    } else {
      type_ = extension_installed_bubble::kGeneric;
    }

    if (type_ == extension_installed_bubble::kBundle) {
      [self showWindow:self];
    } else {
      // Start showing window only after extension has fully loaded.
      extensionObserver_.reset(new ExtensionLoadedNotificationObserver(
          self, browser->profile()));
    }
  }
  return self;
}

- (void)windowWillClose:(NSNotification*)notification {
  // Turn off page action icon preview when the window closes, unless we
  // already removed it when the window resigned key status.
  [self removePageActionPreviewIfNecessary];
  extension_ = NULL;
  browser_ = NULL;

  [super windowWillClose:notification];
}

// The controller is the delegate of the window, so it receives "did resign
// key" notifications.  When key is resigned, close the window.
- (void)windowDidResignKey:(NSNotification*)notification {
  // If the browser window is closing, we need to remove the page action
  // immediately, otherwise the closing animation may overlap with
  // browser destruction.
  [self removePageActionPreviewIfNecessary];
  [super windowDidResignKey:notification];
}

- (IBAction)closeWindow:(id)sender {
  DCHECK([[self window] isVisible]);
  [self close];
}

// Extracted to a function here so that it can be overwritten for unit
// testing.
- (void)removePageActionPreviewIfNecessary {
  if (!extension_ || !extension_->page_action() || pageActionRemoved_)
    return;
  pageActionRemoved_ = YES;

  BrowserWindowCocoa* window =
      static_cast<BrowserWindowCocoa*>(browser_->window());
  LocationBarViewMac* locationBarView =
      [window->cocoa_controller() locationBarBridge];
  locationBarView->SetPreviewEnabledPageAction(extension_->page_action(),
                                               false);  // disables preview.
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
  NSPoint arrowPoint = NSZeroPoint;

  switch(type_) {
    case extension_installed_bubble::kOmniboxKeyword: {
      LocationBarViewMac* locationBarView =
          [window->cocoa_controller() locationBarBridge];
      arrowPoint = locationBarView->GetPageInfoBubblePoint();
      break;
    }
    case extension_installed_bubble::kBrowserAction: {
      BrowserActionsController* controller =
          [[window->cocoa_controller() toolbarController]
              browserActionsController];
      arrowPoint = [controller popupPointForBrowserAction:extension_];
      break;
    }
    case extension_installed_bubble::kPageAction: {
      LocationBarViewMac* locationBarView =
          [window->cocoa_controller() locationBarBridge];

      // Tell the location bar to show a preview of the page action icon, which
      // would ordinarily only be displayed on a page of the appropriate type.
      // We remove this preview when the extension installed bubble closes.
      locationBarView->SetPreviewEnabledPageAction(extension_->page_action(),
                                                   true);

      // Find the center of the bottom of the page action icon.
      arrowPoint =
          locationBarView->GetPageActionBubblePoint(extension_->page_action());
      break;
    }
    case extension_installed_bubble::kBundle:
    case extension_installed_bubble::kGeneric: {
      // Point at the bottom of the wrench menu.
      NSView* wrenchButton =
          [[window->cocoa_controller() toolbarController] wrenchButton];
      const NSRect bounds = [wrenchButton bounds];
      NSPoint anchor = NSMakePoint(NSMidX(bounds), NSMaxY(bounds));
      arrowPoint = [wrenchButton convertPoint:anchor toView:nil];
      break;
    }
    default: {
      NOTREACHED();
    }
  }
  return arrowPoint;
}

// Override -[BaseBubbleController showWindow:] to tweak bubble location and
// set up UI elements.
- (void)showWindow:(id)sender {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Load nib and calculate height based on messages to be shown.
  NSWindow* window = [self initializeWindow];
  int newWindowHeight = [self calculateWindowHeight];
  [self.bubble setFrameSize:NSMakeSize(
      NSWidth([[window contentView] bounds]), newWindowHeight)];
  NSSize windowDelta = NSMakeSize(
      0, newWindowHeight - NSHeight([[window contentView] bounds]));
  windowDelta = [[window contentView] convertSize:windowDelta toView:nil];
  NSRect newFrame = [window frame];
  newFrame.size.height += windowDelta.height;
  [window setFrame:newFrame display:NO];

  // Now that we have resized the window, adjust y pos of the messages.
  [self setMessageFrames:newWindowHeight];

  // Find window origin, taking into account bubble size and arrow location.
  self.anchorPoint =
      [self.parentWindow convertBaseToScreen:[self calculateArrowPoint]];
  [super showWindow:sender];
}

// Finish nib loading, set arrow location and load icon into window.  This
// function is exposed for unit testing.
- (NSWindow*)initializeWindow {
  NSWindow* window = [self window];  // completes nib load

  if (type_ == extension_installed_bubble::kOmniboxKeyword) {
    [self.bubble setArrowLocation:info_bubble::kTopLeft];
  } else {
    [self.bubble setArrowLocation:info_bubble::kTopRight];
  }

  if (type_ == extension_installed_bubble::kBundle)
    return window;

  // Set appropriate icon, resizing if necessary.
  if ([icon_ size].width > extension_installed_bubble::kIconSize) {
    [icon_ setSize:NSMakeSize(extension_installed_bubble::kIconSize,
                              extension_installed_bubble::kIconSize)];
  }
  [iconImage_ setImage:icon_];
  [iconImage_ setNeedsDisplay:YES];
  return window;
}

- (bool)hasActivePageAction:(extensions::Command*)command {
  extensions::CommandService* command_service =
      extensions::CommandServiceFactory::GetForProfile(browser_->profile());
  if (type_ == extension_installed_bubble::kPageAction) {
    if (extension_->page_action_command() &&
        command_service->GetPageActionCommand(
            extension_->id(),
            extensions::CommandService::ACTIVE_ONLY,
            command,
            NULL)) {
      return true;
    }
  }

  return false;
}

- (bool)hasActiveBrowserAction:(extensions::Command*)command {
  extensions::CommandService* command_service =
      extensions::CommandServiceFactory::GetForProfile(browser_->profile());
  if (type_ == extension_installed_bubble::kBrowserAction) {
    if (extension_->browser_action_command() &&
        command_service->GetBrowserActionCommand(
            extension_->id(),
            extensions::CommandService::ACTIVE_ONLY,
            command,
            NULL)) {
      return true;
    }
  }

  return false;
}

- (NSString*)installMessageForCurrentExtensionAction {
  if (type_ == extension_installed_bubble::kPageAction) {
    extensions::Command page_action_command;
    if ([self hasActivePageAction:&page_action_command]) {
      return l10n_util::GetNSStringF(
          IDS_EXTENSION_INSTALLED_PAGE_ACTION_INFO_WITH_SHORTCUT,
          page_action_command.accelerator().GetShortcutText());
    } else {
      return l10n_util::GetNSString(
          IDS_EXTENSION_INSTALLED_PAGE_ACTION_INFO);
    }
  } else {
    CHECK_EQ(extension_installed_bubble::kBrowserAction, type_);
    extensions::Command browser_action_command;
    if ([self hasActiveBrowserAction:&browser_action_command]) {
      return l10n_util::GetNSStringF(
          IDS_EXTENSION_INSTALLED_BROWSER_ACTION_INFO_WITH_SHORTCUT,
          browser_action_command.accelerator().GetShortcutText());
    } else {
      return l10n_util::GetNSString(
          IDS_EXTENSION_INSTALLED_BROWSER_ACTION_INFO);
    }
  }
}

// Calculate the height of each install message, resizing messages in their
// frames to fit window width.  Return the new window height, based on the
// total of all message heights.
- (int)calculateWindowHeight {
  // Adjust the window height to reflect the sum height of all messages
  // and vertical padding.
  int newWindowHeight = 2 * extension_installed_bubble::kOuterVerticalMargin;

  // First part of extension installed message.
  if (type_ != extension_installed_bubble::kBundle) {
    string16 extension_name = UTF8ToUTF16(extension_->name().c_str());
    base::i18n::AdjustStringForLocaleDirection(&extension_name);
    [extensionInstalledMsg_ setStringValue:l10n_util::GetNSStringF(
        IDS_EXTENSION_INSTALLED_HEADING, extension_name)];
    [GTMUILocalizerAndLayoutTweaker
      sizeToFitFixedWidthTextField:extensionInstalledMsg_];
    newWindowHeight += [extensionInstalledMsg_ frame].size.height +
        extension_installed_bubble::kInnerVerticalMargin;
  }

  // If type is page action, include a special message about page actions.
  if (type_ == extension_installed_bubble::kBrowserAction ||
      type_ == extension_installed_bubble::kPageAction) {
    [extraInfoMsg_ setStringValue:[self
        installMessageForCurrentExtensionAction]];
    [extraInfoMsg_ setHidden:NO];
    [[extraInfoMsg_ cell]
        setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
    [GTMUILocalizerAndLayoutTweaker
        sizeToFitFixedWidthTextField:extraInfoMsg_];
    newWindowHeight += [extraInfoMsg_ frame].size.height +
        extension_installed_bubble::kInnerVerticalMargin;
  }

  // If type is omnibox keyword, include a special message about the keyword.
  if (type_ == extension_installed_bubble::kOmniboxKeyword) {
    [extraInfoMsg_ setStringValue:l10n_util::GetNSStringF(
        IDS_EXTENSION_INSTALLED_OMNIBOX_KEYWORD_INFO,
        UTF8ToUTF16(extension_->omnibox_keyword()))];
    [extraInfoMsg_ setHidden:NO];
    [[extraInfoMsg_ cell]
        setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
    [GTMUILocalizerAndLayoutTweaker
        sizeToFitFixedWidthTextField:extraInfoMsg_];
    newWindowHeight += [extraInfoMsg_ frame].size.height +
        extension_installed_bubble::kInnerVerticalMargin;
  }

  // If type is bundle, list the extensions that were installed and those that
  // failed.
  if (type_ == extension_installed_bubble::kBundle) {
    NSInteger installedListHeight =
        [self addExtensionList:installedHeadingMsg_
                      itemsMsg:installedItemsMsg_
                         state:BundleInstaller::Item::STATE_INSTALLED];

    NSInteger failedListHeight =
        [self addExtensionList:failedHeadingMsg_
                      itemsMsg:failedItemsMsg_
                         state:BundleInstaller::Item::STATE_FAILED];

    newWindowHeight += installedListHeight + failedListHeight;

    // Put some space between the lists if both are present.
    if (installedListHeight > 0 && failedListHeight > 0)
      newWindowHeight += extension_installed_bubble::kInnerVerticalMargin;
  } else {
    // Second part of extension installed message.
    [[extensionInstalledInfoMsg_ cell]
        setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
    [GTMUILocalizerAndLayoutTweaker
        sizeToFitFixedWidthTextField:extensionInstalledInfoMsg_];
    newWindowHeight += [extensionInstalledInfoMsg_ frame].size.height;
  }

  extensions::Command command;
  if ([self hasActivePageAction:&command] ||
      [self hasActiveBrowserAction:&command]) {
    [manageShortcutLink_ setHidden:NO];
    [[manageShortcutLink_ cell]
        setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
    newWindowHeight += 2 * extension_installed_bubble::kInnerVerticalMargin;
    newWindowHeight += [GTMUILocalizerAndLayoutTweaker
                            sizeToFitView:manageShortcutLink_].height;
    newWindowHeight += extension_installed_bubble::kInnerVerticalMargin;
  }

  return newWindowHeight;
}

- (NSInteger)addExtensionList:(NSTextField*)headingMsg
                     itemsMsg:(NSTextField*)itemsMsg
                        state:(BundleInstaller::Item::State)state {
  string16 heading = bundle_->GetHeadingTextFor(state);
  bool hidden = heading.empty();
  [headingMsg setHidden:hidden];
  [itemsMsg setHidden:hidden];
  if (hidden)
    return 0;

  [headingMsg setStringValue:base::SysUTF16ToNSString(heading)];
  [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:headingMsg];

  NSMutableString* joinedItems = [NSMutableString string];
  BundleInstaller::ItemList items = bundle_->GetItemsWithState(state);
  for (size_t i = 0; i < items.size(); ++i) {
    if (i > 0)
      [joinedItems appendString:@"\n"];
    [joinedItems appendString:base::SysUTF16ToNSString(
        items[i].GetNameForDisplay())];
  }

  [itemsMsg setStringValue:joinedItems];
  [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:itemsMsg];

  return [headingMsg frame].size.height +
      extension_installed_bubble::kInnerVerticalMargin +
      [itemsMsg frame].size.height;
}

// Adjust y-position of messages to sit properly in new window height.
- (void)setMessageFrames:(int)newWindowHeight {
  if (type_ == extension_installed_bubble::kBundle) {
    // Layout the messages from the bottom up.
    NSTextField* msgs[] = { failedItemsMsg_, failedHeadingMsg_,
                            installedItemsMsg_, installedHeadingMsg_ };
    NSInteger offsetFromBottom = 0;
    BOOL isFirstVisible = YES;
    for (size_t i = 0; i < arraysize(msgs); ++i) {
      if ([msgs[i] isHidden])
        continue;

      NSRect frame = [msgs[i] frame];
      NSInteger margin = isFirstVisible ?
          extension_installed_bubble::kOuterVerticalMargin :
          extension_installed_bubble::kInnerVerticalMargin;

      frame.origin.y = offsetFromBottom + margin;
      [msgs[i] setFrame:frame];
      offsetFromBottom += frame.size.height + margin;

      isFirstVisible = NO;
    }

    // Move the close button a bit to vertically align it with the heading.
    NSInteger closeButtonFudge = 1;
    NSRect frame = [closeButton_ frame];
    frame.origin.y = newWindowHeight - (frame.size.height + closeButtonFudge +
         extension_installed_bubble::kOuterVerticalMargin);
    [closeButton_ setFrame:frame];

    return;
  }

  NSRect extensionMessageFrame1 = [extensionInstalledMsg_ frame];
  NSRect extensionMessageFrame2 = [extensionInstalledInfoMsg_ frame];

  extensionMessageFrame1.origin.y = newWindowHeight - (
      extensionMessageFrame1.size.height +
      extension_installed_bubble::kOuterVerticalMargin);
  [extensionInstalledMsg_ setFrame:extensionMessageFrame1];
  if (extension_->is_verbose_install_message()) {
    // The extra message is only shown when appropriate.
    NSRect extraMessageFrame = [extraInfoMsg_ frame];
    extraMessageFrame.origin.y = extensionMessageFrame1.origin.y - (
        extraMessageFrame.size.height +
        extension_installed_bubble::kInnerVerticalMargin);
    [extraInfoMsg_ setFrame:extraMessageFrame];
    extensionMessageFrame2.origin.y = extraMessageFrame.origin.y - (
        extensionMessageFrame2.size.height +
        extension_installed_bubble::kInnerVerticalMargin);
  } else {
    extensionMessageFrame2.origin.y = extensionMessageFrame1.origin.y - (
        extensionMessageFrame2.size.height +
        extension_installed_bubble::kInnerVerticalMargin);
  }
  [extensionInstalledInfoMsg_ setFrame:extensionMessageFrame2];

  extensions::Command command;
  if (![manageShortcutLink_ isHidden]) {
    NSRect manageShortcutFrame = [manageShortcutLink_ frame];
    manageShortcutFrame.origin.y = NSMinY(extensionMessageFrame2) - (
        NSHeight(manageShortcutFrame) +
        extension_installed_bubble::kInnerVerticalMargin);
    // Right-align the link.
    manageShortcutFrame.origin.x = NSMaxX(extensionMessageFrame2) -
                                   NSWidth(manageShortcutFrame);
    [manageShortcutLink_ setFrame:manageShortcutFrame];
  }
}

// Exposed for unit testing.
- (NSRect)getExtensionInstalledMsgFrame {
  return [extensionInstalledMsg_ frame];
}

- (NSRect)getExtraInfoMsgFrame {
  return [extraInfoMsg_ frame];
}

- (NSRect)getExtensionInstalledInfoMsgFrame {
  return [extensionInstalledInfoMsg_ frame];
}

- (void)extensionUnloaded:(id)sender {
  extension_ = NULL;
}

- (IBAction)onManageShortcutClicked:(id)sender {
  [self close];
  std::string configure_url = chrome::kChromeUIExtensionsURL;
  configure_url += chrome::kExtensionConfigureCommandsSubPage;
  chrome::NavigateParams params(chrome::GetSingletonTabNavigateParams(
      browser_, GURL(configure_url)));
  chrome::Navigate(&params);
}

@end
