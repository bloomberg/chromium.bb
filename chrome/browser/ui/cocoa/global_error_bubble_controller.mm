// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/global_error_bubble_controller.h"

#include "base/logging.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/search_engines/util.h"
#import "chrome/browser/ui/browser.h"
#import "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/l10n_util.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#import "chrome/browser/ui/global_error.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

namespace {

// The vertical offset of the wrench bubble from the wrench menu button.
const CGFloat kWrenchBubblePointOffsetY = 6;

} // namespace

@implementation GlobalErrorBubbleController

+ (void)showForBrowser:(Browser*)browser
                 error:(GlobalError*)error {
  NSWindow* parentWindow = browser->window()->GetNativeHandle();
  BrowserWindowController* bwc = [BrowserWindowController
      browserWindowControllerForWindow:parentWindow];
  NSView* wrenchButton = [[bwc toolbarController] wrenchButton];
  NSPoint offset = NSMakePoint(NSMidX([wrenchButton bounds]),
                               kWrenchBubblePointOffsetY);

  // The bubble will be automatically deleted when the window is closed.
  GlobalErrorBubbleController* bubble = [[GlobalErrorBubbleController alloc]
      initWithWindowNibPath:@"GlobalErrorBubble"
             relativeToView:wrenchButton
                     offset:offset];
  bubble->error_ = error;
  [bubble showWindow:nil];
}

- (void)awakeFromNib {
  [super awakeFromNib];

  DCHECK(error_);

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  [iconView_ setImage:rb.GetNativeImageNamed(
      error_->GetBubbleViewIconResourceID()).ToNSImage()];

  [title_ setStringValue:SysUTF16ToNSString(error_->GetBubbleViewTitle())];
  [message_ setStringValue:SysUTF16ToNSString(error_->GetBubbleViewMessage())];
  [acceptButton_ setTitle:
      SysUTF16ToNSString(error_->GetBubbleViewAcceptButtonLabel())];
  string16 cancelLabel = error_->GetBubbleViewCancelButtonLabel();
  if (cancelLabel.empty())
    [cancelButton_ setHidden:YES];
  else
    [cancelButton_ setTitle:SysUTF16ToNSString(cancelLabel)];

  // Adapt window size to bottom buttons. Do this before all other layouting.
  NSArray* views = [NSArray arrayWithObjects:
      title_, message_, [acceptButton_ superview], nil];
  NSSize ds = NSMakeSize(0, cocoa_l10n_util::VerticallyReflowGroup(views));
  ds = [[self bubble] convertSize:ds toView:nil];

  NSRect frame = [[self window] frame];
  frame.origin.y -= ds.height;
  frame.size.height += ds.height;
  [[self window] setFrame:frame display:YES];
}

- (void)showWindow:(id)sender {
  BrowserWindowController* bwc = [BrowserWindowController
      browserWindowControllerForWindow:[self parentWindow]];
  [bwc lockBarVisibilityForOwner:self withAnimation:NO delay:NO];
  [super showWindow:sender];
}

- (void)close {
  error_->BubbleViewDidClose();
  BrowserWindowController* bwc = [BrowserWindowController
      browserWindowControllerForWindow:[self parentWindow]];
  [bwc releaseBarVisibilityForOwner:self withAnimation:YES delay:NO];
  [super close];
}

- (IBAction)onAccept:(id)sender {
  error_->BubbleViewAcceptButtonPressed();
  [self close];
}

- (IBAction)onCancel:(id)sender {
  error_->BubbleViewCancelButtonPressed();
  [self close];
}

@end

void GlobalError::ShowBubbleView(Browser* browser, GlobalError* error) {
  [GlobalErrorBubbleController showForBrowser:browser error:error];
}
