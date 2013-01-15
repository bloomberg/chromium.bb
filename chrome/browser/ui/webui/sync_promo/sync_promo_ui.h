// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SYNC_PROMO_SYNC_PROMO_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SYNC_PROMO_SYNC_PROMO_UI_H_

#include "content/public/browser/web_ui_controller.h"

class Profile;
class PrefServiceSyncable;

// The Web UI handler for chrome://signin.
class SyncPromoUI : public content::WebUIController {
 public:
  // Please keep this in sync with enums in sync_promo_trial.cc.
  enum Source {
    SOURCE_START_PAGE = 0, // This must be first.
    SOURCE_NTP_LINK,
    SOURCE_MENU,
    SOURCE_SETTINGS,
    SOURCE_UNKNOWN, // This must be last.
  };

  // Constructs a SyncPromoUI.
  explicit SyncPromoUI(content::WebUI* web_ui);

  // Returns true if the sync promo should be visible.
  // |profile| is the profile of the tab the promo would be shown on.
  static bool ShouldShowSyncPromo(Profile* profile);

  // Returns true if we should show the sync promo at startup.
  static bool ShouldShowSyncPromoAtStartup(Profile* profile,
                                           bool is_new_profile);

  // Called when the sync promo has been shown so that we can keep track
  // of the number of times we've displayed it.
  static void DidShowSyncPromoAtStartup(Profile* profile);

  // Returns true if a user has seen the sync promo at startup previously.
  static bool HasShownPromoAtStartup(Profile* profile);

  // Returns true if the user has previously skipped the sync promo.
  static bool HasUserSkippedSyncPromo(Profile* profile);

  // Registers the fact that the user has skipped the sync promo.
  static void SetUserSkippedSyncPromo(Profile* profile);

  // Registers the preferences the Sync Promo UI needs.
  static void RegisterUserPrefs(PrefServiceSyncable* prefs);

  // Returns the sync promo URL wth the given arguments in the query.
  // |next_page| is the URL to navigate to when the user completes or skips the
  // promo. If an empty URL is given then the promo will navigate to the NTP.
  // |source| identifies from where the sync promo is being called, and is used
  // to record sync promo UMA stats in the context of the source.
  // |auto_close| whether to close the sync promo automatically when done.
  static GURL GetSyncPromoURL(
      const GURL& next_page, Source source, bool auto_close);

  // Gets the next page URL from the query portion of the sync promo URL.
  static GURL GetNextPageURLForSyncPromoURL(const GURL& url);

  // Gets the source from the query portion of the sync promo URL.
  // The source identifies from where the sync promo was opened.
  static Source GetSourceForSyncPromoURL(const GURL& url);

  // Returns whether the given sync URL contains auto_close parameter.
  static bool GetAutoCloseForSyncPromoURL(const GURL& url);

  // Returns true if chrome should use the web-based sign in flow, false if
  // chrome should use the ClientLogin flow.  This function will return true
  // only for platforms where |ENABLE_ONE_CLICK_SIGNIN| is defined.
  static bool UseWebBasedSigninFlow();

  // Forces UseWebBasedSigninFlow() to return true when set; used in tests only.
  static void ForceWebBasedSigninFlowForTesting(bool force);

 private:
  DISALLOW_COPY_AND_ASSIGN(SyncPromoUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SYNC_PROMO_SYNC_PROMO_UI_H_
