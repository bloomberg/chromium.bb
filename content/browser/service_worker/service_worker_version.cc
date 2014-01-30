// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_version.h"

#include "base/stl_util.h"
#include "content/browser/service_worker/embedded_worker_instance.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/common/service_worker/service_worker_messages.h"

namespace content {

namespace {

void RunSoon(const base::Closure& callback) {
  base::MessageLoop::current()->PostTask(FROM_HERE, callback);
}

}  // namespace

//-------------------------------------------------------------------
class ServiceWorkerVersion::WorkerObserverBase
    : public EmbeddedWorkerInstance::Observer {
 public:
  virtual ~WorkerObserverBase() {
    version_->embedded_worker_->RemoveObserver(this);
  }

  virtual void OnStarted() OVERRIDE { NOTREACHED(); }
  virtual void OnStopped() OVERRIDE { NOTREACHED(); }
  virtual void OnMessageReceived(const IPC::Message& message) OVERRIDE {
    NOTREACHED();
  }

 protected:
  explicit WorkerObserverBase(ServiceWorkerVersion* version)
      : version_(version) {
    version_->embedded_worker_->AddObserver(this);
  }

  ServiceWorkerVersion* version() { return version_; }

 private:
  ServiceWorkerVersion* version_;
};


// Observer class that is attached while the worker is starting.
class ServiceWorkerVersion::StartObserver : public WorkerObserverBase {
 public:
  typedef ServiceWorkerVersion::StatusCallback StatusCallback;

  StartObserver(ServiceWorkerVersion* version, const StatusCallback& callback)
      : WorkerObserverBase(version),
        callback_(callback) {}
  virtual ~StartObserver() {}

  virtual void OnStarted() OVERRIDE {
    Completed(SERVICE_WORKER_OK);
  }

  virtual void OnStopped() OVERRIDE {
    Completed(SERVICE_WORKER_ERROR_START_WORKER_FAILED);
  }

 private:
  void Completed(ServiceWorkerStatusCode status) {
    StatusCallback callback = callback_;
    version()->observer_.reset();
    callback.Run(status);
  }

  StatusCallback callback_;
  DISALLOW_COPY_AND_ASSIGN(StartObserver);
};

// Observer class that is attached while the worker is stopping.
class ServiceWorkerVersion::StopObserver : public WorkerObserverBase {
 public:
  typedef ServiceWorkerVersion::StatusCallback StatusCallback;
  StopObserver(ServiceWorkerVersion* version, const StatusCallback& callback)
      : WorkerObserverBase(version),
        callback_(callback) {}
  virtual ~StopObserver() {}

  virtual void OnStopped() OVERRIDE {
    StatusCallback callback = callback_;
    version()->observer_.reset();
    callback.Run(SERVICE_WORKER_OK);
  }

  virtual void OnMessageReceived(const IPC::Message& message) OVERRIDE {
    // We just ignore messages, as we're stopping.
  }

 private:
  StatusCallback callback_;
  DISALLOW_COPY_AND_ASSIGN(StopObserver);
};

//-------------------------------------------------------------------
ServiceWorkerVersion::ServiceWorkerVersion(
    ServiceWorkerRegistration* registration,
    EmbeddedWorkerRegistry* worker_registry,
    int64 version_id)
    : version_id_(version_id),
      is_shutdown_(false),
      registration_(registration) {
  if (worker_registry)
    embedded_worker_ = worker_registry->CreateWorker();
}

ServiceWorkerVersion::~ServiceWorkerVersion() { DCHECK(is_shutdown_); }

void ServiceWorkerVersion::Shutdown() {
  is_shutdown_ = true;
  registration_ = NULL;
  embedded_worker_.reset();
}

void ServiceWorkerVersion::StartWorker(const StatusCallback& callback) {
  DCHECK(!is_shutdown_);
  DCHECK(registration_);
  DCHECK(!observer_);
  if (status() == RUNNING) {
    RunSoon(base::Bind(callback, SERVICE_WORKER_OK));
    return;
  }
  observer_.reset(new StartObserver(this, callback));
  ServiceWorkerStatusCode status = embedded_worker_->Start(
      version_id_,
      registration_->script_url());
  if (status != SERVICE_WORKER_OK) {
    observer_.reset();
    RunSoon(base::Bind(callback, status));
  }
}

void ServiceWorkerVersion::StopWorker(const StatusCallback& callback) {
  DCHECK(!is_shutdown_);
  DCHECK(!observer_);
  if (status() == STOPPED) {
    RunSoon(base::Bind(callback, SERVICE_WORKER_OK));
    return;
  }
  observer_.reset(new StopObserver(this, callback));
  ServiceWorkerStatusCode status = embedded_worker_->Stop();
  if (status != SERVICE_WORKER_OK) {
    observer_.reset();
    RunSoon(base::Bind(callback, status));
  }
}

bool ServiceWorkerVersion::DispatchFetchEvent(
    const ServiceWorkerFetchRequest& request) {
  if (status() != RUNNING)
    return false;
  return embedded_worker_->SendMessage(
      ServiceWorkerMsg_FetchEvent(request)) == SERVICE_WORKER_OK;
}

void ServiceWorkerVersion::AddProcessToWorker(int process_id) {
  DCHECK(!is_shutdown_);
  embedded_worker_->AddProcessReference(process_id);
}

void ServiceWorkerVersion::RemoveProcessToWorker(int process_id) {
  embedded_worker_->ReleaseProcessReference(process_id);
}

}  // namespace content
