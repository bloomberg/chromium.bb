// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/location_bar/location_bar_view_mac.h"

LocationBarViewMac::LocationBarViewMac(CommandUpdater* command_updater,
                                       Profile* profile,
                                       Browser* browser)
    : LocationBar(profile),
      ChromeOmniboxEditController(command_updater),
      browser_(browser) {}

LocationBarViewMac::~LocationBarViewMac() {
}

GURL LocationBarViewMac::GetDestinationURL() const {
  return destination_url();
}

WindowOpenDisposition LocationBarViewMac::GetWindowOpenDisposition() const {
  return disposition();
}

ui::PageTransition LocationBarViewMac::GetPageTransition() const {
  return transition();
}

base::TimeTicks LocationBarViewMac::GetMatchSelectionTimestamp() const {
  return match_selection_timestamp();
}

void LocationBarViewMac::AcceptInput() {
}

void LocationBarViewMac::AcceptInput(
    base::TimeTicks match_selection_timestamp) {
}

void LocationBarViewMac::FocusLocation(bool select_all) {
}

void LocationBarViewMac::FocusSearch() {
}

void LocationBarViewMac::UpdateContentSettingsIcons() {
}

void LocationBarViewMac::UpdateManagePasswordsIconAndBubble() {
}

void LocationBarViewMac::UpdateSaveCreditCardIcon() {
}

void LocationBarViewMac::UpdateLocalCardMigrationIcon() {
}

void LocationBarViewMac::UpdateBookmarkStarVisibility() {
}

void LocationBarViewMac::UpdateLocationBarVisibility(bool visible,
                                                     bool animate) {
}

void LocationBarViewMac::SaveStateToContents(content::WebContents* contents) {}

void LocationBarViewMac::Revert() {
}

const OmniboxView* LocationBarViewMac::GetOmniboxView() const {
  return nullptr;
}

OmniboxView* LocationBarViewMac::GetOmniboxView() {
  return nullptr;
}

LocationBarTesting* LocationBarViewMac::GetLocationBarForTesting() {
  return this;
}

bool LocationBarViewMac::GetBookmarkStarVisibility() {
  return false;
}

bool LocationBarViewMac::TestContentSettingImagePressed(size_t index) {
  return false;
}

bool LocationBarViewMac::IsContentSettingBubbleShowing(size_t index) {
  return false;
}

ToolbarModel* LocationBarViewMac::GetToolbarModel() {
  return browser_->toolbar_model();
}

const ToolbarModel* LocationBarViewMac::GetToolbarModel() const {
  return browser_->toolbar_model();
}
