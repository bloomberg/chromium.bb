// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/share_menu_controller.h"

#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/accelerators_cocoa.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/fast_resize_view.h"
#import "chrome/browser/ui/cocoa/last_active_browser_cocoa.h"
#include "chrome/grit/generated_resources.h"
#include "net/base/mac/url_conversions.h"
#include "ui/base/accelerators/platform_accelerator_cocoa.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image.h"
#include "ui/snapshot/snapshot.h"

namespace {

NSString* const kExtensionPrefPanePath =
    @"/System/Library/PreferencePanes/Extensions.prefPane";
// Undocumented, used by Safari.
const UInt32 kOpenSharingSubpaneDescriptorType = 'ptru';
NSString* const kOpenSharingSubpaneActionKey = @"action";
NSString* const kOpenSharingSubpaneActionValue = @"revealExtensionPoint";
NSString* const kOpenSharingSubpaneProtocolKey = @"protocol";
NSString* const kOpenSharingSubpaneProtocolValue = @"com.apple.share-services";

}  // namespace

@implementation ShareMenuController {
  // The following three ivars are provided to the system via NSSharingService
  // delegates. They're needed for the transition animation, and to provide a
  // screenshot of the shared site for services that support it.
  NSWindow* windowForShare_;  // weak
  NSRect rectForShare_;
  base::scoped_nsobject<NSImage> snapshotForShare_;
}

+ (BOOL)shouldShowMoreItem {
  return base::mac::IsAtLeastOS10_10();
}

// NSMenuDelegate
- (void)menuNeedsUpdate:(NSMenu*)menu {
  [menu removeAllItems];
  [menu setAutoenablesItems:NO];

  Browser* lastActiveBrowser = chrome::GetLastActiveBrowser();
  BOOL canShare =
      lastActiveBrowser != nullptr &&
      // Avoid |CanEmailPageLocation| segfault in interactive UI tests
      lastActiveBrowser->tab_strip_model()->GetActiveWebContents() != nullptr &&
      chrome::CanEmailPageLocation(chrome::GetLastActiveBrowser());
  // Using a real URL instead of empty string to avoid system log about relative
  // URLs in the pasteboard. This URL will not actually be shared to, just used
  // to fetch sharing services that can handle the NSURL type.
  NSArray* services = [NSSharingService
      sharingServicesForItems:@[ [NSURL URLWithString:@"https://google.com"] ]];
  NSSharingService* readingListService = [NSSharingService
      sharingServiceNamed:NSSharingServiceNameAddToSafariReadingList];
  for (NSSharingService* service in services) {
    // Don't include "Add to Reading List".
    if ([service isEqual:readingListService])
      continue;
    NSMenuItem* item = [self menuItemForService:service];
    [item setEnabled:canShare];
    [menu addItem:item];
  }
  if (![[self class] shouldShowMoreItem]) {
    return;
  }
  base::scoped_nsobject<NSMenuItem> moreItem([[NSMenuItem alloc]
      initWithTitle:l10n_util::GetNSString(IDS_SHARING_MORE_MAC)
             action:@selector(openSharingPrefs:)
      keyEquivalent:@""]);
  [moreItem setTarget:self];
  [moreItem setImage:[self moreImage]];
  [menu addItem:moreItem];
}

// NSSharingServiceDelegate

- (void)sharingService:(NSSharingService*)service
         didShareItems:(NSArray*)items {
  // TODO(lgrey): Add an UMA stat.
  [self clearTransitionData];
}

- (void)sharingService:(NSSharingService*)service
    didFailToShareItems:(NSArray*)items
                  error:(NSError*)error {
  // TODO(lgrey): Add an UMA stat.
  [self clearTransitionData];
}

- (NSRect)sharingService:(NSSharingService*)service
    sourceFrameOnScreenForShareItem:(id)item {
  return rectForShare_;
}

- (NSWindow*)sharingService:(NSSharingService*)service
    sourceWindowForShareItems:(NSArray*)items
          sharingContentScope:(NSSharingContentScope*)scope {
  *scope = NSSharingContentScopeFull;
  return windowForShare_;
}

- (NSImage*)sharingService:(NSSharingService*)service
    transitionImageForShareItem:(id)item
                    contentRect:(NSRect*)contentRect {
  return snapshotForShare_;
}

// Private methods

