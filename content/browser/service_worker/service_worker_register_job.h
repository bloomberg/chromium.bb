// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTER_JOB_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTER_JOB_H_

#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_registration_status.h"
#include "content/browser/service_worker/service_worker_storage.h"

namespace content {

// A ServiceWorkerRegisterJob lives only for the lifetime of a single
// registration or unregistration.
class ServiceWorkerRegisterJob {
 public:
  typedef base::Callback<void(
      ServiceWorkerRegisterJob* job,
      ServiceWorkerRegistrationStatus status,
      ServiceWorkerRegistration* registration)> RegistrationCompleteCallback;

  // All type of jobs (Register and Unregister) complete through a
  // single call to this callback on the IO thread.
  ServiceWorkerRegisterJob(const base::WeakPtr<ServiceWorkerStorage>& storage,
                           const RegistrationCompleteCallback& callback);
  ~ServiceWorkerRegisterJob();

  // The Registration flow includes most or all of the following,
  // depending on what is already registered:
  //  - creating a ServiceWorkerRegistration instance if there isn't
  //    already something registered
  //  - creating a ServiceWorkerVersion for the new registration instance.
  //  - starting a worker for the ServiceWorkerVersion
  //  - telling the Version to evaluate the script
  //  - firing the 'install' event at the ServiceWorkerVersion
  //  - firing the 'activate' event at the ServiceWorkerVersion
  //  - Waiting for older ServiceWorkerVersions to deactivate
  //  - designating the new version to be the 'active' version
  // This method should be called once and only once per job.
  void StartRegister(const GURL& pattern, const GURL& script_url);

  // The Unregistration process is primarily cleanup, removing
  // everything that was created during the Registration process,
  // including the ServiceWorkerRegistration itself.
  // This method should be called once and only once per job.
  void StartUnregister(const GURL& pattern);

 private:
  // These are all steps in the registration and unregistration pipeline.
  void RegisterPatternAndContinue(
      const GURL& pattern,
      const GURL& script_url,
      const ServiceWorkerStorage::RegistrationCallback& callback,
      ServiceWorkerRegistrationStatus previous_status);

  void UnregisterPatternAndContinue(
      const GURL& pattern,
      const GURL& script_url,
      const ServiceWorkerStorage::UnregistrationCallback& callback,
      bool found,
      ServiceWorkerRegistrationStatus previous_status,
      const scoped_refptr<ServiceWorkerRegistration>& previous_registration);

  // These methods are the last internal callback in the callback
  // chain, and ultimately call callback_.
  void UnregisterComplete(ServiceWorkerRegistrationStatus status);
  void RegisterComplete(
      ServiceWorkerRegistrationStatus status,
      const scoped_refptr<ServiceWorkerRegistration>& registration);

  const base::WeakPtr<ServiceWorkerStorage> storage_;
  const RegistrationCompleteCallback callback_;
  base::WeakPtrFactory<ServiceWorkerRegisterJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRegisterJob);
};
}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTER_JOB_H_
