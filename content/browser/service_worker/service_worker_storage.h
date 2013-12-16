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
class ServiceWorkerRegisterJob;

// This class provides an interface to load registration data and
// instantiate ServiceWorkerRegistration objects. Any asynchronous
// operations are run through instances of ServiceWorkerRegisterJob.
class CONTENT_EXPORT ServiceWorkerStorage {
 public:
  ServiceWorkerStorage(const base::FilePath& path,
                       quota::QuotaManagerProxy* quota_manager_proxy);
  ~ServiceWorkerStorage();

  typedef base::Callback<void(ServiceWorkerRegistrationStatus status,
                              const scoped_refptr<ServiceWorkerRegistration>&
                                  registration)> RegistrationCallback;
  typedef base::Callback<
      void(ServiceWorkerRegistrationStatus status)> UnregistrationCallback;

  // `found` is only valid if status == REGISTRATION_OK.
  typedef base::Callback<void(bool found,
                              ServiceWorkerRegistrationStatus status,
                              const scoped_refptr<ServiceWorkerRegistration>&
                                  registration)> FindRegistrationCallback;

  void FindRegistrationForDocument(const GURL& document_url,
                                   const FindRegistrationCallback& callback);
  void FindRegistrationForPattern(const GURL& pattern,
                                  const FindRegistrationCallback& callback);

  void Register(const GURL& pattern,
                const GURL& script_url,
                const RegistrationCallback& callback);

  void Unregister(const GURL& pattern, const UnregistrationCallback& callback);

 private:
  friend class ServiceWorkerRegisterJob;
  FRIEND_TEST_ALL_PREFIXES(ServiceWorkerStorageTest, PatternMatches);

  typedef std::map<GURL, scoped_refptr<ServiceWorkerRegistration> >
      PatternToRegistrationMap;
  typedef ScopedVector<ServiceWorkerRegisterJob> RegistrationJobList;

  // TODO(alecflett): These are temporary internal methods providing
  // synchronous in-memory registration. Eventually these will be
  // replaced by asynchronous methods that persist registration to disk.
  scoped_refptr<ServiceWorkerRegistration> RegisterInternal(
      const GURL& pattern,
      const GURL& script_url);
  void UnregisterInternal(const GURL& pattern);
  static bool PatternMatches(const GURL& pattern, const GURL& script_url);

  // Jobs are removed whenever they are finished or canceled.
  void EraseJob(ServiceWorkerRegisterJob* job);

  // Called at ServiceWorkerRegisterJob completion.
  void RegisterComplete(const RegistrationCallback& callback,
                        ServiceWorkerRegisterJob* job,
                        ServiceWorkerRegistrationStatus status,
                        ServiceWorkerRegistration* registration);

  // Called at ServiceWorkerRegisterJob completion.
  void UnregisterComplete(const UnregistrationCallback& callback,
                          ServiceWorkerRegisterJob* job,
                          ServiceWorkerRegistrationStatus status,
                          ServiceWorkerRegistration* registration);

  // This is the in-memory registration. Eventually the registration will be
  // persisted to disk.
  // A list of currently running jobs. This is a temporary structure until we
  // start managing overlapping registrations explicitly.
  RegistrationJobList registration_jobs_;

  // in-memory map, to eventually be replaced with persistence
  PatternToRegistrationMap registration_by_pattern_;
  scoped_refptr<quota::QuotaManagerProxy> quota_manager_proxy_;
  base::FilePath path_;
  base::WeakPtrFactory<ServiceWorkerStorage> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerStorage);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_STORAGE_H_
