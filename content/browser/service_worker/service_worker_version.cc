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

typedef ServiceWorkerVersion::StatusCallback StatusCallback;
typedef ServiceWorkerVersion::MessageCallback MessageCallback;

namespace {

void RunSoon(const base::Closure& callback) {
  if (!callback.is_null())
    base::MessageLoop::current()->PostTask(FROM_HERE, callback);
}

template <typename CallbackArray, typename Arg>
void RunCallbacks(const CallbackArray& callbacks, const Arg& arg) {
  for (typename CallbackArray::const_iterator i = callbacks.begin();
       i != callbacks.end(); ++i)
    (*i).Run(arg);
}

// A callback adapter to start a |task| after StartWorker.
void RunTaskAfterStartWorker(
    base::WeakPtr<ServiceWorkerVersion> version,
    const StatusCallback& error_callback,
    const base::Closure& task,
    ServiceWorkerStatusCode status) {
  if (status != SERVICE_WORKER_OK) {
    if (!error_callback.is_null())
      error_callback.Run(status);
    return;
  }
  if (version->status() != ServiceWorkerVersion::RUNNING) {
    // We've tried to start the worker (and it has succeeded), but
    // it looks it's not running yet.
    NOTREACHED() << "The worker's not running after successful StartWorker";
    if (!error_callback.is_null())
      error_callback.Run(SERVICE_WORKER_ERROR_START_WORKER_FAILED);
    return;
  }
  task.Run();
}

void RunEmptyMessageCallback(const MessageCallback& callback,
                             ServiceWorkerStatusCode status) {
  callback.Run(status, IPC::Message());
}

void HandleInstallFinished(const StatusCallback& callback,
                           ServiceWorkerStatusCode status,
                           const IPC::Message& message) {
  if (status != SERVICE_WORKER_OK) {
    callback.Run(status);
    return;
  }

  if (message.type() != ServiceWorkerHostMsg_InstallEventFinished::ID) {
    NOTREACHED() << "Got unexpected response for InstallEvent: "
                 << message.type();
    callback.Run(SERVICE_WORKER_ERROR_FAILED);
    return;
  }
  callback.Run(SERVICE_WORKER_OK);
}

void HandleFetchResponse(const ServiceWorkerVersion::FetchCallback& callback,
                         ServiceWorkerStatusCode status,
                         const IPC::Message& message) {
  Tuple1<ServiceWorkerFetchResponse> response;
  if (message.type() != ServiceWorkerHostMsg_FetchEventFinished::ID) {
    NOTREACHED() << "Got unexpected response for FetchEvent: "
                 << message.type();
    callback.Run(SERVICE_WORKER_ERROR_FAILED, response.a);
    return;
  }
  ServiceWorkerHostMsg_FetchEventFinished::Read(&message, &response);
  callback.Run(status, response.a);
}

}  // namespace

ServiceWorkerVersion::ServiceWorkerVersion(
    ServiceWorkerRegistration* registration,
    EmbeddedWorkerRegistry* worker_registry,
    int64 version_id)
    : version_id_(version_id),
      is_shutdown_(false),
      registration_(registration),
      weak_factory_(this) {
  if (worker_registry) {
    embedded_worker_ = worker_registry->CreateWorker();
    embedded_worker_->AddObserver(this);
  }
}

ServiceWorkerVersion::~ServiceWorkerVersion() { DCHECK(is_shutdown_); }

void ServiceWorkerVersion::Shutdown() {
  is_shutdown_ = true;
  registration_ = NULL;
  if (embedded_worker_) {
    embedded_worker_->RemoveObserver(this);
    embedded_worker_.reset();
  }
}

void ServiceWorkerVersion::StartWorker(const StatusCallback& callback) {
  DCHECK(!is_shutdown_);
  DCHECK(embedded_worker_);
  DCHECK(registration_);
  if (status() == RUNNING) {
    RunSoon(base::Bind(callback, SERVICE_WORKER_OK));
    return;
  }
  if (status() == STOPPING) {
    RunSoon(base::Bind(callback, SERVICE_WORKER_ERROR_START_WORKER_FAILED));
    return;
  }
  if (start_callbacks_.empty()) {
    ServiceWorkerStatusCode status = embedded_worker_->Start(
        version_id_,
        registration_->script_url());
    if (status != SERVICE_WORKER_OK) {
      RunSoon(base::Bind(callback, status));
      return;
    }
  }
  start_callbacks_.push_back(callback);
}

