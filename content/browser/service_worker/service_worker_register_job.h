// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTER_JOB_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTER_JOB_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_registration_status.h"
#include "content/browser/service_worker/service_worker_storage.h"
#include "content/browser/service_worker/service_worker_version.h"

namespace content {

class EmbeddedWorkerRegistry;
class ServiceWorkerJobCoordinator;

// A ServiceWorkerRegisterJob lives only for the lifetime of a single
// registration or unregistration.
class ServiceWorkerRegisterJob {
 public:
  enum RegistrationType {
    REGISTER,
    UNREGISTER,
  };

  typedef base::Callback<void(ServiceWorkerStatusCode status)>
      StatusCallback;

  typedef base::Callback<
      void(ServiceWorkerStatusCode status,
           const scoped_refptr<ServiceWorkerRegistration>& registration)>
      RegistrationCallback;
  typedef StatusCallback UnregistrationCallback;

  // All type of jobs (Register and Unregister) complete through a
  // single call to this callback on the IO thread.
  ServiceWorkerRegisterJob(ServiceWorkerStorage* storage,
                           EmbeddedWorkerRegistry* worker_registry,
                           ServiceWorkerJobCoordinator* coordinator,
                           const GURL& pattern,
                           const GURL& script_url,
                           RegistrationType type);
  ~ServiceWorkerRegisterJob();

  void AddCallback(const RegistrationCallback& callback, int process_id);

  void Start();

  bool Equals(ServiceWorkerRegisterJob* job);

 private:
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
  void StartRegister();

  // The Unregistration process is primarily cleanup, removing
  // everything that was created during the Registration process,
  // including the ServiceWorkerRegistration itself.
  // This method should be called once and only once per job.
  void StartUnregister();

  // These are all steps in the registration and unregistration pipeline.
  void RegisterPatternAndContinue(
      const StatusCallback& callback,
      ServiceWorkerStatusCode previous_status);

  void UnregisterPatternAndContinue(
      const StatusCallback& callback,
      ServiceWorkerStatusCode previous_status,
      const scoped_refptr<ServiceWorkerRegistration>& previous_registration);

  void StartWorkerAndContinue(
      const StatusCallback& callback,
      ServiceWorkerStatusCode previous_status);

  // This method is the last internal callback in the callback
  // chain, and ultimately call callback_.
  void Complete(ServiceWorkerStatusCode status);

  // The ServiceWorkerStorage object should always outlive
  // this.

  // TODO(alecflett) When we support job cancelling, if we are keeping
  // this job alive for any reason, be sure to clear this variable,
  // because we may be cancelling while there are outstanding
  // callbacks that expect access to storage_.
  ServiceWorkerStorage* storage_;
  EmbeddedWorkerRegistry* worker_registry_;
  ServiceWorkerJobCoordinator* coordinator_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> pending_version_;
  const GURL pattern_;
  const GURL script_url_;
  const RegistrationType type_;
  std::vector<RegistrationCallback> callbacks_;
  std::vector<int> pending_process_ids_;
  base::WeakPtrFactory<ServiceWorkerRegisterJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRegisterJob);
};
}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTER_JOB_H_
