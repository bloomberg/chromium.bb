// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notifications_prefs_cache.h"

#include <string>

#include "base/string_util.h"
#include "base/values.h"
#include "base/utf_string_conversions.h"
#include "content/browser/browser_thread.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNotificationPresenter.h"

NotificationsPrefsCache::NotificationsPrefsCache()
        : default_content_setting_(CONTENT_SETTING_DEFAULT),
          is_initialized_(false) {
}

void NotificationsPrefsCache::CacheAllowedOrigin(
    const GURL& origin) {
  CheckThreadAccess();
  std::set<GURL>::iterator iter;
  allowed_origins_.insert(origin);
  if ((iter = denied_origins_.find(origin)) != denied_origins_.end())
    denied_origins_.erase(iter);
}

void NotificationsPrefsCache::CacheDeniedOrigin(
    const GURL& origin) {
  CheckThreadAccess();
  std::set<GURL>::iterator iter;
  denied_origins_.insert(origin);
  if ((iter = allowed_origins_.find(origin)) != allowed_origins_.end())
    allowed_origins_.erase(iter);
}

void NotificationsPrefsCache::SetCacheAllowedOrigins(
    const std::vector<GURL>& allowed) {
  allowed_origins_.clear();
  allowed_origins_.insert(allowed.begin(), allowed.end());
}

void NotificationsPrefsCache::SetCacheDeniedOrigins(
    const std::vector<GURL>& denied) {
  denied_origins_.clear();
  denied_origins_.insert(denied.begin(), denied.end());
}

void NotificationsPrefsCache::SetCacheDefaultContentSetting(
    ContentSetting setting) {
  default_content_setting_ = setting;
}

// static
void NotificationsPrefsCache::ListValueToGurlVector(
    const ListValue& origin_list,
    std::vector<GURL>* origin_vector) {
  ListValue::const_iterator i;
  std::string origin;
  for (i = origin_list.begin(); i != origin_list.end(); ++i) {
    (*i)->GetAsString(&origin);
    origin_vector->push_back(GURL(origin));
  }
}

int NotificationsPrefsCache::HasPermission(const GURL& origin) {
  if (IsOriginAllowed(origin))
    return WebKit::WebNotificationPresenter::PermissionAllowed;
  if (IsOriginDenied(origin))
    return WebKit::WebNotificationPresenter::PermissionDenied;
  switch (default_content_setting_) {
    case CONTENT_SETTING_ALLOW:
      return WebKit::WebNotificationPresenter::PermissionAllowed;
    case CONTENT_SETTING_BLOCK:
      return WebKit::WebNotificationPresenter::PermissionDenied;
    case CONTENT_SETTING_ASK:
    case CONTENT_SETTING_DEFAULT:
    default:  // Make gcc happy.
      return WebKit::WebNotificationPresenter::PermissionNotAllowed;
  }
}

NotificationsPrefsCache::~NotificationsPrefsCache() {}

bool NotificationsPrefsCache::IsOriginAllowed(
    const GURL& origin) {
  CheckThreadAccess();
  return allowed_origins_.find(origin) != allowed_origins_.end();
}

bool NotificationsPrefsCache::IsOriginDenied(
    const GURL& origin) {
  CheckThreadAccess();
  return denied_origins_.find(origin) != denied_origins_.end();
}

void NotificationsPrefsCache::CheckThreadAccess() {
  if (is_initialized_) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  } else {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  }
}
