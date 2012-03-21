// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_GOOGLE_SEARCH_COUNTER_H_
#define CHROME_BROWSER_GOOGLE_GOOGLE_SEARCH_COUNTER_H_

#include "base/memory/singleton.h"
#include "chrome/browser/google/google_search_metrics.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

// A listener for counting Google searches from various search access points. No
// actual search query content is observed. See GoogleSearchMetrics for more
// details about these access points.
class GoogleSearchCounter : content::NotificationObserver {
 public:
  // Initialize the global instance.
  static void RegisterForNotifications();

  // Return the singleton instance of GoogleSearchCounter.
  static GoogleSearchCounter* GetInstance();

 private:
  friend struct DefaultSingletonTraits<GoogleSearchCounter>;
  friend class GoogleSearchCounterTest;

  GoogleSearchCounter();
  virtual ~GoogleSearchCounter();

  void ProcessCommittedEntry(const content::NotificationSource& source,
                             const content::NotificationDetails& details);

  // Replace the internal metrics object with a dummy or a mock. This instance
  // takes ownership of |search_metrics|.
  void SetSearchMetricsForTesting(GoogleSearchMetrics* search_metrics);

  // Register this counter for all notifications we care about.
  void RegisterForNotificationsInternal();

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  content::NotificationRegistrar registrar_;
  scoped_ptr<GoogleSearchMetrics> search_metrics_;

  DISALLOW_COPY_AND_ASSIGN(GoogleSearchCounter);
};

#endif  // CHROME_BROWSER_GOOGLE_GOOGLE_SEARCH_COUNTER_H_
