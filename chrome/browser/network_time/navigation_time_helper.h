// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NETWORK_TIME_NAVIGATION_TIME_HELPER_H_
#define CHROME_BROWSER_NETWORK_TIME_NAVIGATION_TIME_HELPER_H_

#include <map>

#include "base/time/time.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

// NavigationTimeHelper returns the navigation time in network time for
// given navigation entry.
class NavigationTimeHelper
    : public content::NotificationObserver,
      public content::WebContentsUserData<NavigationTimeHelper> {
 public:
  virtual ~NavigationTimeHelper();

  base::Time GetNavigationTime(const content::NavigationEntry* entry);

 protected:
  // Tests only.
  NavigationTimeHelper();

  // Return network time for given |local_time|.
  virtual base::Time GetNetworkTime(base::Time local_time);

 private:
  explicit NavigationTimeHelper(content::WebContents* web_contents);
  friend class content::WebContentsUserData<NavigationTimeHelper>;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  content::NotificationRegistrar registrar_;

  content::WebContents* web_contents_;

  // Map from navigation entries to the navigation times calculated for them.
  struct NavigationTimeInfo {
    NavigationTimeInfo(base::Time local_time, base::Time network_time)
        : local_time(local_time), network_time(network_time) {}
    base::Time local_time;
    base::Time network_time;
  };
  typedef std::map<const void*, NavigationTimeInfo> NavigationTimeCache;
  NavigationTimeCache time_cache_;

  DISALLOW_COPY_AND_ASSIGN(NavigationTimeHelper);
};

#endif  // CHROME_BROWSER_NETWORK_TIME_NAVIGATION_TIME_HELPER_H_
