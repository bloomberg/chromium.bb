// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/passwords_bubble_cocoa.h"

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_block.h"
#include "chrome/browser/ui/browser_finder.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "chrome/browser/ui/cocoa/location_bar/manage_passwords_decoration.h"
#import "chrome/browser/ui/cocoa/passwords/passwords_bubble_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
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
    ManagePasswordsBubbleModel::DisplayReason displayReason,
    ManagePasswordsIcon* icon)
    : model_(PasswordsModelDelegateFromWebContents(webContents),
             displayReason),
      icon_(icon),
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

void ManagePasswordsBubbleCocoa::Show(bool user_action) {
  // Create and show the bubble.
  NSView* browserView = webContents_->GetNativeView();
  DCHECK(browserView);
  NSWindow* browserWindow = [browserView window];
  DCHECK(browserWindow);
  controller_ = [[ManagePasswordsBubbleController alloc]
      initWithParentWindow:browserWindow
                     model:&model_];
  [controller_ setShouldOpenAsKeyWindow:(user_action ? YES : NO)];
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

void ManagePasswordsBubbleCocoa::Close(bool no_animation) {
  if (no_animation) {
    InfoBubbleWindow* window = base::mac::ObjCCastStrict<InfoBubbleWindow>(
        [controller_ window]);
    [window setAllowedAnimations:info_bubble::kAnimateNone];
  }
  [controller_ close];
}

void ManagePasswordsBubbleCocoa::Close() {
  Close(false);
}

void ManagePasswordsBubbleCocoa::OnClose() {
  delete this;
}

// static
void ManagePasswordsBubbleCocoa::Show(content::WebContents* webContents,
                                      bool user_action) {
  if (bubble_) {
    // The bubble is currently shown. It's to be reopened with the new content.
    // Disable closing animation so that it's destroyed immediately.
    bubble_->Close(true);
  }
  DCHECK(!bubble_);

  NSWindow* window = [webContents->GetNativeView() window];
  if (!window) {
    // The tab isn't active right now.
    return;
  }
  BrowserWindowController* bwc =
      [BrowserWindowController browserWindowControllerForWindow:window];
  bubble_ = new ManagePasswordsBubbleCocoa(
      webContents, user_action ? ManagePasswordsBubbleModel::USER_ACTION
                               : ManagePasswordsBubbleModel::AUTOMATIC,
      [bwc locationBarBridge]->manage_passwords_decoration()->icon());

  bubble_->Show(user_action);
}
