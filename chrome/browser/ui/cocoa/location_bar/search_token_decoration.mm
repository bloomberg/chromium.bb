// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/search_token_decoration.h"

#include "base/strings/sys_string_conversions.h"
#import "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"

SearchTokenDecoration::SearchTokenDecoration() {
}

SearchTokenDecoration::~SearchTokenDecoration() {
}

void SearchTokenDecoration::SetSearchTokenText(
    const string16& search_token_text) {
  if (search_token_text_ == search_token_text)
    return;
  search_token_text_ = search_token_text;

  NSDictionary* attributes = @{
      NSFontAttributeName : GetFont(),
      NSForegroundColorAttributeName : OmniboxViewMac::SuggestTextColor()
  };
  search_provider_attributed_string_.reset([[NSAttributedString alloc]
      initWithString:base::SysUTF16ToNSString(search_token_text_)
          attributes:attributes]);
}

void SearchTokenDecoration::DrawInFrame(NSRect frame, NSView* control_view) {
  DrawAttributedString(search_provider_attributed_string_, frame);
}

CGFloat SearchTokenDecoration::GetWidthForSpace(CGFloat width,
                                                CGFloat text_width) {
  // Padding between the control's text and the search token.
  const CGFloat kAutoCollapsePadding = 20.0;

  // Collapse if the search token would reduce the space remaining for the
  // omnibox text.
  CGFloat desired_width = [search_provider_attributed_string_ size].width;
  CGFloat remaining_width = width - desired_width - kAutoCollapsePadding;
  if (remaining_width < text_width)
    return kOmittedWidth;

  return desired_width;
}
