// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_OMNIBOX_WATCHER_H_
#define CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_OMNIBOX_WATCHER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/safe_browsing/incident_reporting/add_incident_callback.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace safe_browsing {

// Observes all URLs opened via the omnibox, adding an incident to the
// incident reporting service for those that are typed impossibly quickly.
class OmniboxWatcher : public content::NotificationObserver {
 public:
  OmniboxWatcher(Profile* profile, const AddIncidentCallback& callback);
  ~OmniboxWatcher() override;

 private:
  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // The place to send a callback when we witness a suspicious omnibox
  // navigation.
  AddIncidentCallback incident_callback_;

  // Registrar for receiving Omnibox event notifications.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(OmniboxWatcher);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_INCIDENT_REPORTING_OMNIBOX_WATCHER_H_
