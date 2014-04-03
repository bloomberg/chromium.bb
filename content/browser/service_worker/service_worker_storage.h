// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STORAGE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STORAGE_H_

#include <map>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_vector.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "url/gurl.h"

namespace quota {
class QuotaManagerProxy;
}

namespace content {

class ServiceWorkerRegistration;
class ServiceWorkerRegistrationInfo;

// This class provides an interface to load registration data and
// instantiate ServiceWorkerRegistration objects.
class CONTENT_EXPORT ServiceWorkerStorage {
 public:
  typedef base::Callback<void(ServiceWorkerStatusCode status)> StatusCallback;
  typedef base::Callback<void(ServiceWorkerStatusCode status,
                              const scoped_refptr<ServiceWorkerRegistration>&
                                  registration)> FindRegistrationCallback;

  ServiceWorkerStorage(const base::FilePath& path,
                       quota::QuotaManagerProxy* quota_manager_proxy);
  ~ServiceWorkerStorage();

  // Finds registration for |document_url| or |pattern| or |registration_id|.
  // Returns SERVICE_WORKER_OK with non-null registration if registration
  // is found, or returns SERVICE_WORKER_ERROR_NOT_FOUND if no matching
  // registration is found.
  void FindRegistrationForDocument(const GURL& document_url,
                                   const FindRegistrationCallback& callback);
  void FindRegistrationForPattern(const GURL& pattern,
                                  const FindRegistrationCallback& callback);
  void FindRegistrationForId(int64 registration_id,
                             const FindRegistrationCallback& callback);

  typedef base::Callback<
      void(const std::vector<ServiceWorkerRegistrationInfo>& registrations)>
      GetAllRegistrationInfosCallback;
  void GetAllRegistrations(const GetAllRegistrationInfosCallback& callback);

  // Stores |registration|. Returns SERVICE_WORKER_ERROR_EXISTS if
  // conflicting registration (which has different script_url) is
  // already registered for the |registration|->pattern().
  void StoreRegistration(ServiceWorkerRegistration* registration,
                         const StatusCallback& callback);

  // Deletes |registration|. This may return SERVICE_WORKER_ERROR_NOT_FOUND
  // if no matching registration is found.
  void DeleteRegistration(const GURL& pattern,
                          const StatusCallback& callback);

  // Returns new IDs which are guaranteed to be unique in the storage.
  int64 NewRegistrationId();
  int64 NewVersionId();

 private:
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerStorageTest, PatternMatches);

  typedef std::map<GURL, scoped_refptr<ServiceWorkerRegistration> >
      PatternToRegistrationMap;

  static bool PatternMatches(const GURL& pattern, const GURL& script_url);

  // This is the in-memory registration. Eventually the registration will be
  // persisted to disk.
  PatternToRegistrationMap registration_by_pattern_;

  int last_registration_id_;
  int last_version_id_;

  scoped_refptr<quota::QuotaManagerProxy> quota_manager_proxy_;
  base::FilePath path_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerStorage);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STORAGE_H_
