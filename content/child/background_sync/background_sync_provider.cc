// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/background_sync/background_sync_provider.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "content/child/background_sync/background_sync_type_converters.h"
#include "content/child/service_worker/web_service_worker_registration_impl.h"
#include "content/child/worker_task_runner.h"
#include "content/public/common/service_registry.h"
#include "third_party/WebKit/public/platform/modules/background_sync/WebSyncError.h"
#include "third_party/WebKit/public/platform/modules/background_sync/WebSyncRegistration.h"

namespace content {
namespace {

// Returns the id of the given |service_worker_registration|, which
// is only available on the implementation of the interface.
int64 GetServiceWorkerRegistrationId(
    blink::WebServiceWorkerRegistration* service_worker_registration) {
  return static_cast<WebServiceWorkerRegistrationImpl*>(
             service_worker_registration)->registration_id();
}

}  // namespace

BackgroundSyncProvider::BackgroundSyncProvider(
    ServiceRegistry* service_registry)
    : service_registry_(service_registry) {
  DCHECK(service_registry);
}

BackgroundSyncProvider::~BackgroundSyncProvider() {
}

void BackgroundSyncProvider::registerBackgroundSync(
    const blink::WebSyncRegistration* options,
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebSyncRegistrationCallbacks* callbacks) {
  DCHECK(options);
  DCHECK(service_worker_registration);
  DCHECK(callbacks);
  int64 service_worker_registration_id =
      GetServiceWorkerRegistrationId(service_worker_registration);
  scoped_ptr<const blink::WebSyncRegistration> optionsPtr(options);
  scoped_ptr<blink::WebSyncRegistrationCallbacks> callbacksPtr(callbacks);

  // base::Unretained is safe here, as the mojo channel will be deleted (and
  // will wipe its callbacks) before 'this' is deleted.
  GetBackgroundSyncServicePtr()->Register(
      mojo::ConvertTo<SyncRegistrationPtr>(*(optionsPtr.get())),
      service_worker_registration_id,
      base::Bind(&BackgroundSyncProvider::RegisterCallback,
                 base::Unretained(this), base::Passed(callbacksPtr.Pass())));
}

void BackgroundSyncProvider::unregisterBackgroundSync(
    blink::WebSyncRegistration::Periodicity periodicity,
    int64_t id,
    const blink::WebString& tag,
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebSyncUnregistrationCallbacks* callbacks) {
  DCHECK(service_worker_registration);
  DCHECK(callbacks);
  int64 service_worker_registration_id =
      GetServiceWorkerRegistrationId(service_worker_registration);
  scoped_ptr<blink::WebSyncUnregistrationCallbacks> callbacksPtr(callbacks);

  // base::Unretained is safe here, as the mojo channel will be deleted (and
  // will wipe its callbacks) before 'this' is deleted.
  GetBackgroundSyncServicePtr()->Unregister(
      mojo::ConvertTo<BackgroundSyncPeriodicity>(periodicity), id, tag.utf8(),
      service_worker_registration_id,
      base::Bind(&BackgroundSyncProvider::UnregisterCallback,
                 base::Unretained(this), base::Passed(callbacksPtr.Pass())));
}

void BackgroundSyncProvider::getRegistration(
    blink::WebSyncRegistration::Periodicity periodicity,
    const blink::WebString& tag,
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebSyncRegistrationCallbacks* callbacks) {
  DCHECK(service_worker_registration);
  DCHECK(callbacks);
  int64 service_worker_registration_id =
      GetServiceWorkerRegistrationId(service_worker_registration);
  scoped_ptr<blink::WebSyncRegistrationCallbacks> callbacksPtr(callbacks);

  // base::Unretained is safe here, as the mojo channel will be deleted (and
  // will wipe its callbacks) before 'this' is deleted.
  GetBackgroundSyncServicePtr()->GetRegistration(
      mojo::ConvertTo<BackgroundSyncPeriodicity>(periodicity), tag.utf8(),
      service_worker_registration_id,
      base::Bind(&BackgroundSyncProvider::RegisterCallback,
                 base::Unretained(this), base::Passed(callbacksPtr.Pass())));
}

void BackgroundSyncProvider::getRegistrations(
    blink::WebSyncRegistration::Periodicity periodicity,
    blink::WebServiceWorkerRegistration* service_worker_registration,
    blink::WebSyncGetRegistrationsCallbacks* callbacks) {
  DCHECK(service_worker_registration);
  DCHECK(callbacks);
  int64 service_worker_registration_id =
      GetServiceWorkerRegistrationId(service_worker_registration);
  scoped_ptr<blink::WebSyncGetRegistrationsCallbacks> callbacksPtr(callbacks);

  // base::Unretained is safe here, as the mojo channel will be deleted (and
  // will wipe its callbacks) before 'this' is deleted.
  GetBackgroundSyncServicePtr()->GetRegistrations(
      mojo::ConvertTo<BackgroundSyncPeriodicity>(periodicity),
      service_worker_registration_id,
      base::Bind(&BackgroundSyncProvider::GetRegistrationsCallback,
                 base::Unretained(this), base::Passed(callbacksPtr.Pass())));
}

void BackgroundSyncProvider::RegisterCallback(
    scoped_ptr<blink::WebSyncRegistrationCallbacks> callbacks,
    const SyncRegistrationPtr& options) {
  // TODO(iclelland): Pass through the various errors from the manager to here
  // and handle them appropriately.
  scoped_ptr<blink::WebSyncRegistration> result;
  if (!options.is_null())
    result = mojo::ConvertTo<scoped_ptr<blink::WebSyncRegistration>>(options);

  callbacks->onSuccess(result.release());
}

void BackgroundSyncProvider::UnregisterCallback(
    scoped_ptr<blink::WebSyncUnregistrationCallbacks> callbacks,
    bool success) {
  // TODO(iclelland): Pass through the various errors from the manager to here
  // and handle them appropriately.
  if (success) {
    callbacks->onSuccess(new bool(success));
  } else {
    callbacks->onError(
        new blink::WebSyncError(blink::WebSyncError::ErrorTypeUnknown,
                                "Sync registration does not exist"));
  }
}

void BackgroundSyncProvider::GetRegistrationsCallback(
    scoped_ptr<blink::WebSyncGetRegistrationsCallbacks> callbacks,
    const mojo::Array<SyncRegistrationPtr>& registrations) {
  // TODO(iclelland): Pass through the various errors from the manager to here
  // and handle them appropriately.
  blink::WebVector<blink::WebSyncRegistration*>* results =
      new blink::WebVector<blink::WebSyncRegistration*>(registrations.size());
  for (size_t i = 0; i < registrations.size(); ++i) {
    (*results)[i] = mojo::ConvertTo<scoped_ptr<blink::WebSyncRegistration>>(
                        registrations[i]).release();
  }
  callbacks->onSuccess(results);
}

BackgroundSyncServicePtr&
BackgroundSyncProvider::GetBackgroundSyncServicePtr() {
  if (!background_sync_service_.get())
    service_registry_->ConnectToRemoteService(&background_sync_service_);
  return background_sync_service_;
}

}  // namespace content
