// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_GOOGLE_SEARCH_COUNTER_ANDROID_H_
#define CHROME_BROWSER_GOOGLE_GOOGLE_SEARCH_COUNTER_ANDROID_H_

#include "base/macros.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

// A listener for counting Google searches in Android Chrome from various search
// access points. No actual search query content is observed. See
// GoogleSearchMetrics for more details about these access points.
class GoogleSearchCounterAndroid : content::NotificationObserver {
 public:
  explicit GoogleSearchCounterAndroid(Profile* profile);
  virtual ~GoogleSearchCounterAndroid();

 private:
  friend class GoogleSearchCounterAndroidTest;

  void ProcessCommittedEntry(const content::NotificationSource& source,
                             const content::NotificationDetails& details);

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  Profile* profile_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(GoogleSearchCounterAndroid);
};

#endif  // CHROME_BROWSER_GOOGLE_GOOGLE_SEARCH_COUNTER_ANDROID_H_