void ServiceWorkerVersion::StopWorker(const StatusCallback& callback) {
  DCHECK(!is_shutdown_);
  DCHECK(embedded_worker_);
  if (status() == STOPPED) {
    RunSoon(base::Bind(callback, SERVICE_WORKER_OK));
    return;
  }
  if (stop_callbacks_.empty()) {
    ServiceWorkerStatusCode status = embedded_worker_->Stop();
    if (status != SERVICE_WORKER_OK) {
      RunSoon(base::Bind(callback, status));
      return;
    }
  }
  stop_callbacks_.push_back(callback);
}

void ServiceWorkerVersion::SendMessage(
    const IPC::Message& message, const StatusCallback& callback) {
  DCHECK(!is_shutdown_);
  DCHECK(embedded_worker_);
  if (status() != RUNNING) {
    // Schedule calling this method after starting the worker.
    StartWorker(base::Bind(&RunTaskAfterStartWorker,
                           weak_factory_.GetWeakPtr(), callback,
                           base::Bind(&self::SendMessage,
                                      weak_factory_.GetWeakPtr(),
                                      message, callback)));
    return;
  }

  ServiceWorkerStatusCode status = embedded_worker_->SendMessage(
      kInvalidRequestId, message);
  RunSoon(base::Bind(callback, status));
}

void ServiceWorkerVersion::SendMessageAndRegisterCallback(
    const IPC::Message& message, const MessageCallback& callback) {
  DCHECK(!is_shutdown_);
  DCHECK(embedded_worker_);
  if (status() != RUNNING) {
    // Schedule calling this method after starting the worker.
    StartWorker(base::Bind(&RunTaskAfterStartWorker,
                           weak_factory_.GetWeakPtr(),
                           base::Bind(&RunEmptyMessageCallback, callback),
                           base::Bind(&self::SendMessageAndRegisterCallback,
                                      weak_factory_.GetWeakPtr(),
                                      message, callback)));
    return;
  }

  int request_id = message_callbacks_.Add(new MessageCallback(callback));
  ServiceWorkerStatusCode status =
      embedded_worker_->SendMessage(request_id, message);
  if (status != SERVICE_WORKER_OK) {
    message_callbacks_.Remove(request_id);
    RunSoon(base::Bind(callback, status, IPC::Message()));
    return;
  }
}

void ServiceWorkerVersion::DispatchInstallEvent(
    int active_version_embedded_worker_id,
    const StatusCallback& callback) {
  SendMessageAndRegisterCallback(
      ServiceWorkerMsg_InstallEvent(active_version_embedded_worker_id),
      base::Bind(&HandleInstallFinished, callback));
}

void ServiceWorkerVersion::DispatchFetchEvent(
    const ServiceWorkerFetchRequest& request,
    const FetchCallback& callback) {
  SendMessageAndRegisterCallback(
      ServiceWorkerMsg_FetchEvent(request),
      base::Bind(&HandleFetchResponse, callback));
}

void ServiceWorkerVersion::AddProcessToWorker(int process_id) {
  DCHECK(!is_shutdown_);
  embedded_worker_->AddProcessReference(process_id);
}

void ServiceWorkerVersion::RemoveProcessToWorker(int process_id) {
  embedded_worker_->ReleaseProcessReference(process_id);
}

void ServiceWorkerVersion::OnStarted() {
  DCHECK_EQ(RUNNING, status());
  // Fire all start callbacks.
  RunCallbacks(start_callbacks_, SERVICE_WORKER_OK);
  start_callbacks_.clear();
}

void ServiceWorkerVersion::OnStopped() {
  DCHECK_EQ(STOPPED, status());
  // Fire all stop callbacks.
  RunCallbacks(stop_callbacks_, SERVICE_WORKER_OK);
  stop_callbacks_.clear();

  // Let all start callbacks fail.
  RunCallbacks(start_callbacks_, SERVICE_WORKER_ERROR_START_WORKER_FAILED);
  start_callbacks_.clear();

  // Let all message callbacks fail.
  // TODO(kinuko): Consider if we want to add queue+resend mechanism here.
  IDMap<MessageCallback, IDMapOwnPointer>::iterator iter(&message_callbacks_);
  while (!iter.IsAtEnd()) {
    iter.GetCurrentValue()->Run(SERVICE_WORKER_ERROR_ABORT, IPC::Message());
    iter.Advance();
  }
  message_callbacks_.Clear();
}

void ServiceWorkerVersion::OnMessageReceived(
    int request_id, const IPC::Message& message) {
  MessageCallback* callback = message_callbacks_.Lookup(request_id);
  if (callback) {
    callback->Run(SERVICE_WORKER_OK, message);
    message_callbacks_.Remove(request_id);
    return;
  }
  NOTREACHED() << "Got unexpected message: " << request_id
               << " " << message.type();
}

}  // namespace content
