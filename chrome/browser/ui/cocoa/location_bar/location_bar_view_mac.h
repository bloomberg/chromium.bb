// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_LOCATION_BAR_LOCATION_BAR_VIEW_MAC_H_
#define CHROME_BROWSER_UI_COCOA_LOCATION_BAR_LOCATION_BAR_VIEW_MAC_H_

#import <Cocoa/Cocoa.h>
#include <stddef.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/chrome_omnibox_edit_controller.h"
#include "components/prefs/pref_member.h"
#include "components/security_state/core/security_state.h"

class CommandUpdater;
class Profile;

// A C++ bridge class that represents the location bar UI element to
// the portable code.  Wires up an OmniboxViewMac instance to
// the location bar text field, which handles most of the work.

class LocationBarViewMac : public LocationBar,
                           public LocationBarTesting,
                           public ChromeOmniboxEditController {
 public:
  LocationBarViewMac(CommandUpdater* command_updater,
                     Profile* profile,
                     Browser* browser);
  ~LocationBarViewMac() override;

  // Overridden from LocationBar:
  GURL GetDestinationURL() const override;
  WindowOpenDisposition GetWindowOpenDisposition() const override;
  ui::PageTransition GetPageTransition() const override;
  base::TimeTicks GetMatchSelectionTimestamp() const override;
  void AcceptInput() override;
  void AcceptInput(base::TimeTicks match_selection_timestamp) override;
  void FocusLocation(bool select_all) override;
  void FocusSearch() override;
  void UpdateContentSettingsIcons() override;
  void UpdateManagePasswordsIconAndBubble() override;
  void UpdateSaveCreditCardIcon() override;
  void UpdateLocalCardMigrationIcon() override;
  void UpdateBookmarkStarVisibility() override;
  void UpdateLocationBarVisibility(bool visible, bool animate) override;
  void SaveStateToContents(content::WebContents* contents) override;
  void Revert() override;
  const OmniboxView* GetOmniboxView() const override;
  OmniboxView* GetOmniboxView() override;
  LocationBarTesting* GetLocationBarForTesting() override;

  // Overridden from LocationBarTesting:
  bool GetBookmarkStarVisibility() override;
  bool TestContentSettingImagePressed(size_t index) override;
  bool IsContentSettingBubbleShowing(size_t index) override;

  // ChromeOmniboxEditController:
  ToolbarModel* GetToolbarModel() override;
  const ToolbarModel* GetToolbarModel() const override;

 private:
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(LocationBarViewMac);
};

#endif  // CHROME_BROWSER_UI_COCOA_LOCATION_BAR_LOCATION_BAR_VIEW_MAC_H_
