// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTER_JOB_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTER_JOB_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "content/browser/service_worker/service_worker_register_job_base.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "url/gurl.h"

namespace content {

class ServiceWorkerJobCoordinator;
class ServiceWorkerStorage;

// Handles the registration of a Service Worker.
//
// The registration flow includes most or all of the following,
// depending on what is already registered:
//  - creating a ServiceWorkerRegistration instance if there isn't
//    already something registered
//  - creating a ServiceWorkerVersion for the new registration instance.
//  - starting a worker for the ServiceWorkerVersion
//  - telling the Version to evaluate the script
//  - firing the 'install' event at the ServiceWorkerVersion
//  - firing the 'activate' event at the ServiceWorkerVersion
//  - waiting for older ServiceWorkerVersions to deactivate
//  - designating the new version to be the 'active' version
class ServiceWorkerRegisterJob : public ServiceWorkerRegisterJobBase {
 public:
  typedef base::Callback<void(ServiceWorkerStatusCode status,
                              const scoped_refptr<ServiceWorkerRegistration>&
                                  registration)> RegistrationCallback;

  ServiceWorkerRegisterJob(base::WeakPtr<ServiceWorkerContextCore> context,
                           const GURL& pattern,
                           const GURL& script_url);
  virtual ~ServiceWorkerRegisterJob();

  // Registers a callback to be called when the job completes (whether
  // successfully or not). Multiple callbacks may be registered. |process_id| is
  // added via AddProcessToWorker to the ServiceWorkerVersion created by the
  // registration job.
  void AddCallback(const RegistrationCallback& callback, int process_id);

  // ServiceWorkerRegisterJobBase implementation:
  virtual void Start() OVERRIDE;
  virtual bool Equals(ServiceWorkerRegisterJobBase* job) OVERRIDE;
  virtual RegistrationJobType GetType() OVERRIDE;

 private:
  void HandleExistingRegistrationAndContinue(
      ServiceWorkerStatusCode status,
      const scoped_refptr<ServiceWorkerRegistration>& registration);
  void RegisterAndContinue(ServiceWorkerStatusCode status);
  void UpdateAndContinue(ServiceWorkerStatusCode status);
  void OnStartWorkerFinished(ServiceWorkerStatusCode status);
  void InstallAndContinue();
  void OnInstallFinished(ServiceWorkerStatusCode status);
  void ActivateAndContinue();
  void Complete(ServiceWorkerStatusCode status);

  void RunCallbacks(ServiceWorkerStatusCode status);

  // The ServiceWorkerStorage object should always outlive this.
  base::WeakPtr<ServiceWorkerContextCore> context_;
  scoped_refptr<ServiceWorkerRegistration> registration_;
  scoped_refptr<ServiceWorkerVersion> pending_version_;
  const GURL pattern_;
  const GURL script_url_;
  std::vector<RegistrationCallback> callbacks_;
  std::vector<int> pending_process_ids_;
  base::WeakPtrFactory<ServiceWorkerRegisterJob> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerRegisterJob);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTER_JOB_H_
