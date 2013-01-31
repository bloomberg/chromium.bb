// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/search_token_decoration.h"

#import "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"

SearchTokenDecoration::SearchTokenDecoration() {
}

SearchTokenDecoration::~SearchTokenDecoration() {
}

void SearchTokenDecoration::SetSearchProviderName(
    const string16& search_provider_name) {
  if (search_provider_name_ == search_provider_name)
    return;
  search_provider_name_ = search_provider_name;

  NSDictionary* attributes = [NSDictionary dictionaryWithObjectsAndKeys:
      OmniboxViewMac::GetFieldFont(), NSFontAttributeName,
      OmniboxViewMac::SuggestTextColor(), NSForegroundColorAttributeName,
      nil];
  NSString* string = l10n_util::GetNSStringF(IDS_OMNIBOX_SEARCH_TOKEN_TEXT,
                                             search_provider_name);
  search_provider_attributed_string_.reset([[NSAttributedString alloc]
      initWithString:string
          attributes:attributes]);
}

void SearchTokenDecoration::DrawInFrame(NSRect frame, NSView* control_view) {
  NSRect text_rect = NSInsetRect(frame, 0, kTextYInset);
  [search_provider_attributed_string_ drawInRect:text_rect];
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
