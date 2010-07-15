// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/cocoa/location_bar/star_decoration.h"

#include "app/l10n_util_mac.h"
#include "chrome/app/chrome_dll_resource.h"
#import "chrome/browser/autocomplete/autocomplete_edit_view_mac.h"
#include "chrome/browser/command_updater.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

namespace {

// The info-bubble point should look like it points to the point
// between the star's lower tips.  Determined via Pixie.app
// magnification.
const CGFloat kStarPointYOffset = 4.0;

}  // namespace

StarDecoration::StarDecoration(CommandUpdater* command_updater)
    : command_updater_(command_updater) {
  SetVisible(true);
  SetStarred(false);
}

StarDecoration::~StarDecoration() {
}

void StarDecoration::SetStarred(bool starred) {
  const int image_id = starred ? IDR_OMNIBOX_STAR_LIT : IDR_OMNIBOX_STAR;
  const int tip_id = starred ? IDS_TOOLTIP_STARRED : IDS_TOOLTIP_STAR;
  SetImage(AutocompleteEditViewMac::ImageForResource(image_id));
  tooltip_.reset([l10n_util::GetNSStringWithFixup(tip_id) retain]);
}

NSPoint StarDecoration::GetBubblePointInFrame(NSRect frame) {
  const NSRect draw_frame = GetDrawRectInFrame(frame);
  return NSMakePoint(NSMidX(draw_frame),
                     NSMaxY(draw_frame) - kStarPointYOffset);
}

bool StarDecoration::OnMousePressed(NSRect frame) {
  command_updater_->ExecuteCommand(IDC_BOOKMARK_PAGE);
  return true;
}

NSString* StarDecoration::GetToolTip() {
  return tooltip_.get();
}
