// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/autosignin_prompt_view_controller.h"

#include <Carbon/Carbon.h>

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/string16.h"
#import "chrome/browser/ui/cocoa/key_equivalent_constants.h"
#import "chrome/browser/ui/cocoa/passwords/passwords_bubble_utils.h"
#include "chrome/browser/ui/passwords/password_dialog_controller.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/cocoa/controls/hyperlink_text_view.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Returns a NSRegularControlSize button. It's used for improving the contrast
// due to Accessabilty standards.
NSButton* BiggerDialogButton(NSString* title) {
  base::scoped_nsobject<NSButton> button(
      [[NSButton alloc] initWithFrame:NSZeroRect]);
  CGFloat fontSize = [NSFont systemFontSizeForControlSize:NSRegularControlSize];
  [button setFont:[NSFont systemFontOfSize:fontSize]];
  [button setTitle:title];
  [button setBezelStyle:NSRoundedBezelStyle];
  [[button cell] setControlSize:NSRegularControlSize];
  [button sizeToFit];
  return button.autorelease();
}

}  // namespace

@interface AutoSigninPromptView : NSView
@property (nonatomic, copy) BOOL (^escHandler)(NSEvent* theEvent);
@end

@implementation AutoSigninPromptView
@synthesize escHandler = _escHandler;

-(void)dealloc {
  [_escHandler release];
  [super dealloc];
}

- (BOOL)performKeyEquivalent:(NSEvent*)theEvent {
  if (_escHandler(theEvent))
    return YES;
  return [super performKeyEquivalent:theEvent];
}

@end

@interface AutoSigninPromptViewController () {
  NSButton* _okButton;
  NSButton* _turnOffButton;
  HyperlinkTextView* _contentText;
}
- (void)onOkClicked:(id)sender;
- (void)onTurnOffClicked:(id)sender;
- (BOOL)handleEscPress:(NSEvent*)theEvent;
@end

@implementation AutoSigninPromptViewController
@synthesize bridge = _bridge;

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
  base::scoped_nsobject<AutoSigninPromptView> view(
      [[AutoSigninPromptView alloc] initWithFrame:NSZeroRect]);
  __block AutoSigninPromptViewController* weakSelf = self;
  [view setEscHandler:^(NSEvent* theEvent) {
      return [weakSelf handleEscPress:theEvent];
  }];


  // Title.
  base::string16 titleText =
      self.bridge->GetDialogController()->GetAutoSigninPromoTitle();
  NSTextView* titleView =
      TitleDialogLabelWithLink(titleText, gfx::Range(), self);
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
      BiggerDialogButton(l10n_util::GetNSString(IDS_AUTO_SIGNIN_FIRST_RUN_OK));
  [_okButton setTarget:self];
  [_okButton setAction:@selector(onOkClicked:)];
  [_okButton setKeyEquivalent:kKeyEquivalentReturn];
  [view addSubview:_okButton];

  _turnOffButton = BiggerDialogButton(
       l10n_util::GetNSString(IDS_AUTO_SIGNIN_FIRST_RUN_TURN_OFF));
  [_turnOffButton setTarget:self];
  [_turnOffButton setAction:@selector(onTurnOffClicked:)];
  [view addSubview:_turnOffButton];

  // Invisible button to handle ESC.
  base::scoped_nsobject<NSButton> cancel_button(
      [[NSButton alloc] initWithFrame:NSZeroRect]);
  [cancel_button setTarget:self];
  [cancel_button setAction:@selector(onEscClicked:)];
  [cancel_button setKeyEquivalent:kKeyEquivalentEscape];
  [view addSubview:cancel_button];

  // Layout.
  // Compute the bubble width using the title and the buttons.
  const CGFloat contentWidth = kDesiredBubbleWidth;
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

- (BOOL)handleEscPress:(NSEvent*)theEvent {
  if ([theEvent keyCode] == kVK_Escape) {
    if (_bridge)
      _bridge->PerformClose();
    return YES;
  }
  return NO;
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
