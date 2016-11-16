// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/signin_promo_view_controller.h"

#include "base/mac/scoped_nsobject.h"
#include "chrome/browser/ui/cocoa/chrome_style.h"
#import "chrome/browser/ui/cocoa/hover_close_button.h"
#include "chrome/browser/ui/cocoa/passwords/passwords_bubble_utils.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/user_metrics.h"
#import "ui/base/cocoa/controls/hyperlink_text_view.h"
#include "ui/base/l10n/l10n_util_mac.h"

@interface SignInPromoViewController () {
  base::scoped_nsobject<NSButton> _signInButton;
  base::scoped_nsobject<NSButton> _noButton;
  base::scoped_nsobject<NSButton> _closeButton;
}

// "Sign In" and "No thanks" button handlers.
- (void)onSignInClicked:(id)sender;
- (void)onNoClicked:(id)sender;
- (void)onCloseClicked:(id)sender;
@end

@implementation SignInPromoViewController

- (instancetype)initWithDelegate:
    (id<BasePasswordsContentViewDelegate>)delegate {
  return [super initWithDelegate:delegate];
}

- (NSButton*)defaultButton {
  return _signInButton;
}

// ------------------------------------
// | Sign in to Chrome!               |
// |                                  |
// |               [ No ] [ Sign In ] |
// ------------------------------------
- (void)loadView {
  base::scoped_nsobject<NSView> view([[NSView alloc] initWithFrame:NSZeroRect]);
  // Close button.
  const int dimension = chrome_style::GetCloseButtonSize();
  NSRect frame = NSMakeRect(0, 0, dimension, dimension);
  _closeButton.reset(
      [[WebUIHoverCloseButton alloc] initWithFrame:frame]);
  [_closeButton setAction:@selector(onCloseClicked:)];
  [_closeButton setTarget:self];
  [view addSubview:_closeButton];
  // Title.
  HyperlinkTextView* titleView = TitleBubbleLabelWithLink(
      [self.delegate model]->title(), gfx::Range(), nil);
  // Force the text to wrap to fit in the bubble size.
  int titleRightPadding =
      2 * chrome_style::kCloseButtonPadding + NSWidth([_closeButton frame]);
  int titleWidth = kDesiredBubbleWidth - kFramePadding - titleRightPadding;
  [titleView setVerticallyResizable:YES];
  [titleView setFrameSize:NSMakeSize(titleWidth, MAXFLOAT)];
  // Set the same text inset as in the pending password bubble.
  [[titleView textContainer] setLineFragmentPadding:kTitleTextInset];
  [titleView sizeToFit];
  [view addSubview:titleView];

  NSString* signInText =
      l10n_util::GetNSString(IDS_PASSWORD_MANAGER_SIGNIN_PROMO_SIGN_IN);
  _signInButton.reset([[self addButton:signInText
                                toView:view
                                target:self
                                action:@selector(onSignInClicked:)] retain]);
  NSString* noText =
      l10n_util::GetNSString(IDS_PASSWORD_MANAGER_SIGNIN_PROMO_NO_THANKS);
  _noButton.reset([[self addButton:noText
                            toView:view
                            target:self
                            action:@selector(onNoClicked:)] retain]);

  // Layout the elements, starting at the bottom and moving up.
  CGFloat curX =
      kDesiredBubbleWidth - kFramePadding - NSWidth([_signInButton frame]);
  CGFloat curY = kFramePadding;
  [_signInButton setFrameOrigin:NSMakePoint(curX, curY)];

  curX -= kRelatedControlHorizontalPadding;
  curX -= NSWidth([_noButton frame]);
  [_noButton setFrameOrigin:NSMakePoint(curX, curY)];

  curY = NSMaxY([_noButton frame]) + kUnrelatedControlVerticalPadding;
  [titleView setFrameOrigin:NSMakePoint(kFramePadding, curY)];
  const CGFloat height = NSMaxY([titleView frame]) + kFramePadding;
  // The close button is in the corner.
  NSPoint closeButtonOrigin = NSMakePoint(
      NSMaxX([titleView frame]) + chrome_style::kCloseButtonPadding,
      height - NSHeight([_closeButton frame]) -
          chrome_style::kCloseButtonPadding);
  [_closeButton setFrameOrigin:closeButtonOrigin];
  [view setFrame:NSMakeRect(0, 0, kDesiredBubbleWidth, height)];
  [self setView:view];
  content::RecordAction(
      base::UserMetricsAction("Signin_Impression_FromPasswordBubble"));
}

- (void)onSignInClicked:(id)sender {
  ManagePasswordsBubbleModel* model = [self.delegate model];
  if (model)
    model->OnSignInToChromeClicked();
  [self.delegate viewShouldDismiss];
}

- (void)onNoClicked:(id)sender {
  ManagePasswordsBubbleModel* model = [self.delegate model];
  if (model)
    model->OnSkipSignInClicked();
  [self.delegate viewShouldDismiss];
}

- (void)onCloseClicked:(id)sender {
  [self.delegate viewShouldDismiss];
}

@end

@implementation SignInPromoViewController (Testing)

- (NSButton*)signInButton {
  return _signInButton.get();
}

- (NSButton*)noButton {
  return _noButton.get();
}

- (NSButton*)closeButton {
  return _closeButton.get();
}

@end
