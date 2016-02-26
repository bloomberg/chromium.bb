// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/save_card_bubble_view_bridge.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/save_card_bubble_controller.h"
#include "chrome/browser/ui/chrome_style.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#include "grit/components_strings.h"
#include "skia/ext/skia_utils_mac.h"
#import "ui/base/cocoa/controls/hyperlink_text_view.h"
#import "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

const CGFloat kDesiredBubbleWidth = 370;
const CGFloat kFramePadding = 16;
const CGFloat kRelatedControlHorizontalPadding = 2;
const CGFloat kUnrelatedControlVerticalPadding = 15;

const SkColor kIconBorderColor = 0x10000000;  // SkColorSetARGB(10, 0, 0, 0);
}

namespace autofill {

#pragma mark SaveCardBubbleViewBridge

SaveCardBubbleViewBridge::SaveCardBubbleViewBridge(
    SaveCardBubbleController* controller,
    BrowserWindowController* browser_window_controller)
    : controller_(controller) {
  view_controller_ = [[SaveCardBubbleViewCocoa alloc]
      initWithBrowserWindowController:browser_window_controller
                               bridge:this];
  DCHECK(view_controller_);
  [view_controller_ showWindow:nil];
}

SaveCardBubbleViewBridge::~SaveCardBubbleViewBridge() {}

base::string16 SaveCardBubbleViewBridge::GetWindowTitle() const {
  return controller_ ? controller_->GetWindowTitle() : base::string16();
}

CreditCard SaveCardBubbleViewBridge::GetCard() const {
  return controller_ ? controller_->GetCard() : CreditCard();
}

void SaveCardBubbleViewBridge::OnSaveButton() {
  if (controller_)
    controller_->OnSaveButton();
  Hide();
}

void SaveCardBubbleViewBridge::OnCancelButton() {
  if (controller_)
    controller_->OnCancelButton();
  Hide();
}

void SaveCardBubbleViewBridge::OnLearnMoreClicked() {
  if (controller_)
    controller_->OnLearnMoreClicked();
}

void SaveCardBubbleViewBridge::OnBubbleClosed() {
  if (controller_)
    controller_->OnBubbleClosed();

  delete this;
}

void SaveCardBubbleViewBridge::Hide() {
  // SaveCardBubbleViewBridge::OnBubbleClosed won't be able to call
  // OnBubbleClosed on the bubble controller since we null the reference to it
  // below. So we need to call it here.
  controller_->OnBubbleClosed();
  controller_ = nullptr;
  [view_controller_ close];
}

}  // autofill

#pragma mark SaveCardBubbleViewCocoa

@interface SaveCardBubbleViewCocoa ()
+ (base::scoped_nsobject<NSTextField>)makeTextField;
+ (base::scoped_nsobject<NSButton>)makeButton;
@end

@implementation SaveCardBubbleViewCocoa {
  autofill::SaveCardBubbleViewBridge* bridge_;  // Weak.
}

+ (base::scoped_nsobject<NSTextField>)makeTextField {
  base::scoped_nsobject<NSTextField> textField(
      [[NSTextField alloc] initWithFrame:NSZeroRect]);
  [textField setEditable:NO];
  [textField setSelectable:NO];
  [textField setDrawsBackground:NO];
  [textField setBezeled:NO];

  return textField;
}

