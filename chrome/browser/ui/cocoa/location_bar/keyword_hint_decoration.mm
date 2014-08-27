// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#import "chrome/browser/ui/cocoa/location_bar/keyword_hint_decoration.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "skia/ext/skia_utils_mac.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

// How far to inset the hint image from sides.  Lines baseline of text
// in image with baseline of prefix and suffix.
const CGFloat kHintImageYInset = 4.0;

// Extra padding right and left of the image.
const CGFloat kHintImagePadding = 1.0;

// Maxmimum of the available space to allow the hint to take over.
// Should leave enough so that the user has space to edit things.
const CGFloat kHintAvailableRatio = 2.0 / 3.0;

// Helper to convert |s| to an |NSString|, trimming whitespace at
// ends.
NSString* TrimAndConvert(const base::string16& s) {
  base::string16 output;
  base::TrimWhitespace(s, base::TRIM_ALL, &output);
  return base::SysUTF16ToNSString(output);
}

}  // namespace

KeywordHintDecoration::KeywordHintDecoration() {
  NSColor* text_color = [NSColor lightGrayColor];
  attributes_.reset([@{ NSFontAttributeName : GetFont(),
                        NSForegroundColorAttributeName : text_color
                      } retain]);
}

KeywordHintDecoration::~KeywordHintDecoration() {
}

NSImage* KeywordHintDecoration::GetHintImage() {
  if (!hint_image_) {
    hint_image_.reset(ResourceBundle::GetSharedInstance().
        GetNativeImageNamed(IDR_OMNIBOX_KEYWORD_HINT_TAB).CopyNSImage());
  }
  return hint_image_;
}

void KeywordHintDecoration::SetKeyword(const base::string16& short_name,
                                       bool is_extension_keyword) {
  // KEYWORD_HINT is a message like "Press [tab] to search <site>".
  // [tab] is a parameter to be replaced by an image.  "<site>" is
  // derived from |short_name|.
  std::vector<size_t> content_param_offsets;
  int message_id = is_extension_keyword ?
      IDS_OMNIBOX_EXTENSION_KEYWORD_HINT : IDS_OMNIBOX_KEYWORD_HINT;
  const base::string16 keyword_hint(
      l10n_util::GetStringFUTF16(message_id,
                                 base::string16(), short_name,
                                 &content_param_offsets));

  // Should always be 2 offsets, see the comment in
  // location_bar_view.cc after IDS_OMNIBOX_KEYWORD_HINT fetch.
  DCHECK_EQ(content_param_offsets.size(), 2U);

  // Where to put the [tab] image.
  const size_t split = content_param_offsets.front();

  // Trim the spaces from the edges (there is space in the image) and
  // convert to |NSString|.
  hint_prefix_.reset([TrimAndConvert(keyword_hint.substr(0, split)) retain]);
  hint_suffix_.reset([TrimAndConvert(keyword_hint.substr(split)) retain]);
}

CGFloat KeywordHintDecoration::GetWidthForSpace(CGFloat width) {
  NSImage* image = GetHintImage();
  const CGFloat image_width = image ? [image size].width : 0.0;

  // AFAICT, on Windows the choices are "everything" if it fits, then
  // "image only" if it fits.

  // Entirely too small to fit, omit.
  if (width < image_width)
    return kOmittedWidth;

  // Show the full hint if it won't take up too much space.  The image
  // needs to be placed at a pixel boundary, round the text widths so
  // that any partially-drawn pixels don't look too close (or too
  // far).
  CGFloat full_width =
      std::floor(GetLabelSize(hint_prefix_, attributes_).width + 0.5) +
      kHintImagePadding + image_width + kHintImagePadding +
      std::floor(GetLabelSize(hint_suffix_, attributes_).width + 0.5);
  if (full_width <= width * kHintAvailableRatio)
    return full_width;

  return image_width;
}

void KeywordHintDecoration::DrawInFrame(NSRect frame, NSView* control_view) {
  NSImage* image = GetHintImage();
  const CGFloat image_width = image ? [image size].width : 0.0;

  const bool draw_full = NSWidth(frame) > image_width;

  if (draw_full) {
    NSRect prefix_rect = frame;
    const CGFloat prefix_width = GetLabelSize(hint_prefix_, attributes_).width;
    DCHECK_GE(NSWidth(prefix_rect), prefix_width);
    DrawLabel(hint_prefix_, attributes_, prefix_rect);

    // The image should be drawn at a pixel boundary, round the prefix
    // so that partial pixels aren't oddly close (or distant).
    frame.origin.x += std::floor(prefix_width + 0.5) + kHintImagePadding;
    frame.size.width -= std::floor(prefix_width + 0.5) + kHintImagePadding;
  }

  NSRect image_rect = NSInsetRect(frame, 0.0, kHintImageYInset);
  image_rect.size = [image size];
  [image drawInRect:image_rect
            fromRect:NSZeroRect  // Entire image
           operation:NSCompositeSourceOver
            fraction:1.0
      respectFlipped:YES
               hints:nil];
  frame.origin.x += NSWidth(image_rect);
  frame.size.width -= NSWidth(image_rect);

  if (draw_full) {
    NSRect suffix_rect = frame;
    const CGFloat suffix_width = GetLabelSize(hint_suffix_, attributes_).width;

    // Draw the text kHintImagePadding away from [tab] icon so that
    // equal amount of space is maintained on either side of the icon.
    // This also ensures that suffix text is at the same distance
    // from [tab] icon in different web pages.
    suffix_rect.origin.x += kHintImagePadding;
    DCHECK_GE(NSWidth(suffix_rect), suffix_width);
    DrawLabel(hint_suffix_, attributes_, suffix_rect);
  }
}