// Saves details required by delegate methods for the transition animation.
- (void)saveTransitionDataFromBrowser:(Browser*)browser {
  windowForShare_ = browser->window()->GetNativeWindow();

  NSView* contentsView = [[windowForShare_ windowController] tabContentArea];
  NSRect rectInWindow =
      [[contentsView superview] convertRect:[contentsView frame] toView:nil];
  rectForShare_ = [windowForShare_ convertRectToScreen:rectInWindow];

  gfx::Image image;
  gfx::Rect rect = gfx::Rect(NSRectToCGRect([contentsView bounds]));
  if (ui::GrabViewSnapshot(contentsView, rect, &image)) {
    snapshotForShare_.reset(image.CopyNSImage());
  }
}

- (void)clearTransitionData {
  windowForShare_ = nil;
  rectForShare_ = NSZeroRect;
  snapshotForShare_.reset();
}

// Performs the share action using the sharing service represented by |sender|.
- (void)performShare:(NSMenuItem*)sender {
  Browser* browser = chrome::GetLastActiveBrowser();
  DCHECK(browser);
  [self saveTransitionDataFromBrowser:browser];

  content::WebContents* contents =
      browser->tab_strip_model()->GetActiveWebContents();
  NSURL* url = net::NSURLWithGURL(contents->GetLastCommittedURL());
  NSString* title = base::SysUTF16ToNSString(contents->GetTitle());

  NSSharingService* service =
      base::mac::ObjCCastStrict<NSSharingService>([sender representedObject]);
  [service setDelegate:self];
  [service setSubject:title];

  NSArray* itemsToShare;
  if ([service
          isEqual:[NSSharingService
                      sharingServiceNamed:NSSharingServiceNamePostOnTwitter]]) {
    // The Twitter share service expects the title as an additional share item.
    // This is the same approach system apps use.
    itemsToShare = @[ url, title ];
  } else {
    itemsToShare = @[ url ];
  }
  [service performWithItems:itemsToShare];
}

// Opens the "Sharing" subpane of the "Extensions" macOS preference pane.
- (void)openSharingPrefs:(NSMenuItem*)sender {
  DCHECK([[self class] shouldShowMoreItem]);
  NSURL* prefPaneURL =
      [NSURL fileURLWithPath:kExtensionPrefPanePath isDirectory:YES];
  NSDictionary* args = @{
    kOpenSharingSubpaneActionKey : kOpenSharingSubpaneActionValue,
    kOpenSharingSubpaneProtocolKey : kOpenSharingSubpaneProtocolValue
  };
  NSData* data = [NSPropertyListSerialization
      dataWithPropertyList:args
                    format:NSPropertyListXMLFormat_v1_0
                   options:0
                     error:nil];
  base::scoped_nsobject<NSAppleEventDescriptor> descriptor(
      [[NSAppleEventDescriptor alloc]
          initWithDescriptorType:kOpenSharingSubpaneDescriptorType
                            data:data]);
  [[NSWorkspace sharedWorkspace] openURLs:@[ prefPaneURL ]
                  withAppBundleIdentifier:nil
                                  options:NSWorkspaceLaunchAsync
           additionalEventParamDescriptor:descriptor
                        launchIdentifiers:NULL];
}

// Returns the image to be used for the "More..." menu item, or nil on macOS
// version where this private method is unsupported.
- (NSImage*)moreImage {
  if ([NSSharingServicePicker
          respondsToSelector:@selector(sharedMoreMenuImage)]) {
    return
        [NSSharingServicePicker performSelector:@selector(sharedMoreMenuImage)];
  }
  return nil;
}

// Creates a menu item that calls |service| when invoked.
- (NSMenuItem*)menuItemForService:(NSSharingService*)service {
  BOOL isMail = [service
      isEqual:[NSSharingService
                  sharingServiceNamed:NSSharingServiceNameComposeEmail]];
  NSString* keyEquivalent = isMail ? [self keyEquivalentForMail] : @"";
  NSString* title = isMail ? l10n_util::GetNSString(IDS_EMAIL_PAGE_LOCATION_MAC)
                           : service.menuItemTitle;
  base::scoped_nsobject<NSMenuItem> item([[NSMenuItem alloc]
      initWithTitle:title
             action:@selector(performShare:)
      keyEquivalent:keyEquivalent]);
  [item setTarget:self];
  [item setImage:[service image]];
  [item setRepresentedObject:service];
  return item.autorelease();
}

- (NSString*)keyEquivalentForMail {
  AcceleratorsCocoa* keymap = AcceleratorsCocoa::GetInstance();
  const ui::Accelerator* accelerator =
      keymap->GetAcceleratorForCommand(IDC_EMAIL_PAGE_LOCATION);
  const ui::PlatformAcceleratorCocoa* platform =
      static_cast<const ui::PlatformAcceleratorCocoa*>(
          accelerator->platform_accelerator());
  return platform->characters();
}

@end
