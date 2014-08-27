// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/translate_decoration.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#import "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#include "chrome/grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util_mac.h"

TranslateDecoration::TranslateDecoration(CommandUpdater* command_updater)
    : command_updater_(command_updater) {
  SetLit(false);
}

TranslateDecoration::~TranslateDecoration() {}

void TranslateDecoration::SetLit(bool on) {
  const int image_id = on ? IDR_TRANSLATE_ACTIVE : IDR_TRANSLATE;
  SetImage(OmniboxViewMac::ImageForResource(image_id));
}

NSPoint TranslateDecoration::GetBubblePointInFrame(NSRect frame) {
  const NSRect draw_frame = GetDrawRectInFrame(frame);
  return NSMakePoint(NSMidX(draw_frame), NSMaxY(draw_frame));
}

bool TranslateDecoration::AcceptsMousePress() {
  return true;
}

bool TranslateDecoration::OnMousePressed(NSRect frame, NSPoint location) {
  command_updater_->ExecuteCommand(IDC_TRANSLATE_PAGE);
  return true;
}

NSString* TranslateDecoration::GetToolTip() {
  return l10n_util::GetNSStringWithFixup(IDS_TOOLTIP_TRANSLATE);
}
