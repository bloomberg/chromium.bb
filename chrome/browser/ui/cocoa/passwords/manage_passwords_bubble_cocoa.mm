// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_cocoa.h"

#include "base/mac/scoped_block.h"
#include "chrome/browser/ui/browser_finder.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/browser/ui/cocoa/location_bar/manage_passwords_decoration.h"
#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon.h"
#include "content/public/browser/web_contents.h"

typedef void (^Callback)(void);

@interface ManagePasswordsBubbleCocoaNotificationBridge : NSObject {
  base::mac::ScopedBlock<Callback> callback_;
}
- (id)initWithCallback:(Callback)callback;
- (void)onClose;
@end

@implementation ManagePasswordsBubbleCocoaNotificationBridge
- (id)initWithCallback:(Callback)callback {
  if ((self = [super init])) {
    callback_.reset(callback, base::scoped_policy::RETAIN);
  }
  return self;
}
- (void)onClose {
  callback_.get()();
}
@end

// static
ManagePasswordsBubbleCocoa* ManagePasswordsBubbleCocoa::bubble_ = NULL;

ManagePasswordsBubbleCocoa::ManagePasswordsBubbleCocoa(
    content::WebContents* webContents,
    DisplayReason displayReason,
    ManagePasswordsIcon* icon)
    : ManagePasswordsBubble(webContents, displayReason),
      icon_(icon),
      closing_(false),
      controller_(nil),
      webContents_(webContents),
      bridge_(nil) {
  DCHECK(icon_);
  icon_->SetActive(true);
}

ManagePasswordsBubbleCocoa::~ManagePasswordsBubbleCocoa() {
  [[NSNotificationCenter defaultCenter] removeObserver:bridge_];
  // Clear the global bubble_ pointer.
  bubble_ = NULL;
  if (icon_)
    icon_->SetActive(false);
}

void ManagePasswordsBubbleCocoa::Show() {
  // Create and show the bubble.
  NSView* browserView = webContents_->GetNativeView();
  DCHECK(browserView);
  NSWindow* browserWindow = [browserView window];
  DCHECK(browserWindow);
  controller_ = [[ManagePasswordsBubbleController alloc]
      initWithParentWindow:browserWindow
                     model:model()];
  [controller_ showWindow:nil];

  // Listen for close notification to perform cleanup: the window isn't
  // necessarily closed by calling Close(). Use a block instead of directly
  // calling OnClose from the bridge so that we don't need to expose OnClose
  // in the public API of ManagePasswordsBubbleCocoa.
  bridge_.reset([[ManagePasswordsBubbleCocoaNotificationBridge alloc]
      initWithCallback:^{
          OnClose();
      }]);
  [[NSNotificationCenter defaultCenter]
      addObserver:bridge_
         selector:@selector(onClose)
             name:NSWindowWillCloseNotification
           object:[controller_ window]];
}

void ManagePasswordsBubbleCocoa::Close() {
  if (!closing_) {
    closing_ = true;
    [controller_ close];
  }
}

void ManagePasswordsBubbleCocoa::OnClose() {
  delete this;
}

// static
void ManagePasswordsBubbleCocoa::Show(content::WebContents* webContents,
                                      bool user_action) {
  if (bubble_ && (bubble_->webContents_ != webContents)) {
    // The bubble is currently shown for some other tab. We should close it now
    // and open for |webContents|.
    bubble_->Close();
  }
  if (bubble_)
    return;

  NSWindow* window = [webContents->GetNativeView() window];
  if (!window) {
    // The tab isn't active right now.
    return;
  }
  BrowserWindowController* bwc =
      [BrowserWindowController browserWindowControllerForWindow:window];
  bubble_ = new ManagePasswordsBubbleCocoa(
      webContents,
      user_action ? ManagePasswordsBubble::USER_ACTION
                  : ManagePasswordsBubble::AUTOMATIC,
      [bwc locationBarBridge]->manage_passwords_decoration()->icon());

  bubble_->Show();
}
