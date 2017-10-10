// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/passwords_bubble_utils.h"

#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/cocoa/chrome_style.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/cocoa/controls/hyperlink_text_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

HyperlinkTextView* LabelWithLink(const base::string16& text,
                                 SkColor color,
                                 NSFont* font,
                                 gfx::Range range,
                                 id<NSTextViewDelegate> delegate) {
  base::scoped_nsobject<HyperlinkTextView> textView(
      [[HyperlinkTextView alloc] initWithFrame:NSZeroRect]);
  NSColor* textColor = skia::SkColorToCalibratedNSColor(color);
  [textView setMessage:base::SysUTF16ToNSString(text)
              withFont:font
          messageColor:textColor];
  NSRange brandRange = range.ToNSRange();
  if (brandRange.length) {
    NSColor* linkColor =
        skia::SkColorToCalibratedNSColor(chrome_style::GetLinkColor());
    [textView addLinkRange:brandRange
                    withURL:nil
                  linkColor:linkColor];
    [textView setDelegate:delegate];

    // Create the link with no underlining.
    [textView setLinkTextAttributes:nil];
    NSTextStorage* text = [textView textStorage];
    [text addAttribute:NSUnderlineStyleAttributeName
                 value:@(NSUnderlineStyleNone)
                 range:brandRange];
  } else {
    // TODO(vasilii): remove if crbug.com/515189 is fixed.
    [textView setRefusesFirstResponder:YES];
  }

  return textView.autorelease();
}

}  // namespace

NSFont* LabelFont() {
  return [NSFont systemFontOfSize:[NSFont smallSystemFontSize]];
}

NSSize LabelSize(int resourceID) {
  return [l10n_util::GetNSString(resourceID)
      sizeWithAttributes:@{NSFontAttributeName : LabelFont()}];
}

void InitLabel(NSTextField* textField, const base::string16& text) {
  [textField setStringValue:base::SysUTF16ToNSString(text)];
  [textField setEditable:NO];
  [textField setSelectable:NO];
  [textField setDrawsBackground:NO];
  [textField setBezeled:NO];
  [textField setFont:LabelFont()];
  [[textField cell] setLineBreakMode:NSLineBreakByTruncatingTail];
  [textField sizeToFit];
}

std::pair<CGFloat, CGFloat> GetResizedColumns(
    CGFloat maxWidth,
    std::pair<CGFloat, CGFloat> columnsWidth) {
  DCHECK_GE(maxWidth, kItemLabelSpacing + kMinUsernameSize);
  // Free space can be negative.
  CGFloat freeSpace =
      maxWidth - (columnsWidth.first + columnsWidth.second + kItemLabelSpacing);
  if (freeSpace >= 0) {
    return std::make_pair(columnsWidth.first + freeSpace / 2,
                          columnsWidth.second + freeSpace / 2);
  }
  // Make sure that the sizes are nonnegative.
  CGFloat firstColumnPercent =
      columnsWidth.first / (columnsWidth.first + columnsWidth.second);
  CGFloat firstColumnSize = std::max(
      kMinUsernameSize, columnsWidth.first + freeSpace * firstColumnPercent);
  return std::make_pair(
      firstColumnSize,
      maxWidth - kItemLabelSpacing - firstColumnSize);
}

NSSecureTextField* PasswordLabel(const base::string16& text) {
  base::scoped_nsobject<NSSecureTextField> textField(
      [[NSSecureTextField alloc] initWithFrame:NSZeroRect]);
  InitLabel(textField, text);
  return textField.autorelease();
}

NSTextField* Label(const base::string16& text) {
  base::scoped_nsobject<NSTextField> textField(
      [[NSTextField alloc] initWithFrame:NSZeroRect]);
  InitLabel(textField, text);
  return textField.autorelease();
}

NSButton* DialogButton(NSString* title) {
  base::scoped_nsobject<NSButton> button(
      [[NSButton alloc] initWithFrame:NSZeroRect]);
  [button setFont:[NSFont systemFontOfSize:[NSFont smallSystemFontSize]]];
  [button setTitle:title];
  [button setBezelStyle:NSRoundedBezelStyle];
  [[button cell] setControlSize:NSSmallControlSize];
  [button sizeToFit];
  return button.autorelease();
}

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

HyperlinkTextView* TitleBubbleLabelWithLink(const base::string16& text,
                                            gfx::Range range,
                                            id<NSTextViewDelegate> delegate) {
  return LabelWithLink(
      text, SK_ColorBLACK,
      [NSFont systemFontOfSize:[NSFont systemFontSize]],
      range, delegate);
}

HyperlinkTextView* TitleDialogLabelWithLink(const base::string16& text,
                                            gfx::Range range,
                                            id<NSTextViewDelegate> delegate) {
  return LabelWithLink(text, SK_ColorBLACK,
                       ui::ResourceBundle::GetSharedInstance()
                           .GetFontList(chrome_style::kTitleFontStyle)
                           .GetPrimaryFont()
                           .GetNativeFont(),
                       range, delegate);
}

HyperlinkTextView* LabelWithLink(const base::string16& text,
                                 SkColor color,
                                 gfx::Range range,
                                 id<NSTextViewDelegate> delegate) {
  return LabelWithLink(text, color,
                       ui::ResourceBundle::GetSharedInstance()
                           .GetFontList(ui::ResourceBundle::SmallFont)
                           .GetPrimaryFont()
                           .GetNativeFont(),
                       range, delegate);
}
