// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATIONS_PREFS_CACHE_H
#define CHROME_BROWSER_NOTIFICATIONS_NOTIFICATIONS_PREFS_CACHE_H

#include <set>

#include "base/ref_counted.h"
#include "googleurl/src/gurl.h"

class ListValue;

// Class which caches notification preferences.
// Construction occurs on the UI thread when the contents
// of the profile preferences are initially cached.  Once constructed
// this class should only be accessed on the IO thread.
class NotificationsPrefsCache :
    public base::RefCountedThreadSafe<NotificationsPrefsCache> {
 public:
  NotificationsPrefsCache(const ListValue* allowed, const ListValue* denied);

  // Checks to see if a given origin has permission to create desktop
  // notifications.  Returns a constant from WebNotificationPresenter
  // class.
  int HasPermission(const GURL& origin);

  // Updates the cache with a new origin allowed or denied.
  void CacheAllowedOrigin(const GURL& origin);
  void CacheDeniedOrigin(const GURL& origin);

 private:
  friend class base::RefCountedThreadSafe<NotificationsPrefsCache>;

  ~NotificationsPrefsCache() {}

  // Helper functions which read preferences.
  bool IsOriginAllowed(const GURL& origin);
  bool IsOriginDenied(const GURL& origin);

  // Storage of the actual preferences.
  std::set<GURL> allowed_origins_;
  std::set<GURL> denied_origins_;

  DISALLOW_COPY_AND_ASSIGN(NotificationsPrefsCache);
};

#endif  // #ifndef CHROME_BROWSER_NOTIFICATIONS_NOTIFICATIONS_PREFS_CACHE_H
