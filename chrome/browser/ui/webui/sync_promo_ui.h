// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SYNC_PROMO_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SYNC_PROMO_UI_H_
#pragma once

#include "chrome/browser/ui/webui/chrome_web_ui.h"

class Profile;
class PrefService;

// The Web UI handler for chrome://syncpromo.
class SyncPromoUI : public ChromeWebUI {
 public:
  // Constructs a SyncPromoUI. |contents| is the TabContents that this WebUI is
  // associated with. |contents| may not be NULL.
  explicit SyncPromoUI(TabContents* contents);

  // Returns true if the sync promo should be visible.
  // |profile| is the profile of the tab the promo would be shown on.
  static bool ShouldShowSyncPromo(Profile* profile);

  // Returns true if we should show the sync promo at startup.
  static bool ShouldShowSyncPromoAtStartup(Profile* profile,
                                           bool is_new_profile);

  // Called when the sync promo has been shown so that we can keep track
  // of the number of times we've displayed it.
  static void DidShowSyncPromoAtStartup(Profile* profile);

  // Returns true if the user has previously skipped the sync promo.
  static bool HasUserSkippedSyncPromo(Profile* profile);

  // Registers the fact that the user has skipped the sync promo.
  static void SetUserSkippedSyncPromo(Profile* profile);

  // Registers the preferences the Sync Promo UI needs.
  static void RegisterUserPrefs(PrefService* prefs);

  // Returns the sync promo URL wth the given arguments in the query.
  // |next_page| is the URL to navigate to when the user completes or skips the
  // promo. If an empty URL is given then the promo will navigate to the NTP.
  // If |show_title| is true then the promo title is made visible.
  static GURL GetSyncPromoURL(const GURL& next_page, bool show_title);

  // Gets the is launch page value from the query portion of the sync promo URL.
  static bool GetIsLaunchPageForSyncPromoURL(const GURL& url);

  // Gets the next page URL from the query portion of the sync promo URL.
  static GURL GetNextPageURLForSyncPromoURL(const GURL& url);

  // Returns true if the sync promo page was ever shown at startup.
  static bool UserHasSeenSyncPromoAtStartup(Profile* profile);

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncPromoUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SYNC_PROMO_UI_H_
