// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_SEARCH_GEOLOCATION_DISCLOSURE_TAB_HELPER_H_
#define CHROME_BROWSER_ANDROID_SEARCH_GEOLOCATION_DISCLOSURE_TAB_HELPER_H_

#include "base/macros.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

// Tab helper to show the search geolocation disclosure.
class SearchGeolocationDisclosureTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<
          SearchGeolocationDisclosureTabHelper> {
 public:
  ~SearchGeolocationDisclosureTabHelper() override;

  // content::WebContentsObserver overrides.
  void NavigationEntryCommitted(
      const content::LoadCommittedDetails& load_details) override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  explicit SearchGeolocationDisclosureTabHelper(content::WebContents* contents);
  friend class content::WebContentsUserData<
      SearchGeolocationDisclosureTabHelper>;

  void MaybeShowDefaultSearchGeolocationDisclosure(const GURL& gurl);

  // Record metrics, once per client, of the permission state before and after
  // the disclosure has been shown.
  void RecordPreDisclosureMetrics(const GURL& gurl);
  void RecordPostDisclosureMetrics(const GURL& gurl);
  Profile* GetProfile();

  bool consistent_geolocation_enabled_;

  DISALLOW_COPY_AND_ASSIGN(SearchGeolocationDisclosureTabHelper);
};

#endif  // CHROME_BROWSER_ANDROID_SEARCH_GEOLOCATION_DISCLOSURE_TAB_HELPER_H_
