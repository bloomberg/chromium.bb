// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/global_error_bubble_controller.h"

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "chrome/browser/ui/browser.h"
#import "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/l10n_util.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#import "chrome/browser/ui/cocoa/wrench_menu/wrench_menu_controller.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_bubble_view_base.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "components/search_engines/util.h"
#include "grit/generated_resources.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/image/image.h"

namespace {
const CGFloat kParagraphSpacing = 6;
} // namespace

namespace GlobalErrorBubbleControllerInternal {

// This is the bridge to the C++ GlobalErrorBubbleViewBase object.
class Bridge : public GlobalErrorBubbleViewBase {
 public:
  Bridge(GlobalErrorBubbleController* controller) : controller_(controller) {
  }

 private:
  virtual void CloseBubbleView() OVERRIDE {
    [controller_ close];
  }

  GlobalErrorBubbleController* controller_;  // Weak, owns this.
};

}  // namespace GlobalErrorBubbleControllerInternal

@implementation GlobalErrorBubbleController

+ (GlobalErrorBubbleViewBase*)showForBrowser:(Browser*)browser
    error:(const base::WeakPtr<GlobalErrorWithStandardBubble>&)error {
  NSWindow* parentWindow = browser->window()->GetNativeWindow();
  BrowserWindowController* bwc = [BrowserWindowController
      browserWindowControllerForWindow:parentWindow];
  NSView* wrenchButton = [[bwc toolbarController] wrenchButton];
  NSPoint offset = NSMakePoint(
      NSMidX([wrenchButton bounds]),
      wrench_menu_controller::kWrenchBubblePointOffsetY);

  // The bubble will be automatically deleted when the window is closed.
  GlobalErrorBubbleController* bubble = [[GlobalErrorBubbleController alloc]
      initWithWindowNibPath:@"GlobalErrorBubble"
             relativeToView:wrenchButton
                     offset:offset];
  bubble->error_ = error;
  bubble->bridge_.reset(new GlobalErrorBubbleControllerInternal::Bridge(
      bubble));
  bubble->browser_ = browser;
  [bubble showWindow:nil];

  return bubble->bridge_.get();
}

- (void)awakeFromNib {
  [super awakeFromNib];

  DCHECK(error_);

  gfx::Image image = error_->GetBubbleViewIcon();
  DCHECK(!image.IsEmpty());
  [iconView_ setImage:image.ToNSImage()];

  [title_ setStringValue:SysUTF16ToNSString(error_->GetBubbleViewTitle())];
  std::vector<base::string16> messages = error_->GetBubbleViewMessages();
  base::string16 message = JoinString(messages, '\n');

  base::scoped_nsobject<NSMutableAttributedString> messageValue(
      [[NSMutableAttributedString alloc]
          initWithString:SysUTF16ToNSString(message)]);
  base::scoped_nsobject<NSMutableParagraphStyle> style(
      [[NSMutableParagraphStyle alloc] init]);
  [style setParagraphSpacing:kParagraphSpacing];
  [messageValue addAttribute:NSParagraphStyleAttributeName
                       value:style
                       range:NSMakeRange(0, [messageValue length])];

  [message_ setAttributedStringValue:messageValue];

  [acceptButton_ setTitle:
      SysUTF16ToNSString(error_->GetBubbleViewAcceptButtonLabel())];
  base::string16 cancelLabel = error_->GetBubbleViewCancelButtonLabel();
  if (cancelLabel.empty())
    [cancelButton_ setHidden:YES];
  else
    [cancelButton_ setTitle:SysUTF16ToNSString(cancelLabel)];

  // First make sure that the window is wide enough to accommodate the buttons.
  NSRect frame = [[self window] frame];
  [layoutTweaker_ tweakUI:buttonContainer_];
  CGFloat delta =  NSWidth([buttonContainer_ frame]) - NSWidth(frame);
  if (delta > 0) {
    frame.size.width += delta;
    [[self window] setFrame:frame display:NO];
  }

  // Adapt window height to bottom buttons. Do this before all other layouting.
  NSArray* views = [NSArray arrayWithObjects:
      title_, message_, buttonContainer_, nil];
  NSSize ds = NSMakeSize(0, cocoa_l10n_util::VerticallyReflowGroup(views));
  ds = [[self bubble] convertSize:ds toView:nil];

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
  if (error_)
    error_->BubbleViewDidClose(browser_);
  bridge_.reset();
  BrowserWindowController* bwc = [BrowserWindowController
      browserWindowControllerForWindow:[self parentWindow]];
  [bwc releaseBarVisibilityForOwner:self withAnimation:YES delay:NO];
  [super close];
}

- (IBAction)onAccept:(id)sender {
  if (error_)
    error_->BubbleViewAcceptButtonPressed(browser_);
  [self close];
}

- (IBAction)onCancel:(id)sender {
  if (error_)
    error_->BubbleViewCancelButtonPressed(browser_);
  [self close];
}

@end

GlobalErrorBubbleViewBase* GlobalErrorBubbleViewBase::ShowStandardBubbleView(
    Browser* browser,
    const base::WeakPtr<GlobalErrorWithStandardBubble>& error) {
  return [GlobalErrorBubbleController showForBrowser:browser error:error];
}
