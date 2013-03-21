// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ALTERNATE_ERROR_TAB_OBSERVER_H_
#define CHROME_BROWSER_UI_ALTERNATE_ERROR_TAB_OBSERVER_H_

#include "base/prefs/pref_change_registrar.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

class PrefRegistrySyncable;
class Profile;

// Per-tab class to implement alternate error page functionality.
class AlternateErrorPageTabObserver
    : public content::WebContentsObserver,
      public content::NotificationObserver,
      public content::WebContentsUserData<AlternateErrorPageTabObserver> {
 public:
  virtual ~AlternateErrorPageTabObserver();

  static void RegisterUserPrefs(PrefRegistrySyncable* registry);

 private:
  explicit AlternateErrorPageTabObserver(content::WebContents* web_contents);
  friend class content::WebContentsUserData<AlternateErrorPageTabObserver>;

  // content::WebContentsObserver overrides:
  virtual void RenderViewCreated(
      content::RenderViewHost* render_view_host) OVERRIDE;

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Internal helpers ----------------------------------------------------------

  // Returns the server that can provide alternate error pages.  If the returned
  // URL is empty, the default error page built into WebKit will be used.
  GURL GetAlternateErrorPageURL() const;

  void OnAlternateErrorPagesEnabledChanged();

  // Send the alternate error page URL to the renderer.
  void UpdateAlternateErrorPageURL(content::RenderViewHost* rvh);

  Profile* profile_;
  content::NotificationRegistrar registrar_;
  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(AlternateErrorPageTabObserver);
};

#endif  // CHROME_BROWSER_UI_ALTERNATE_ERROR_TAB_OBSERVER_H_
