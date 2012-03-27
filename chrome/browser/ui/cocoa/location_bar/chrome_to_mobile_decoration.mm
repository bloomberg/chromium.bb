// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/chrome_to_mobile_decoration.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#import "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources_standard.h"
#include "ui/base/l10n/l10n_util_mac.h"

// An inset to anchor the bubble within the icon's built-in padding.
// The popup should be where the Omnibox popup ends up (2px below field).
// Matches the bookmark bubble and star settings.
const CGFloat kIconYInset = 2.0;

ChromeToMobileDecoration::ChromeToMobileDecoration(
    Profile* profile,
    CommandUpdater* command_updater)
    : profile_(profile),
      command_updater_(command_updater) {
  SetVisible(false);
  SetLit(false);
  tooltip_.reset([l10n_util::GetNSStringWithFixup(
      IDS_CHROME_TO_MOBILE_BUBBLE_TOOLTIP) retain]);
}

ChromeToMobileDecoration::~ChromeToMobileDecoration() {
}

NSPoint ChromeToMobileDecoration::GetBubblePointInFrame(NSRect frame) {
  const NSRect draw_frame = GetDrawRectInFrame(frame);
  return NSMakePoint(NSMidX(draw_frame), NSMaxY(draw_frame) - kIconYInset);
}

void ChromeToMobileDecoration::SetLit(bool lit) {
  ThemeService* service = ThemeServiceFactory::GetForProfile(profile_);
  SetImage(service->GetNSImageNamed(lit ? IDR_MOBILE_LIT : IDR_MOBILE, true));
}

bool ChromeToMobileDecoration::AcceptsMousePress() {
  return true;
}

bool ChromeToMobileDecoration::OnMousePressed(NSRect frame) {
  command_updater_->ExecuteCommand(IDC_CHROME_TO_MOBILE_PAGE);
  return true;
}

NSString* ChromeToMobileDecoration::GetToolTip() {
  return tooltip_.get();
}
