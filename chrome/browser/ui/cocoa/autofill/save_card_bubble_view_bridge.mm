// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/autofill/save_card_bubble_view_bridge.h"

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "chrome/browser/ui/cocoa/browser_window_controller.h"
#include "chrome/browser/ui/cocoa/chrome_style.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#import "chrome/browser/ui/cocoa/toolbar/toolbar_controller.h"
#include "components/autofill/core/browser/ui/save_card_bubble_controller.h"
#include "components/strings/grit/components_strings.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/cocoa/cocoa_base_utils.h"
#import "ui/base/cocoa/controls/hyperlink_text_view.h"
#import "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using base::SysUTF16ToNSString;

namespace {

const CGFloat kDesiredBubbleWidth = 370;
const CGFloat kDividerHeight = 1;
const CGFloat kFramePadding = 16;
const CGFloat kRelatedControlHorizontalPadding = 2;
const CGFloat kRelatedControlVerticalPadding = 5;
const CGFloat kUnrelatedControlVerticalPadding = 15;

const SkColor kDividerColor = 0xFFE9E9E9;  // SkColorSetRGB(0xE9, 0xE9, 0xE9);
const SkColor kFooterColor = 0xFFF5F5F5;   // SkColorSetRGB(0xF5, 0xF5, 0xF5);
const SkColor kIconBorderColor = 0x10000000;  // SkColorSetARGB(0x10, 0, 0, 0);

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

base::string16 SaveCardBubbleViewBridge::GetExplanatoryMessage() const {
  return controller_ ? controller_->GetExplanatoryMessage() : base::string16();
}

CreditCard SaveCardBubbleViewBridge::GetCard() const {
  return controller_ ? controller_->GetCard() : CreditCard();
}

const LegalMessageLines SaveCardBubbleViewBridge::GetLegalMessageLines() const {
  return controller_ ? controller_->GetLegalMessageLines()
                     : LegalMessageLines();
}

void SaveCardBubbleViewBridge::OnSaveButton() {
  if (controller_)
    controller_->OnSaveButton(base::string16());
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

void SaveCardBubbleViewBridge::OnLegalMessageLinkClicked(const GURL& url) {
  if (controller_)
    controller_->OnLegalMessageLinkClicked(url);
}

void SaveCardBubbleViewBridge::OnBubbleClosed() {
  if (controller_)
    controller_->OnBubbleClosed();

  delete this;
}

void SaveCardBubbleViewBridge::Hide() {
  if (controller_) {
    // SaveCardBubbleViewBridge::OnBubbleClosed won't be able to call
    // OnBubbleClosed on the bubble controller since we null the reference to it
    // below. So we need to call it here.
    controller_->OnBubbleClosed();
    controller_ = nullptr;
  }
  [view_controller_ close];
}

}  // autofill

#pragma mark SaveCardBubbleViewCocoa

@interface SaveCardBubbleViewCocoa ()
+ (base::scoped_nsobject<NSTextField>)makeLabel:(NSString*)text;
+ (base::scoped_nsobject<NSTextView>)makeWrappingLabel:(NSString*)text
                                          withFontSize:(CGFloat)fontSize;
+ (base::scoped_nsobject<HyperlinkTextView>)makeHyperlinkText:(NSString*)text;
+ (base::scoped_nsobject<NSButton>)makeButton:(NSString*)text;
@end

@implementation SaveCardBubbleViewCocoa {
  autofill::SaveCardBubbleViewBridge* bridge_;  // Weak.
}

+ (base::scoped_nsobject<NSTextField>)makeLabel:(NSString*)text {
  base::scoped_nsobject<NSTextField> textField(
      [[NSTextField alloc] initWithFrame:NSZeroRect]);
  [textField setEditable:NO];
  [textField setSelectable:NO];
  [textField setDrawsBackground:NO];
  [textField setBezeled:NO];
  [textField setStringValue:text];
  [textField sizeToFit];

  return textField;
}

+ (base::scoped_nsobject<NSTextView>)makeWrappingLabel:(NSString*)text
                                          withFontSize:(CGFloat)fontSize {
  base::scoped_nsobject<NSTextView> textView(
      [[NSTextView alloc] initWithFrame:NSZeroRect]);
  NSDictionary* attributes =
      @{NSFontAttributeName : [NSFont systemFontOfSize:fontSize]};
  base::scoped_nsobject<NSAttributedString> attributedMessage(
      [[NSAttributedString alloc] initWithString:text attributes:attributes]);
  [[textView textStorage] setAttributedString:attributedMessage];
  [[textView textContainer] setLineFragmentPadding:0.0f];
  [textView setEditable:NO];
  [textView setSelectable:NO];
  [textView setDrawsBackground:NO];
  [textView setVerticallyResizable:YES];
  [textView setFrameSize:NSMakeSize(kDesiredBubbleWidth - (2 * kFramePadding),
                                    MAXFLOAT)];
  [textView sizeToFit];

  return textView;
}

+ (base::scoped_nsobject<HyperlinkTextView>)makeHyperlinkText:(NSString*)text {
  base::scoped_nsobject<HyperlinkTextView> lineView(
      [[HyperlinkTextView alloc] initWithFrame:NSZeroRect]);
  [lineView setMessage:text
              withFont:[NSFont systemFontOfSize:[NSFont systemFontSize]]
          messageColor:[NSColor blackColor]];

  [[lineView textContainer] setLineFragmentPadding:0.0f];
  [lineView setVerticallyResizable:YES];
  [lineView setFrameSize:NSMakeSize(kDesiredBubbleWidth - 2 * kFramePadding,
                                    MAXFLOAT)];
  [lineView sizeToFit];

  return lineView;
}

+ (base::scoped_nsobject<NSButton>)makeButton:(NSString*)text {
  base::scoped_nsobject<NSButton> button(
      [[NSButton alloc] initWithFrame:NSZeroRect]);
  [button setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
  [button setBezelStyle:NSRoundedBezelStyle];
  [[button cell] setControlSize:NSSmallControlSize];
  [button setTitle:text];
  [button sizeToFit];

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
  anchorPoint = ui::ConvertPointFromWindowToScreen(
      [browserWindowController window], anchorPoint);

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
  // Title.
  NSString* title = SysUTF16ToNSString(bridge_->GetWindowTitle());
  base::scoped_nsobject<NSTextView> titleLabel(
      [SaveCardBubbleViewCocoa makeWrappingLabel:title withFontSize:15.0]);

  // Credit card info.
  autofill::CreditCard card = bridge_->GetCard();
  base::scoped_nsobject<NSImageView> cardIcon(
      [[NSImageView alloc] initWithFrame:NSZeroRect]);
  [cardIcon setToolTip:base::SysUTF16ToNSString(card.NetworkForDisplay())];
  [cardIcon setWantsLayer:YES];
  [[cardIcon layer] setBorderWidth:1.0];
  [[cardIcon layer] setCornerRadius:2.0];
  [[cardIcon layer] setMasksToBounds:YES];
  [[cardIcon layer]
      setBorderColor:skia::CGColorCreateFromSkColor(kIconBorderColor)];
  [cardIcon
      setImage:ResourceBundle::GetSharedInstance()
                   .GetNativeImageNamed(
                       autofill::CreditCard::IconResourceId(card.network()))
                   .AsNSImage()];
  [cardIcon setFrameSize:[[cardIcon image] size]];

  // Midline horizontal ellipsis follwed by last four digits.
  base::scoped_nsobject<NSTextField> lastFourLabel([SaveCardBubbleViewCocoa
      makeLabel:SysUTF16ToNSString(base::string16(autofill::kMidlineEllipsis) +
                                   card.LastFourDigits())]);

  base::scoped_nsobject<NSTextField> expirationDateLabel(
      [SaveCardBubbleViewCocoa
          makeLabel:base::SysUTF16ToNSString(
                        card.AbbreviatedExpirationDateForDisplay())]);

  // Explanatory text (only shown for upload).
  base::scoped_nsobject<NSTextView> explanationLabel(
      [[NSTextView alloc] initWithFrame:NSZeroRect]);
  base::string16 explanation = bridge_->GetExplanatoryMessage();
  if (!explanation.empty()) {
    explanationLabel.reset([SaveCardBubbleViewCocoa
                               makeWrappingLabel:SysUTF16ToNSString(explanation)
                                    withFontSize:[NSFont systemFontSize]]
                               .release());
  }

  // "Learn more" link.
  NSString* learnMoreString = l10n_util::GetNSString(IDS_LEARN_MORE);
  base::scoped_nsobject<HyperlinkTextView> learnMoreLink(
      [SaveCardBubbleViewCocoa makeHyperlinkText:learnMoreString]);
  [learnMoreLink setDelegate:self];

  NSColor* linkColor =
      skia::SkColorToCalibratedNSColor(chrome_style::GetLinkColor());
  NSRange range = NSMakeRange(0, [learnMoreString length]);
  [learnMoreLink addLinkRange:range withURL:nil linkColor:linkColor];
  [[learnMoreLink textStorage] addAttribute:NSUnderlineStyleAttributeName
                                      value:@(NSUnderlineStyleNone)
                                      range:range];

  // Cancel button.
  base::scoped_nsobject<NSButton> cancelButton([SaveCardBubbleViewCocoa
      makeButton:l10n_util::GetNSString(IDS_NO_THANKS)]);
  [cancelButton setTarget:self];
  [cancelButton setAction:@selector(onCancelButton:)];
  [cancelButton setKeyEquivalent:@"\e"];

  // Save button.
  base::scoped_nsobject<NSButton> saveButton([SaveCardBubbleViewCocoa
      makeButton:l10n_util::GetNSString(IDS_AUTOFILL_SAVE_CARD_PROMPT_ACCEPT)]);
  [saveButton setTarget:self];
  [saveButton setAction:@selector(onSaveButton:)];
  [saveButton setKeyEquivalent:@"\r"];

  // Footer with legal text (only shown for upload).
  base::scoped_nsobject<NSBox> divider(
      [[NSBox alloc] initWithFrame:NSZeroRect]);
  base::scoped_nsobject<NSView> footerView(
      [[NSView alloc] initWithFrame:NSZeroRect]);
  const autofill::LegalMessageLines& lines = bridge_->GetLegalMessageLines();
  if (!lines.empty()) {
    [divider setBoxType:NSBoxCustom];
    [divider setBorderType:NSLineBorder];
    [divider setBorderColor:skia::SkColorToCalibratedNSColor(kDividerColor)];
    [divider setFrameSize:NSMakeSize(kDesiredBubbleWidth, kDividerHeight)];

    [footerView setWantsLayer:YES];
    [[footerView layer]
        setBackgroundColor:skia::CGColorCreateFromSkColor(kFooterColor)];

    CGFloat linesHeight = kFramePadding;
    for (auto lineIter = lines.rbegin(); lineIter != lines.rend(); ++lineIter) {
      // Create the legal message line view.
      base::scoped_nsobject<HyperlinkTextView> lineView([SaveCardBubbleViewCocoa
          makeHyperlinkText:SysUTF16ToNSString(lineIter->text())]);
      [lineView setDelegate:self];

      // Add the links.
      for (const autofill::LegalMessageLine::Link& link : lineIter->links()) {
        NSRange range = NSMakeRange(link.range.start(), link.range.length());
        [lineView addLinkRange:range withURL:nil linkColor:linkColor];
        [[lineView textStorage] addAttribute:NSUnderlineStyleAttributeName
                                       value:@(NSUnderlineStyleNone)
                                       range:range];
      }

      // Add the line to the footer view.
      [footerView addSubview:lineView];
      [lineView setFrameOrigin:NSMakePoint(kFramePadding, linesHeight)];
      linesHeight +=
          [lineView frame].size.height + kRelatedControlVerticalPadding;
    }
    [footerView setFrameSize:NSMakeSize(kDesiredBubbleWidth,
                                        linesHeight + kFramePadding)];
  }

  // Layout.
  [footerView setFrameOrigin:NSZeroPoint];

  [divider setFrameOrigin:NSMakePoint(0, NSMaxY([footerView frame]))];

  [saveButton setFrameOrigin:
      NSMakePoint(kDesiredBubbleWidth - kFramePadding -
                      NSWidth([saveButton frame]),
                  NSMaxY([divider frame]) + kFramePadding)];
  [cancelButton setFrameOrigin:
      NSMakePoint(NSMinX([saveButton frame]) -
                      kRelatedControlHorizontalPadding -
                      NSWidth([cancelButton frame]),
                  NSMaxY([divider frame]) + kFramePadding)];
  [learnMoreLink setFrameOrigin:
      NSMakePoint(kFramePadding,
                  NSMidY([saveButton frame]) -
                      (NSHeight([learnMoreLink frame]) / 2.0))];

  [explanationLabel setFrameOrigin:
      NSMakePoint(kFramePadding,
                  NSMaxY([saveButton frame]) +
                      kUnrelatedControlVerticalPadding)];

  NSView* viewBelowIcon =
      ([explanationLabel frame].size.height > 0) ? explanationLabel.get()
                                                 : saveButton.get();
  [cardIcon setFrameOrigin:
      NSMakePoint(kFramePadding,
                  NSMaxY([viewBelowIcon frame]) +
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
    titleLabel, cardIcon, lastFourLabel, expirationDateLabel, explanationLabel,
    learnMoreLink, cancelButton, saveButton, divider, footerView
  ]];

  // Update window frame.
  NSRect windowFrame = [[self window] frame];
  windowFrame.size.height = NSMaxY([titleLabel frame]) + kFramePadding +
                            info_bubble::kBubbleArrowHeight;
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

  // Check each of the links in each of the legal message lines ot see if they
  // are the source of the click.
  const autofill::LegalMessageLines& lines = bridge_->GetLegalMessageLines();
  for (const autofill::LegalMessageLine& line : lines) {
    if (line.text() ==
        base::SysNSStringToUTF16([[textView textStorage] string])) {
      for (const autofill::LegalMessageLine::Link& link : line.links()) {
        if (link.range.start() <= charIndex && charIndex < link.range.end()) {
          bridge_->OnLegalMessageLinkClicked(link.url);
          return YES;
        }
      }
    }
  }

  // If none of the legal message links are the source of the click, the source
  // must be the learn more link.
  bridge_->OnLearnMoreClicked();
  return YES;
}

@end
