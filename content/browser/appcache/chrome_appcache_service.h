// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_CHROME_APPCACHE_SERVICE_H_
#define CONTENT_BROWSER_APPCACHE_CHROME_APPCACHE_SERVICE_H_
#pragma once

#include "base/ref_counted.h"
#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/common/notification_registrar.h"
#include "content/browser/browser_thread.h"
#include "webkit/appcache/appcache_policy.h"
#include "webkit/appcache/appcache_service.h"

class ChromeURLRequestContext;
class FilePath;

// An AppCacheService subclass used by the chrome. There is an instance
// associated with each Profile. This derivation adds refcounting semantics
// since a profile has multiple URLRequestContexts which refer to the same
// object, and those URLRequestContexts are refcounted independently of the
// owning profile.
//
// All methods, except the ctor, are expected to be called on
// the IO thread (unless specifically called out in doc comments).
class ChromeAppCacheService
    : public base::RefCountedThreadSafe<ChromeAppCacheService,
                                        BrowserThread::DeleteOnIOThread>,
      public appcache::AppCacheService,
      public appcache::AppCachePolicy,
      public NotificationObserver {
 public:
  ChromeAppCacheService();

  void InitializeOnIOThread(
      const FilePath& profile_path, bool is_incognito,
      scoped_refptr<HostContentSettingsMap> content_settings_map,
      bool clear_local_state_on_exit);

  // Helpers used by the extension service to grant and revoke
  // unlimited storage to app extensions.
  void SetOriginQuotaInMemory(const GURL& origin, int64 quota);
  void ResetOriginQuotaInMemory(const GURL& origin);

  void SetClearLocalStateOnExit(bool clear_local_state);

 private:
  friend class BrowserThread;
  friend class DeleteTask<ChromeAppCacheService>;

  virtual ~ChromeAppCacheService();

  // AppCachePolicy overrides
  virtual bool CanLoadAppCache(const GURL& manifest_url);
  virtual int CanCreateAppCache(const GURL& manifest_url,
                                net::CompletionCallback* callback);

  // NotificationObserver override
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  scoped_refptr<HostContentSettingsMap> host_contents_settings_map_;
  NotificationRegistrar registrar_;
  bool clear_local_state_on_exit_;
  FilePath cache_path_;

  DISALLOW_COPY_AND_ASSIGN(ChromeAppCacheService);
};

#endif  // CONTENT_BROWSER_APPCACHE_CHROME_APPCACHE_SERVICE_H_