+ (base::scoped_nsobject<NSButton>)makeButton {
  base::scoped_nsobject<NSButton> button(
      [[NSButton alloc] initWithFrame:NSZeroRect]);
  [button setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
  [button setBezelStyle:NSRoundedBezelStyle];
  [[button cell] setControlSize:NSSmallControlSize];

  return button;
}

- (id)initWithBrowserWindowController:
          (BrowserWindowController*)browserWindowController
                               bridge:
                                   (autofill::SaveCardBubbleViewBridge*)bridge {
  DCHECK(bridge);

  base::scoped_nsobject<InfoBubbleWindow> window([[InfoBubbleWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:NSBorderlessWindowMask
                  backing:NSBackingStoreBuffered
                    defer:NO]);

  NSPoint anchorPoint =
      [[browserWindowController toolbarController] saveCreditCardBubblePoint];
  anchorPoint =
      [[browserWindowController window] convertBaseToScreen:anchorPoint];

  if ((self = [super initWithWindow:window
                       parentWindow:[browserWindowController window]
                         anchoredAt:anchorPoint])) {
    bridge_ = bridge;
  }

  return self;
}

- (void)showWindow:(id)sender {
  [self loadView];
  [super showWindow:sender];
}

- (void)close {
  if (bridge_) {
    bridge_->OnBubbleClosed();
    bridge_ = nullptr;
  }
  [super close];
}

- (void)loadView {
  // Title is an NSTextView instead of an NSTextField to allow it to wrap.
  base::scoped_nsobject<NSTextView> titleLabel(
      [[NSTextView alloc] initWithFrame:NSZeroRect]);
  NSDictionary* attributes =
      @{NSFontAttributeName : [NSFont systemFontOfSize:15.0]};
  base::scoped_nsobject<NSAttributedString> attributedMessage(
      [[NSAttributedString alloc]
          initWithString:base::SysUTF16ToNSString(bridge_->GetWindowTitle())
              attributes:attributes]);
  [[titleLabel textStorage] setAttributedString:attributedMessage];
  [[titleLabel textContainer] setLineFragmentPadding:0.0f];
  [titleLabel setEditable:NO];
  [titleLabel setSelectable:NO];
  [titleLabel setDrawsBackground:NO];
  [titleLabel setVerticallyResizable:YES];
  [titleLabel setFrameSize:NSMakeSize(kDesiredBubbleWidth - (2 * kFramePadding),
                                      MAXFLOAT)];
  [titleLabel sizeToFit];

  // Credit card info.
  autofill::CreditCard card = bridge_->GetCard();
  base::scoped_nsobject<NSImageView> cardIcon(
      [[NSImageView alloc] initWithFrame:NSZeroRect]);
  [cardIcon setToolTip:base::SysUTF16ToNSString(card.TypeForDisplay())];
  [cardIcon setWantsLayer:YES];
  [[cardIcon layer] setBorderWidth:1.0];
  [[cardIcon layer] setCornerRadius:2.0];
  [[cardIcon layer] setMasksToBounds:YES];
  [[cardIcon layer]
      setBorderColor:skia::CGColorCreateFromSkColor(kIconBorderColor)];
  [cardIcon setImage:ResourceBundle::GetSharedInstance()
                         .GetNativeImageNamed(
                             autofill::CreditCard::IconResourceId(card.type()))
                         .AsNSImage()];
  [cardIcon setFrameSize:[[cardIcon image] size]];

  base::scoped_nsobject<NSTextField> lastFourLabel(
      [SaveCardBubbleViewCocoa makeTextField]);
  // Midline horizontal ellipsis follwed by last four digits.
  [lastFourLabel setStringValue:base::SysUTF16ToNSString(
                                    base::UTF8ToUTF16("\xE2\x8B\xAF") +
                                    card.LastFourDigits())];
  [lastFourLabel sizeToFit];

  base::scoped_nsobject<NSTextField> expirationDateLabel(
      [SaveCardBubbleViewCocoa makeTextField]);
  [expirationDateLabel
      setStringValue:base::SysUTF16ToNSString(
                         card.AbbreviatedExpirationDateForDisplay())];
  [expirationDateLabel sizeToFit];

  // "Learn more" link.
  base::scoped_nsobject<HyperlinkTextView> learnMoreLink(
      [[HyperlinkTextView alloc] initWithFrame:NSZeroRect]);
  NSString* learnMoreString = l10n_util::GetNSString(IDS_LEARN_MORE);
  [learnMoreLink
        setMessage:learnMoreString
          withFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]
      messageColor:[NSColor blackColor]];
  [learnMoreLink setDelegate:self];
  NSColor* linkColor =
      skia::SkColorToCalibratedNSColor(chrome_style::GetLinkColor());
  [learnMoreLink addLinkRange:NSMakeRange(0, [learnMoreString length])
                      withURL:nil
                    linkColor:linkColor];
  NSTextStorage* text = [learnMoreLink textStorage];
  [text addAttribute:NSUnderlineStyleAttributeName
               value:[NSNumber numberWithInt:NSUnderlineStyleNone]
               range:NSMakeRange(0, [learnMoreString length])];
  [[learnMoreLink textContainer] setLineFragmentPadding:0.0f];
  [learnMoreLink setVerticallyResizable:YES];
  [learnMoreLink
      setFrameSize:NSMakeSize(kDesiredBubbleWidth - 2 * kFramePadding,
                              MAXFLOAT)];
  [learnMoreLink sizeToFit];

  // Cancel button.
  base::scoped_nsobject<NSButton> cancelButton(
      [SaveCardBubbleViewCocoa makeButton]);
  [cancelButton
      setTitle:l10n_util::GetNSString(IDS_AUTOFILL_SAVE_CARD_PROMPT_DENY)];
  [cancelButton sizeToFit];
  [cancelButton setTarget:self];
  [cancelButton setAction:@selector(onCancelButton:)];
  [cancelButton setKeyEquivalent:@"\e"];

  // Save button.
  base::scoped_nsobject<NSButton> saveButton(
      [SaveCardBubbleViewCocoa makeButton]);
  [saveButton
      setTitle:l10n_util::GetNSString(IDS_AUTOFILL_SAVE_CARD_PROMPT_ACCEPT)];
  [saveButton sizeToFit];
  [saveButton setTarget:self];
  [saveButton setAction:@selector(onSaveButton:)];
  [saveButton setKeyEquivalent:@"\r"];

  // Layout.
  [saveButton setFrameOrigin:
      NSMakePoint(kDesiredBubbleWidth - kFramePadding -
                      NSWidth([saveButton frame]),
                  kFramePadding)];
  [cancelButton setFrameOrigin:
      NSMakePoint(NSMinX([saveButton frame]) -
                      kRelatedControlHorizontalPadding -
                      NSWidth([cancelButton frame]),
                  kFramePadding)];
  [learnMoreLink setFrameOrigin:
      NSMakePoint(kFramePadding,
                  NSMidY([saveButton frame]) -
                      (NSHeight([learnMoreLink frame]) / 2.0))];

  [cardIcon setFrameOrigin:
      NSMakePoint(kFramePadding,
                  NSMaxY([saveButton frame]) +
                      kUnrelatedControlVerticalPadding)];
  [lastFourLabel setFrameOrigin:
      NSMakePoint(NSMaxX([cardIcon frame]) + kRelatedControlHorizontalPadding,
                  NSMidY([cardIcon frame]) -
                      (NSHeight([lastFourLabel frame]) / 2.0))];
  [expirationDateLabel setFrameOrigin:
      NSMakePoint(NSMaxX([lastFourLabel frame]) +
                      kRelatedControlHorizontalPadding,
                  NSMidY([cardIcon frame]) -
                      (NSHeight([expirationDateLabel frame]) / 2.0))];

  [titleLabel setFrameOrigin:
      NSMakePoint(kFramePadding,
                  NSMaxY([cardIcon frame]) + kUnrelatedControlVerticalPadding)];

  [[[self window] contentView] setSubviews:@[
    titleLabel, cardIcon, lastFourLabel, expirationDateLabel, learnMoreLink,
    cancelButton, saveButton
  ]];

  // Update window frame.
  NSRect windowFrame = [[self window] frame];
  windowFrame.size.height = NSMaxY([titleLabel frame]) + kFramePadding;
  windowFrame.size.width = kDesiredBubbleWidth;
  [[self window] setFrame:windowFrame display:NO];
}

- (void)onSaveButton:(id)sender {
  DCHECK(bridge_);
  bridge_->OnSaveButton();
}

- (void)onCancelButton:(id)sender {
  DCHECK(bridge_);
  bridge_->OnCancelButton();
}

- (BOOL)textView:(NSTextView*)textView
    clickedOnLink:(id)link
          atIndex:(NSUInteger)charIndex {
  DCHECK(bridge_);
  bridge_->OnLearnMoreClicked();
  return YES;
}

@end
