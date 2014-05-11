// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/search_button_decoration.h"

#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#import "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#include "grit/theme_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

const CGFloat kInnerPadding = 16;
const CGFloat kLeftPadding = 3;

}  // namespace

SearchButtonDecoration::SearchButtonDecoration(LocationBarViewMac* owner)
    : ButtonDecoration(IMAGE_GRID(IDR_OMNIBOX_SEARCH_BUTTON),
                       IDR_OMNIBOX_SEARCH_BUTTON_LOUPE,
                       IMAGE_GRID(IDR_OMNIBOX_SEARCH_BUTTON_HOVER),
                       IDR_OMNIBOX_SEARCH_BUTTON_LOUPE,
                       IMAGE_GRID(IDR_OMNIBOX_SEARCH_BUTTON_PRESSED),
                       IDR_OMNIBOX_SEARCH_BUTTON_LOUPE,
                       kInnerPadding),
      owner_(owner) {
  DCHECK(owner_);
}

SearchButtonDecoration::~SearchButtonDecoration() {
}

CGFloat SearchButtonDecoration::GetWidthForSpace(CGFloat width) {
  CGFloat width_for_space = ButtonDecoration::GetWidthForSpace(width);
  if (width_for_space == kOmittedWidth ||
      width_for_space + kLeftPadding > width)
    return kOmittedWidth;
  return width_for_space + kLeftPadding;
}

void SearchButtonDecoration::DrawInFrame(NSRect frame, NSView* control_view) {
  frame = NSMakeRect(NSMinX(frame) + kLeftPadding,
                     NSMinY(frame),
                     NSWidth(frame) - kLeftPadding,
                     NSHeight(frame));
  ButtonDecoration::DrawInFrame(frame, control_view);
}

bool SearchButtonDecoration::OnMousePressed(NSRect frame, NSPoint location) {
  owner_->GetOmniboxView()->model()->AcceptInput(
      owner_->GetWindowOpenDisposition(), false);
  return true;
}
