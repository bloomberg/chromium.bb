// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/manage_passwords_decoration.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/cocoa/omnibox/omnibox_view_mac.h"
#include "chrome/browser/ui/cocoa/passwords/manage_passwords_bubble_cocoa.h"
#include "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"
#include "ui/base/l10n/l10n_util_mac.h"

// ManagePasswordsIconCocoa

ManagePasswordsIconCocoa::ManagePasswordsIconCocoa(
    ManagePasswordsDecoration* decoration)
    : decoration_(decoration) {
}

ManagePasswordsIconCocoa::~ManagePasswordsIconCocoa() {
}

void ManagePasswordsIconCocoa::UpdateVisibleUI() {
  decoration_->UpdateVisibleUI();
}

// ManagePasswordsDecoration

ManagePasswordsDecoration::ManagePasswordsDecoration(
    CommandUpdater* command_updater,
    LocationBarViewMac* location_bar)
    : command_updater_(command_updater),
      location_bar_(location_bar),
      icon_(new ManagePasswordsIconCocoa(this)) {
  UpdateUIState();
}

ManagePasswordsDecoration::~ManagePasswordsDecoration() {
  if (ManagePasswordsBubbleCocoa::instance())
    ManagePasswordsBubbleCocoa::instance()->SetIcon(NULL);
}

NSPoint ManagePasswordsDecoration::GetBubblePointInFrame(NSRect frame) {
  const NSRect draw_frame = GetDrawRectInFrame(frame);
  return NSMakePoint(NSMidX(draw_frame), NSMaxY(draw_frame));
}

bool ManagePasswordsDecoration::AcceptsMousePress() {
  return true;
}

bool ManagePasswordsDecoration::OnMousePressed(NSRect frame, NSPoint location) {
  bool result = ImageDecoration::OnMousePressed(frame, location);
  if (ManagePasswordsBubbleCocoa::instance())
    ManagePasswordsBubbleCocoa::instance()->Close();
  else
    command_updater_->ExecuteCommand(IDC_MANAGE_PASSWORDS_FOR_PAGE);
  return result;
}

NSString* ManagePasswordsDecoration::GetToolTip() {
  return icon_->tooltip_text_id()
             ? l10n_util::GetNSStringWithFixup(icon_->tooltip_text_id())
             : nil;
}

void ManagePasswordsDecoration::OnChange() {
  // |location_bar_| can be NULL in tests.
  if (location_bar_)
    location_bar_->OnDecorationsChanged();
}

void ManagePasswordsDecoration::UpdateUIState() {
  if (icon_->state() == password_manager::ui::INACTIVE_STATE) {
    SetVisible(false);
    SetImage(nil);
    if (ManagePasswordsBubbleCocoa::instance())
      ManagePasswordsBubbleCocoa::instance()->Close();
    return;
  }
  SetVisible(true);
  SetImage(OmniboxViewMac::ImageForResource(icon_->icon_id()));
}

void ManagePasswordsDecoration::UpdateVisibleUI() {
  UpdateUIState();
  OnChange();
}
