// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/extensions/extension_installed_bubble_controller.h"

#include "base/i18n/rtl.h"
#include "base/mac/bundle_locations.h"
#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/commands/command_service.h"
#include "chrome/browser/extensions/bundle_installer.h"
#include "chrome/browser/extensions/extension_action.h"
#include "chrome/browser/extensions/extension_action_manager.h"
#include "chrome/browser/extensions/extension_install_ui.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_style.h"
#include "chrome/browser/ui/cocoa/browser_window_cocoa.h"
#include "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/extensions/browser_actions_controller.h"
#include "chrome/browser/ui/cocoa/hover_close_button.h"
#include "chrome/browser/ui/cocoa/info_bubble_view.h"
#include "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/browser/ui/cocoa/new_tab_button.h"
#include "chrome/browser/ui/cocoa/tabs/tab_strip_view.h"
#include "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/common/extensions/api/commands/commands_handler.h"
#include "chrome/common/extensions/api/extension_action/action_info.h"
#include "chrome/common/extensions/api/omnibox/omnibox_handler.h"
#include "chrome/common/extensions/sync_helper.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "extensions/common/extension.h"
#import "skia/ext/skia_utils_mac.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#import "ui/base/cocoa/controls/hyperlink_text_view.h"
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
    registrar_.Add(this,
                   extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED,
                   content::Source<Profile>(profile));
    registrar_.Add(this,
                   extensions::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED,
                   content::Source<Profile>(profile));
  }

 private:
  // NotificationObserver implementation. Tells the controller to start showing
  // its window on the main thread when the extension has finished loading.
  virtual void Observe(
      int type,
      const content::NotificationSource& source,
      const content::NotificationDetails& details) OVERRIDE {
    if (type == extensions::NOTIFICATION_EXTENSION_LOADED_DEPRECATED) {
      const Extension* extension =
          content::Details<const Extension>(details).ptr();
      if (extension == [controller_ extension]) {
        [controller_ performSelectorOnMainThread:@selector(showWindow:)
                                      withObject:controller_
                                   waitUntilDone:NO];
      }
    } else if (type == extensions::NOTIFICATION_EXTENSION_UNLOADED_DEPRECATED) {
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
// Exposed for unit test.
@synthesize pageActionPreviewShowing = pageActionPreviewShowing_;

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
    pageActionPreviewShowing_ = NO;

    if (bundle_) {
      type_ = extension_installed_bubble::kBundle;
    } else if (extension->is_app()) {
      type_ = extension_installed_bubble::kApp;
    } else if (!extensions::OmniboxInfo::GetKeyword(extension).empty()) {
      type_ = extension_installed_bubble::kOmniboxKeyword;
    } else if (extensions::ActionInfo::GetBrowserActionInfo(extension)) {
      type_ = extension_installed_bubble::kBrowserAction;
    } else if (extensions::ActionInfo::GetPageActionInfo(extension) &&
               extensions::ActionInfo::IsVerboseInstallMessage(extension)) {
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

// Sets |promo_| based on |promoPlaceholder_|, sets |promoPlaceholder_| to nil.
- (void)initializeLabel {
 // Replace the promo placeholder NSTextField with the real label NSTextView.
 // The former doesn't show links in a nice way, but the latter can't be added
 // in IB without a containing scroll view, so create the NSTextView
 // programmatically.
 promo_.reset([[HyperlinkTextView alloc]
     initWithFrame:[promoPlaceholder_ frame]]);
 [promo_.get() setAutoresizingMask:[promoPlaceholder_ autoresizingMask]];
 [[promoPlaceholder_ superview]
     replaceSubview:promoPlaceholder_ with:promo_.get()];
 promoPlaceholder_ = nil;  // Now released.
 [promo_.get() setDelegate:self];
}

// Returns YES if the sync promo should be shown in the bubble.
- (BOOL)showSyncPromo {
  return extensions::sync_helper::IsSyncableExtension(extension_) &&
         SyncPromoUI::ShouldShowSyncPromo(browser_->profile());
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

- (BOOL)textView:(NSTextView*)aTextView
   clickedOnLink:(id)link
         atIndex:(NSUInteger)charIndex {
  DCHECK_EQ(promo_.get(), aTextView);
  GURL promo_url =
      signin::GetPromoURL(signin::SOURCE_EXTENSION_INSTALL_BUBBLE, false);
  chrome::NavigateParams params(
      chrome::GetSingletonTabNavigateParams(browser_, promo_url));
  chrome::Navigate(&params);
  return YES;
}

// Extracted to a function here so that it can be overridden for unit testing.
- (void)removePageActionPreviewIfNecessary {
  if (!extension_ || !pageActionPreviewShowing_)
    return;
  ExtensionAction* page_action =
      extensions::ExtensionActionManager::Get(browser_->profile())->
      GetPageAction(*extension_);
  if (!page_action)
    return;
  pageActionPreviewShowing_ = NO;

  BrowserWindowCocoa* window =
      static_cast<BrowserWindowCocoa*>(browser_->window());
  LocationBarViewMac* locationBarView =
      [window->cocoa_controller() locationBarBridge];
  locationBarView->SetPreviewEnabledPageAction(page_action,
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
    case extension_installed_bubble::kApp: {
      TabStripView* view = [window->cocoa_controller() tabStripView];
      NewTabButton* button = [view getNewTabButton];
      NSRect bounds = [button bounds];
      NSPoint anchor = NSMakePoint(
          NSMidX(bounds),
          NSMaxY(bounds) - extension_installed_bubble::kAppsBubbleArrowOffset);
      arrowPoint = [button convertPoint:anchor toView:nil];
      break;
    }
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

      ExtensionAction* page_action =
          extensions::ExtensionActionManager::Get(browser_->profile())->
          GetPageAction(*extension_);

      // Tell the location bar to show a preview of the page action icon, which
      // would ordinarily only be displayed on a page of the appropriate type.
      // We remove this preview when the extension installed bubble closes.
      locationBarView->SetPreviewEnabledPageAction(page_action, true);
      pageActionPreviewShowing_ = YES;

      // Find the center of the bottom of the page action icon.
      arrowPoint =
          locationBarView->GetPageActionBubblePoint(page_action);
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

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
      extensions::CommandService::Get(browser_->profile());
  if (type_ == extension_installed_bubble::kPageAction) {
    if (extensions::CommandsInfo::GetPageActionCommand(extension_) &&
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
      extensions::CommandService::Get(browser_->profile());
  if (type_ == extension_installed_bubble::kBrowserAction) {
    if (extensions::CommandsInfo::GetBrowserActionCommand(extension_) &&
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

    return newWindowHeight;
  }

  int sync_promo_height = 0;
  if ([self showSyncPromo]) {
    // First calculate the height of the sign-in promo.
    NSFont* font = [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];

    NSString* link(l10n_util::GetNSStringWithFixup(
        IDS_EXTENSION_INSTALLED_SIGNIN_PROMO_LINK));
    NSString* message(l10n_util::GetNSStringWithFixup(
        IDS_EXTENSION_INSTALLED_SIGNIN_PROMO));

    HyperlinkTextView* view = promo_.get();
    [view setMessageAndLink:message
                   withLink:link
                   atOffset:0
                       font:font
               messageColor:[NSColor blackColor]
                  linkColor:gfx::SkColorToCalibratedNSColor(
                                chrome_style::GetLinkColor())];

    // HACK! The TextView does not report correct height even after you stuff
    // it with text (it tells you it is single-line even if it is multiline), so
    // here the hidden howToUse_ TextField is temporarily repurposed to
    // calculate the correct height for the TextView.
    [[howToUse_ cell] setAttributedStringValue:[promo_ attributedString]];
    [GTMUILocalizerAndLayoutTweaker
          sizeToFitFixedWidthTextField:howToUse_];
    sync_promo_height = NSHeight([howToUse_ frame]);
  }

  // First part of extension installed message, the heading.
  base::string16 extension_name = base::UTF8ToUTF16(extension_->name().c_str());
  base::i18n::AdjustStringForLocaleDirection(&extension_name);
  [heading_ setStringValue:l10n_util::GetNSStringF(
      IDS_EXTENSION_INSTALLED_HEADING, extension_name)];
  [GTMUILocalizerAndLayoutTweaker
      sizeToFitFixedWidthTextField:heading_];
  newWindowHeight += NSHeight([heading_ frame]) +
      extension_installed_bubble::kInnerVerticalMargin;

  // If type is browser/page action, include a special message about them.
  if (type_ == extension_installed_bubble::kBrowserAction ||
      type_ == extension_installed_bubble::kPageAction) {
    [howToUse_ setStringValue:[self
        installMessageForCurrentExtensionAction]];
    [howToUse_ setHidden:NO];
    [[howToUse_ cell]
        setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
    [GTMUILocalizerAndLayoutTweaker
        sizeToFitFixedWidthTextField:howToUse_];
    newWindowHeight += NSHeight([howToUse_ frame]) +
        extension_installed_bubble::kInnerVerticalMargin;
  }

  // If type is omnibox keyword, include a special message about the keyword.
  if (type_ == extension_installed_bubble::kOmniboxKeyword) {
    [howToUse_ setStringValue:l10n_util::GetNSStringF(
        IDS_EXTENSION_INSTALLED_OMNIBOX_KEYWORD_INFO,
        base::UTF8ToUTF16(extensions::OmniboxInfo::GetKeyword(extension_)))];
    [howToUse_ setHidden:NO];
    [[howToUse_ cell]
        setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
    [GTMUILocalizerAndLayoutTweaker
        sizeToFitFixedWidthTextField:howToUse_];
    newWindowHeight += NSHeight([howToUse_ frame]) +
        extension_installed_bubble::kInnerVerticalMargin;
  }

  // If type is app, hide howToManage_, and include a "show me" link in the
  // bubble.
  if (type_ == extension_installed_bubble::kApp) {
    [howToManage_ setHidden:YES];
    [appShortcutLink_ setHidden:NO];
    newWindowHeight += 2 * extension_installed_bubble::kInnerVerticalMargin;
    newWindowHeight += NSHeight([appShortcutLink_ frame]);
  } else {
    // Second part of extension installed message.
    [[howToManage_ cell]
        setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
    [GTMUILocalizerAndLayoutTweaker
        sizeToFitFixedWidthTextField:howToManage_];
    newWindowHeight += NSHeight([howToManage_ frame]);
  }

  // Sync sign-in promo, if any.
  if (sync_promo_height > 0) {
    NSRect promo_frame = [promo_.get() frame];
    promo_frame.size.height = sync_promo_height;
    [promo_.get() setFrame:promo_frame];
    newWindowHeight += extension_installed_bubble::kInnerVerticalMargin;
    newWindowHeight += sync_promo_height;
  }

  extensions::Command command;
  if ([self hasActivePageAction:&command] ||
      [self hasActiveBrowserAction:&command]) {
    [manageShortcutLink_ setHidden:NO];
    [[manageShortcutLink_ cell]
        setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
    [[manageShortcutLink_ cell]
        setTextColor:gfx::SkColorToCalibratedNSColor(
            chrome_style::GetLinkColor())];
    [GTMUILocalizerAndLayoutTweaker sizeToFitView:manageShortcutLink_];
    newWindowHeight += extension_installed_bubble::kInnerVerticalMargin;
    newWindowHeight += NSHeight([manageShortcutLink_ frame]);
  }

  return newWindowHeight;
}

- (NSInteger)addExtensionList:(NSTextField*)headingMsg
                     itemsMsg:(NSTextField*)itemsMsg
                        state:(BundleInstaller::Item::State)state {
  base::string16 heading = bundle_->GetHeadingTextFor(state);
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

  return NSHeight([headingMsg frame]) +
      extension_installed_bubble::kInnerVerticalMargin +
      NSHeight([itemsMsg frame]);
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
      offsetFromBottom += NSHeight(frame) + margin;

      isFirstVisible = NO;
    }

    // Move the close button a bit to vertically align it with the heading.
    NSInteger closeButtonFudge = 1;
    NSRect frame = [closeButton_ frame];
    frame.origin.y = newWindowHeight - (NSHeight(frame) + closeButtonFudge +
         extension_installed_bubble::kOuterVerticalMargin);
    [closeButton_ setFrame:frame];

    return;
  }

  NSRect headingFrame = [heading_ frame];
  headingFrame.origin.y = newWindowHeight - (
      NSHeight(headingFrame) +
      extension_installed_bubble::kOuterVerticalMargin);
  [heading_ setFrame:headingFrame];

  NSRect howToManageFrame = [howToManage_ frame];
  if (!extensions::OmniboxInfo::GetKeyword(extension_).empty() ||
      extensions::ActionInfo::GetBrowserActionInfo(extension_) ||
      extensions::ActionInfo::IsVerboseInstallMessage(extension_)) {
    // For browser actions, page actions and omnibox keyword show the
    // 'how to use' message before the 'how to manage' message.
    NSRect howToUseFrame = [howToUse_ frame];
    howToUseFrame.origin.y = headingFrame.origin.y - (
        NSHeight(howToUseFrame) +
        extension_installed_bubble::kInnerVerticalMargin);
    [howToUse_ setFrame:howToUseFrame];

    howToManageFrame.origin.y = howToUseFrame.origin.y - (
        NSHeight(howToManageFrame) +
        extension_installed_bubble::kInnerVerticalMargin);
  } else {
    howToManageFrame.origin.y = NSMinY(headingFrame) - (
        NSHeight(howToManageFrame) +
        extension_installed_bubble::kInnerVerticalMargin);
  }
  [howToManage_ setFrame:howToManageFrame];

  NSRect frame = howToManageFrame;
  if ([self showSyncPromo]) {
    frame = [promo_.get() frame];
    frame.origin.y = NSMinY(howToManageFrame) -
        (NSHeight(frame) + extension_installed_bubble::kInnerVerticalMargin);
    [promo_.get() setFrame:frame];
  }

  extensions::Command command;
  if (![manageShortcutLink_ isHidden]) {
    NSRect manageShortcutFrame = [manageShortcutLink_ frame];
    manageShortcutFrame.origin.y = NSMinY(frame) - (
        NSHeight(manageShortcutFrame) +
        extension_installed_bubble::kInnerVerticalMargin);
    // Right-align the link.
    manageShortcutFrame.origin.x = NSMaxX(frame) -
                                   NSWidth(manageShortcutFrame);
    [manageShortcutLink_ setFrame:manageShortcutFrame];
  }
}

// Exposed for unit testing.
- (NSRect)headingFrame {
  return [heading_ frame];
}

- (NSRect)frameOfHowToUse {
  return [howToUse_ frame];
}

- (NSRect)frameOfHowToManage {
  return [howToManage_ frame];
}

- (NSRect)frameOfSigninPromo {
  return [promo_ frame];
}

- (NSButton*)appInstalledShortcutLink {
  return appShortcutLink_;
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

- (IBAction)onAppShortcutClicked:(id)sender {
  ExtensionInstallUI::OpenAppInstalledUI(browser_->profile(), extension_->id());
}

- (void)awakeFromNib {
  if (bundle_)
    return;
  [self initializeLabel];
}

@end
