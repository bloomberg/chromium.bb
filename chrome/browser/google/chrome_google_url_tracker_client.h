// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_CHROME_GOOGLE_URL_TRACKER_CLIENT_H_
#define CHROME_BROWSER_GOOGLE_CHROME_GOOGLE_URL_TRACKER_CLIENT_H_

#include "components/google/core/browser/google_url_tracker_client.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

class ChromeGoogleURLTrackerClient : public GoogleURLTrackerClient,
                                     public content::NotificationObserver {
 public:
  explicit ChromeGoogleURLTrackerClient(Profile* profile);
  virtual ~ChromeGoogleURLTrackerClient();

  // GoogleURLTrackerClient:
  virtual void SetListeningForNavigationStart(bool listen) override;
  virtual bool IsListeningForNavigationStart() override;
  virtual bool IsBackgroundNetworkingEnabled() override;
  virtual PrefService* GetPrefs() override;
  virtual net::URLRequestContextGetter* GetRequestContext() override;

 private:
  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) override;

  Profile* profile_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ChromeGoogleURLTrackerClient);
};

#endif  // CHROME_BROWSER_GOOGLE_CHROME_GOOGLE_URL_TRACKER_CLIENT_H_
