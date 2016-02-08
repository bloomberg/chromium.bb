// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/autosignin_prompt_view_controller.h"

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/string16.h"
#import "chrome/browser/ui/cocoa/passwords/passwords_bubble_utils.h"
#include "chrome/browser/ui/passwords/password_dialog_controller.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/cocoa/controls/hyperlink_text_view.h"
#include "ui/base/l10n/l10n_util.h"

@interface AutoSigninPromptViewController()
- (void)onOkClicked:(id)sender;
- (void)onTurnOffClicked:(id)sender;
@end

@implementation AutoSigninPromptViewController
@synthesize bridge = _bridge;
NSButton* _okButton;
NSButton* _turnOffButton;
HyperlinkTextView* _contentText;

- (instancetype)initWithBridge:(PasswordPromptBridgeInterface*)bridge {
  DCHECK(bridge);
  if (self = [super initWithNibName:nil bundle:nil]) {
    _bridge = bridge;
  }
  return self;
}

// ------------------------------------
// | Title                            |
// |                                  |
// | Auto Signin is cool.             |
// |                                  |
// |              [ Turn Off ] [ OK ] |
// ------------------------------------
- (void)loadView {
  base::scoped_nsobject<NSView> view([[NSView alloc] initWithFrame:NSZeroRect]);

  // Title.
  base::string16 titleText =
      self.bridge->GetDialogController()->GetAutoSigninPromoTitle();
  NSTextView* titleView = TitleLabelWithLink(titleText, gfx::Range(), self);
  // The text container by default track the view's width only. Set the width to
  // a big number so the container is resized too.
  [titleView setFrameSize:NSMakeSize(MAXFLOAT, 0)];
  // Now force the view to track the text's size and resize it.
  [titleView setHorizontallyResizable:YES];
  [titleView setVerticallyResizable:YES];
  [titleView sizeToFit];

  // Content.
  std::pair<base::string16, gfx::Range> contentText =
      self.bridge->GetDialogController()->GetAutoSigninText();
  _contentText = LabelWithLink(contentText.first, kAutoSigninTextColor,
                               contentText.second, self);
  [_contentText setVerticallyResizable:YES];

  // Buttons.
  _okButton =
      DialogButton(l10n_util::GetNSString(IDS_AUTO_SIGNIN_FIRST_RUN_OK));
  [_okButton setTarget:self];
  [_okButton setAction:@selector(onOkClicked:)];
  [_okButton setKeyEquivalent:@"\r"];
  [view addSubview:_okButton];

  _turnOffButton =
      DialogButton(l10n_util::GetNSString(IDS_AUTO_SIGNIN_FIRST_RUN_TURN_OFF));
  [_turnOffButton setTarget:self];
  [_turnOffButton setAction:@selector(onTurnOffClicked:)];
  [view addSubview:_turnOffButton];

  // Layout.
  // Compute the bubble width using the title and the buttons.
  const CGFloat contentWidth = 2 * kFramePadding + std::max(
      NSWidth([titleView frame]),
      NSWidth([_okButton frame]) + kRelatedControlHorizontalPadding +
      NSWidth([_turnOffButton frame]));
  CGFloat curX = contentWidth - kFramePadding;
  CGFloat curY = kFramePadding;
  [_okButton setFrameOrigin:NSMakePoint(curX - NSWidth([_okButton frame]),
                                        curY)];
  curX -= (NSWidth([_okButton frame]) + kRelatedControlHorizontalPadding);
  [_turnOffButton setFrameOrigin:NSMakePoint(
      curX - NSWidth([_turnOffButton frame]), curY)];
  curY = 3 * kRelatedControlVerticalSpacing + NSMaxY([_turnOffButton frame]);
  curX = kFramePadding;

  [_contentText setFrameSize:NSMakeSize(contentWidth - 2 * kFramePadding,
                                        MAXFLOAT)];
  [_contentText sizeToFit];
  [view addSubview:_contentText];
  [_contentText setFrameOrigin:NSMakePoint(curX, curY)];
  curY = NSMaxY([_contentText frame]) + 2 * kRelatedControlVerticalSpacing;

  [view addSubview:titleView];
  [titleView setFrameOrigin:NSMakePoint(curX, curY)];

  const CGFloat frameHeight = NSMaxY([titleView frame]) + kFramePadding;
  [view setFrame:NSMakeRect(0, 0, contentWidth, frameHeight)];
  [self setView:view];
}

- (BOOL)textView:(NSTextView*)textView
   clickedOnLink:(id)link
         atIndex:(NSUInteger)charIndex {
  if (_bridge && _bridge->GetDialogController())
    _bridge->GetDialogController()->OnSmartLockLinkClicked();
  return YES;
}

- (void)onOkClicked:(id)sender {
  if (_bridge && _bridge->GetDialogController())
    _bridge->GetDialogController()->OnAutoSigninOK();
}

- (void)onTurnOffClicked:(id)sender {
  if (_bridge && _bridge->GetDialogController())
    _bridge->GetDialogController()->OnAutoSigninTurnOff();
}

@end

@implementation AutoSigninPromptViewController(Testing)
- (NSTextView*)contentText {
  return _contentText;
}

- (NSButton*)okButton {
  return _okButton;
}

- (NSButton*)turnOffButton {
  return _turnOffButton;
}
@end
