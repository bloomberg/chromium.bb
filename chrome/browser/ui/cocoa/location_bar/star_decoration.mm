// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/star_decoration.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#import "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#include "chrome/grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

// The info-bubble point should look like it points to the point
// between the star's lower tips.  The popup should be where the
// Omnibox popup ends up (2px below field).  Determined via Pixie.app
// magnification.
const CGFloat kStarPointYOffset = 2.0;

}  // namespace

StarDecoration::StarDecoration(CommandUpdater* command_updater)
    : command_updater_(command_updater) {
  SetVisible(true);
  SetStarred(false);
}

StarDecoration::~StarDecoration() {
}

void StarDecoration::SetStarred(bool starred) {
  starred_ = starred;
  const int image_id = starred ? IDR_STAR_LIT : IDR_STAR;
  const int tip_id = starred ? IDS_TOOLTIP_STARRED : IDS_TOOLTIP_STAR;
  SetImage(OmniboxViewMac::ImageForResource(image_id));
  tooltip_.reset([l10n_util::GetNSStringWithFixup(tip_id) retain]);
}

NSPoint StarDecoration::GetBubblePointInFrame(NSRect frame) {
  const NSRect draw_frame = GetDrawRectInFrame(frame);
  return NSMakePoint(NSMidX(draw_frame),
                     NSMaxY(draw_frame) - kStarPointYOffset);
}

bool StarDecoration::AcceptsMousePress() {
  return true;
}

bool StarDecoration::OnMousePressed(NSRect frame, NSPoint location) {
  command_updater_->ExecuteCommand(IDC_BOOKMARK_PAGE);
  return true;
}

NSString* StarDecoration::GetToolTip() {
  return tooltip_.get();
}
