// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_APPCACHE_CHROME_APPCACHE_SERVICE_H_
#define CONTENT_BROWSER_APPCACHE_CHROME_APPCACHE_SERVICE_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "webkit/appcache/appcache_policy.h"
#include "webkit/appcache/appcache_service.h"
#include "webkit/quota/special_storage_policy.h"

class FilePath;

namespace content {
class ResourceContext;
}

// An AppCacheService subclass used by the chrome. There is an instance
// associated with each BrowserContext. This derivation adds refcounting
// semantics since a browser context has multiple URLRequestContexts which refer
// to the same object, and those URLRequestContexts are refcounted independently
// of the owning browser context.
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
  explicit ChromeAppCacheService(quota::QuotaManagerProxy* proxy);

  void InitializeOnIOThread(
      const FilePath& cache_path,  // may be empty to use in-memory structures
      const content::ResourceContext* resource_context,
      scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy);

 private:
  friend class BrowserThread;
  friend class DeleteTask<ChromeAppCacheService>;

  virtual ~ChromeAppCacheService();

  // AppCachePolicy overrides
  virtual bool CanLoadAppCache(const GURL& manifest_url,
                               const GURL& first_party);
  virtual bool CanCreateAppCache(const GURL& manifest_url,
                                 const GURL& first_party);

  // NotificationObserver override
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  const content::ResourceContext* resource_context_;
  NotificationRegistrar registrar_;
  FilePath cache_path_;

  DISALLOW_COPY_AND_ASSIGN(ChromeAppCacheService);
};

#endif  // CONTENT_BROWSER_APPCACHE_CHROME_APPCACHE_SERVICE_H_
