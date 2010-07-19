// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/location_bar/keyword_hint_decoration.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/logging.h"
#include "base/sys_string_conversions.h"
#import "chrome/browser/cocoa/image_utils.h"
#include "grit/theme_resources.h"
#include "grit/generated_resources.h"
#include "skia/ext/skia_utils_mac.h"

namespace {

// How far to inset the hint text area from sides.
const CGFloat kHintTextYInset = 4.0;

// How far to inset the hint image from sides.  Lines baseline of text
// in image with baseline of prefix and suffix.
const CGFloat kHintImageYInset = 4.0;

// Maxmimum of the available space to allow the hint to take over.
// Should leave enough so that the user has space to edit things.
const CGFloat kHintAvailableRatio = 2.0 / 3.0;

}  // namespace

KeywordHintDecoration::KeywordHintDecoration(NSFont* font) {
  NSColor* text_color = [NSColor lightGrayColor];
  NSDictionary* attributes =
      [NSDictionary dictionaryWithObjectsAndKeys:
           font, NSFontAttributeName,
           text_color, NSForegroundColorAttributeName,
           nil];
  attributes_.reset([attributes retain]);
}

KeywordHintDecoration::~KeywordHintDecoration() {
}

NSImage* KeywordHintDecoration::GetHintImage() {
  if (!hint_image_) {
    SkBitmap* skiaBitmap = ResourceBundle::GetSharedInstance().
        GetBitmapNamed(IDR_LOCATION_BAR_KEYWORD_HINT_TAB);
    if (skiaBitmap)
      hint_image_.reset([gfx::SkBitmapToNSImage(*skiaBitmap) retain]);
  }
  return hint_image_;
}

void KeywordHintDecoration::SetKeyword(const std::wstring& short_name,
                                       bool is_extension_keyword) {
  // KEYWORD_HINT is a message like "Press [tab] to search <site>".
  // [tab] is a parameter to be replaced by an image.  "<site>" is
  // derived from |short_name|.
  std::vector<size_t> content_param_offsets;
  int message_id = is_extension_keyword ?
      IDS_OMNIBOX_EXTENSION_KEYWORD_HINT : IDS_OMNIBOX_KEYWORD_HINT;
  const std::wstring keyword_hint(
      l10n_util::GetStringF(message_id,
                            std::wstring(), short_name,
                            &content_param_offsets));

  // Should always be 2 offsets, see the comment in
  // location_bar_view.cc after IDS_OMNIBOX_KEYWORD_HINT fetch.
  DCHECK_EQ(content_param_offsets.size(), 2U);

  // Where to put the [tab] image.
  const size_t split = content_param_offsets.front();

  hint_prefix_.reset(
      [base::SysWideToNSString(keyword_hint.substr(0, split)) retain]);
  hint_suffix_.reset(
      [base::SysWideToNSString(keyword_hint.substr(split)) retain]);
}

CGFloat KeywordHintDecoration::GetWidthForSpace(CGFloat width) {
  NSImage* image = GetHintImage();
  const CGFloat image_width = image ? [image size].width : 0.0;

  // AFAICT, on Windows the choices are "everything" if it fits, then
  // "image only" if it fits.

  // Entirely too small to fit, omit.
  if (width < image_width)
    return kOmittedWidth;

  // Show the full hint if it won't take up too much space.
  CGFloat full_width =
      [hint_prefix_ sizeWithAttributes:attributes_].width +
      image_width + [hint_suffix_ sizeWithAttributes:attributes_].width;
  if (full_width <= width * kHintAvailableRatio)
    return full_width;

  return image_width;
}

void KeywordHintDecoration::DrawInFrame(NSRect frame, NSView* control_view) {
  NSImage* image = GetHintImage();
  const CGFloat image_width = image ? [image size].width : 0.0;

  const bool draw_full = NSWidth(frame) > image_width;

  if (draw_full) {
    NSRect prefixRect = NSInsetRect(frame, 0.0, kHintTextYInset);
    prefixRect.size.width =
        [hint_prefix_ sizeWithAttributes:attributes_].width;
    [hint_prefix_ drawInRect:prefixRect withAttributes:attributes_];
    frame.origin.x += NSWidth(prefixRect);
    frame.size.width -= NSWidth(prefixRect);
  }

  NSRect imageRect = NSInsetRect(frame, 0.0, kHintImageYInset);
  imageRect.size = [image size];
  [image drawInRect:imageRect
            fromRect:NSZeroRect  // Entire image
           operation:NSCompositeSourceOver
            fraction:1.0
        neverFlipped:YES];
  frame.origin.x += NSWidth(imageRect);
  frame.size.width -= NSWidth(imageRect);

  if (draw_full) {
    NSRect suffixRect = NSInsetRect(frame, 0.0, kHintTextYInset);
    suffixRect.size.width =
        [hint_suffix_ sizeWithAttributes:attributes_].width;
    [hint_suffix_ drawInRect:suffixRect withAttributes:attributes_];
  }
}
