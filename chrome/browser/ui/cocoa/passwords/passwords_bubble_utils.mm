// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/passwords/passwords_bubble_utils.h"

#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/chrome_style.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/cocoa/controls/hyperlink_text_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/resources/grit/ui_resources.h"

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
  return std::make_pair(
      columnsWidth.first + freeSpace * firstColumnPercent,
      columnsWidth.second + freeSpace * (1 - firstColumnPercent));
}

NSSecureTextField* PasswordLabel(const base::string16& text) {
  base::scoped_nsobject<NSSecureTextField> textField(
      [[NSSecureTextField alloc] initWithFrame:NSZeroRect]);
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

HyperlinkTextView* TitleLabelWithLink(const base::string16& text,
                                      gfx::Range range,
                                      id<NSTextViewDelegate> delegate) {
  base::scoped_nsobject<HyperlinkTextView> titleView(
      [[HyperlinkTextView alloc] initWithFrame:NSZeroRect]);
  NSColor* textColor = [NSColor blackColor];
  NSFont* font = ResourceBundle::GetSharedInstance()
                     .GetFontList(ResourceBundle::MediumFont)
                     .GetPrimaryFont()
                     .GetNativeFont();
  [titleView setMessage:base::SysUTF16ToNSString(text)
               withFont:font
           messageColor:textColor];
  NSRange titleBrandLinkRange = range.ToNSRange();
  if (titleBrandLinkRange.length) {
    NSColor* linkColor =
        skia::SkColorToCalibratedNSColor(chrome_style::GetLinkColor());
    [titleView addLinkRange:titleBrandLinkRange
                    withURL:nil
                  linkColor:linkColor];
    [titleView setDelegate:delegate];

    // Create the link with no underlining.
    [titleView setLinkTextAttributes:nil];
    NSTextStorage* text = [titleView textStorage];
    [text addAttribute:NSUnderlineStyleAttributeName
                 value:@(NSUnderlineStyleNone)
                 range:titleBrandLinkRange];
  } else {
    // TODO(vasilii): remove if crbug.com/515189 is fixed.
    [titleView setRefusesFirstResponder:YES];
  }

  return titleView.autorelease();
}
