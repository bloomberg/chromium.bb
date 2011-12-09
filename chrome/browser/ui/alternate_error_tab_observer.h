// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ALTERNATE_ERROR_TAB_OBSERVER_H_
#define CHROME_BROWSER_UI_ALTERNATE_ERROR_TAB_OBSERVER_H_
#pragma once

#include "chrome/browser/prefs/pref_change_registrar.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "content/public/browser/notification_registrar.h"

class TabContentsWrapper;

// Per-tab class to implement alternate error page functionality.
class AlternateErrorPageTabObserver : public TabContentsObserver,
                                      public content::NotificationObserver {
 public:
  explicit AlternateErrorPageTabObserver(TabContentsWrapper* wrapper);
  virtual ~AlternateErrorPageTabObserver();

  static void RegisterUserPrefs(PrefService* prefs);

 private:
  // TabContentsObserver overrides:
  virtual void RenderViewCreated(RenderViewHost* render_view_host) OVERRIDE;

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Internal helpers ----------------------------------------------------------

  // Returns the server that can provide alternate error pages.  If the returned
  // URL is empty, the default error page built into WebKit will be used.
  GURL GetAlternateErrorPageURL() const;

  // Send the alternate error page URL to the renderer.
  void UpdateAlternateErrorPageURL(RenderViewHost* rvh);

  // Our owning TabContentsWrapper.
  TabContentsWrapper* wrapper_;

  content::NotificationRegistrar registrar_;
  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(AlternateErrorPageTabObserver);
};

#endif  // CHROME_BROWSER_UI_ALTERNATE_ERROR_TAB_OBSERVER_H_
