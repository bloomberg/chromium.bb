// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STORAGE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STORAGE_H_

#include <map>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_vector.h"
#include "content/browser/service_worker/service_worker_registration_status.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace quota {
class QuotaManagerProxy;
}

namespace content {

class ServiceWorkerRegistration;

// This class provides an interface to load registration data and
// instantiate ServiceWorkerRegistration objects.
class CONTENT_EXPORT ServiceWorkerStorage {
 public:
  ServiceWorkerStorage(const base::FilePath& path,
                       quota::QuotaManagerProxy* quota_manager_proxy);
  ~ServiceWorkerStorage();

  // `found` is only valid if status == SERVICE_WORKER_OK.
  typedef base::Callback<void(bool found,
                              ServiceWorkerStatusCode status,
                              const scoped_refptr<ServiceWorkerRegistration>&
                                  registration)> FindRegistrationCallback;

  void FindRegistrationForDocument(const GURL& document_url,
                                   const FindRegistrationCallback& callback);
  void FindRegistrationForPattern(const GURL& pattern,
                                  const FindRegistrationCallback& callback);

  // TODO(alecflett): These are temporary internal methods providing
  // synchronous in-memory registration. Eventually these will be
  // replaced by asynchronous methods that persist registration to disk.
  scoped_refptr<ServiceWorkerRegistration> RegisterInternal(
      const GURL& pattern,
      const GURL& script_url);
  void UnregisterInternal(const GURL& pattern);

 private:
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerStorageTest, PatternMatches);

  typedef std::map<GURL, scoped_refptr<ServiceWorkerRegistration> >
      PatternToRegistrationMap;

  static bool PatternMatches(const GURL& pattern, const GURL& script_url);

  // This is the in-memory registration. Eventually the registration will be
  // persisted to disk.
  PatternToRegistrationMap registration_by_pattern_;

  scoped_refptr<quota::QuotaManagerProxy> quota_manager_proxy_;
  base::FilePath path_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerStorage);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STORAGE_H_
