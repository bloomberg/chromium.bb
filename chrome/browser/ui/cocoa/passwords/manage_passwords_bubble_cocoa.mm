// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_cocoa.h"

#include "base/mac/scoped_block.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_finder.h"
#import "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
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
    DisplayReason displayReason)
    : ManagePasswordsBubble(webContents, displayReason),
      closing_(false),
      controller_(nil),
      webContents_(webContents),
      bridge_(nil) {
}

ManagePasswordsBubbleCocoa::~ManagePasswordsBubbleCocoa() {
  [[NSNotificationCenter defaultCenter] removeObserver:bridge_];
  // Clear the global instance pointer.
  bubble_ = NULL;
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
void ManagePasswordsBubbleCocoa::ShowBubble(content::WebContents* webContents,
                                            DisplayReason displayReason) {
  if (bubble_)
    return;
  bubble_ = new ManagePasswordsBubbleCocoa(webContents, displayReason);
  bubble_->Show();
}
