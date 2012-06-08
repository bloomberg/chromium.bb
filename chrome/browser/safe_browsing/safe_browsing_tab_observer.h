// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_TAB_OBSERVER_H_
#define CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_TAB_OBSERVER_H_
#pragma once

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "content/public/browser/notification_observer.h"

class TabContents;

namespace safe_browsing {

class ClientSideDetectionHost;

// Per-tab class to handle safe-browsing functionality.
class SafeBrowsingTabObserver : public content::NotificationObserver {
 public:
  explicit SafeBrowsingTabObserver(TabContents* tab_contents);
  virtual ~SafeBrowsingTabObserver();

 private:
  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Internal helpers ----------------------------------------------------------

  // Create or destroy SafebrowsingDetectionHost as needed if the user's
  // safe browsing preference has changed.
  void UpdateSafebrowsingDetectionHost();

  // Handles IPCs.
  scoped_ptr<ClientSideDetectionHost> safebrowsing_detection_host_;

  // Our owning TabContents.
  TabContents* tab_contents_;

  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingTabObserver);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SAFE_BROWSING_TAB_OBSERVER_H_
