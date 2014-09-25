// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/bookmarks/bookmark_sync_promo_controller.h"

#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/chrome_style.h"
#include "chrome/grit/generated_resources.h"
#include "skia/ext/skia_utils_mac.h"
#include "third_party/skia/include/core/SkColor.h"
#import "ui/base/cocoa/controls/hyperlink_text_view.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

// Remove underlining from the specified range of characters in a text view.
void RemoveUnderlining(NSTextView* textView, int offset, int length) {
  [textView setLinkTextAttributes:nil];
  NSTextStorage* text = [textView textStorage];
  NSRange range = NSMakeRange(offset, length);
  [text addAttribute:NSUnderlineStyleAttributeName
               value:[NSNumber numberWithInt:NSUnderlineStyleNone]
               range:range];
}

const SkColor kTextColor = SkColorSetRGB(0x66, 0x66, 0x66);
const SkColor kBackgroundColor = SkColorSetRGB(0xf5, 0xf5, 0xf5);
const SkColor kBorderColor = SkColorSetRGB(0xe5, 0xe5, 0xe5);

// Vertical padding of the promo (dp).
const CGFloat kVerticalPadding = 15;

// Width of the border (dp).
const CGFloat kBorderWidth = 1.0;

// Font size of the promo text (pt).
const int kFontSize = 11;

}  // namespace

@implementation BookmarkSyncPromoController

- (id)initWithBrowser:(Browser*)browser {
  if ((self = [super init])) {
    browser_ = browser;
  }
  return self;
}

- (CGFloat)borderWidth {
  return kBorderWidth;
}

- (CGFloat)preferredHeightForWidth:(CGFloat)width {
  CGFloat availableWidth =
      width - (2 * chrome_style::kHorizontalPadding) - (2 * kBorderWidth);
  NSRect frame = [[textView_ textStorage]
      boundingRectWithSize:NSMakeSize(availableWidth, 0.0)
                   options:NSStringDrawingUsesLineFragmentOrigin];
  return frame.size.height + (2 * kVerticalPadding) + (2 * kBorderWidth);
}

- (void)loadView {
  NSBox* promoView = [[[NSBox alloc] init] autorelease];
  [promoView setBoxType:NSBoxCustom];
  [promoView setFillColor:gfx::SkColorToDeviceNSColor(kBackgroundColor)];
  [promoView setContentViewMargins:NSMakeSize(chrome_style::kHorizontalPadding,
                                              kVerticalPadding)];
  [promoView setBorderType:NSLineBorder];
  [promoView setBorderWidth:kBorderWidth];
  [promoView setBorderColor:gfx::SkColorToDeviceNSColor(kBorderColor)];

  // Add the sync promo text.
  size_t offset;
  const base::string16 linkText = l10n_util::GetStringUTF16(
      IDS_BOOKMARK_SYNC_PROMO_LINK);
  const base::string16 promoText =  l10n_util::GetStringFUTF16(
      IDS_BOOKMARK_SYNC_PROMO_MESSAGE,
      linkText, &offset);
  const base::string16 promoTextWithoutLink =
      promoText.substr(0, offset) +
      promoText.substr(offset + linkText.size());

  NSFont* font = [NSFont labelFontOfSize:kFontSize];
  NSColor* linkColor = gfx::SkColorToCalibratedNSColor(
      chrome_style::GetLinkColor());

  textView_.reset([[HyperlinkTextView alloc] init]);
  [textView_ setMessageAndLink:base::SysUTF16ToNSString(promoTextWithoutLink)
                      withLink:base::SysUTF16ToNSString(linkText)
                      atOffset:offset
                          font:font
                  messageColor:gfx::SkColorToDeviceNSColor(kTextColor)
                     linkColor:linkColor];
  [textView_ setRefusesFirstResponder:YES];
  [[textView_ textContainer] setLineFragmentPadding:0.0];
  RemoveUnderlining(textView_, offset, linkText.size());
  [textView_ setDelegate:self];

  [promoView setContentView:textView_];

  [self setView:promoView];
}

- (BOOL)textView:(NSTextView *)textView
   clickedOnLink:(id)link
         atIndex:(NSUInteger)charIndex {
  chrome::ShowBrowserSignin(browser_, signin::SOURCE_BOOKMARK_BUBBLE);
  return YES;
}

@end
