// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notifications_prefs_cache.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/common/pref_service.h"
#include "webkit/api/public/WebNotificationPresenter.h"

NotificationsPrefsCache::NotificationsPrefsCache(
    const ListValue* allowed, const ListValue* denied) {
  ListValue::const_iterator i;
  std::wstring origin;
  if (allowed) {
    for (i = allowed->begin(); i != allowed->end(); ++i) {
      (*i)->GetAsString(&origin);
      allowed_origins_.insert(GURL(WideToUTF8(origin)));
    }
  }
  if (denied) {
    for (i = denied->begin(); i != denied->end(); ++i) {
      (*i)->GetAsString(&origin);
      denied_origins_.insert(GURL(WideToUTF8(origin)));
    }
  }
}

void NotificationsPrefsCache::CacheAllowedOrigin(
    const GURL& origin) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  std::set<GURL>::iterator iter;
  allowed_origins_.insert(origin);
  if ((iter = denied_origins_.find(origin)) != denied_origins_.end())
    denied_origins_.erase(iter);
}

void NotificationsPrefsCache::CacheDeniedOrigin(
    const GURL& origin) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  std::set<GURL>::iterator iter;
  denied_origins_.insert(origin);
  if ((iter = allowed_origins_.find(origin)) != allowed_origins_.end())
    allowed_origins_.erase(iter);
}

int NotificationsPrefsCache::HasPermission(const GURL& origin) {
  if (IsOriginAllowed(origin))
    return WebKit::WebNotificationPresenter::PermissionAllowed;
  if (IsOriginDenied(origin))
    return WebKit::WebNotificationPresenter::PermissionDenied;
  return WebKit::WebNotificationPresenter::PermissionNotAllowed;
}

bool NotificationsPrefsCache::IsOriginAllowed(
    const GURL& origin) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  return (allowed_origins_.find(origin) != allowed_origins_.end());
}

bool NotificationsPrefsCache::IsOriginDenied(
    const GURL& origin) {
  DCHECK(ChromeThread::CurrentlyOn(ChromeThread::IO));
  return (denied_origins_.find(origin) != denied_origins_.end());
}
