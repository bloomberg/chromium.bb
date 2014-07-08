// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_NAVIGATION_CORRECTION_TAB_OBSERVER_H_
#define CHROME_BROWSER_UI_NAVIGATION_CORRECTION_TAB_OBSERVER_H_

#include "base/prefs/pref_change_registrar.h"
#include "components/google/core/browser/google_url_tracker.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

// Per-tab class to implement navigation suggestion service functionality.
class NavigationCorrectionTabObserver
    : public content::WebContentsObserver,
      public content::WebContentsUserData<NavigationCorrectionTabObserver> {
 public:
  virtual ~NavigationCorrectionTabObserver();

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  explicit NavigationCorrectionTabObserver(content::WebContents* web_contents);
  friend class content::WebContentsUserData<NavigationCorrectionTabObserver>;

  // content::WebContentsObserver overrides:
  virtual void RenderViewCreated(
      content::RenderViewHost* render_view_host) OVERRIDE;

  // Internal helpers ----------------------------------------------------------

  // Callback that is called when the Google URL is updated.
  void OnGoogleURLUpdated();

  // Returns the URL for the correction service.  If the returned URL
  // is empty, the default error pages will be used.
  GURL GetNavigationCorrectionURL() const;

  // Called when navigation corrections are enabled or disabled.
  void OnEnabledChanged();

  // Updates the renderer's navigation correction service configuration.
  void UpdateNavigationCorrectionInfo(content::RenderViewHost* rvh);

  Profile* profile_;
  PrefChangeRegistrar pref_change_registrar_;
  scoped_ptr<GoogleURLTracker::Subscription> google_url_updated_subscription_;

  DISALLOW_COPY_AND_ASSIGN(NavigationCorrectionTabObserver);
};

#endif  // CHROME_BROWSER_UI_NAVIGATION_CORRECTION_TAB_OBSERVER_H_
